#include "mysql_result_mock.h"
#include "test.h"
#include "util.h"

#include <errno.h>
#include <http_server.h>
#include <server_p.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

// Simulate writes without HTTP communication or sockets.
static int write_mode;
static int write_calls;

ssize_t
__wrap_writev(int fd, const struct iovec *iov, int count)
{
  write_calls++;

  if (write_mode)
  {
    errno = write_mode == 1 ? EAGAIN : EPIPE;
    return -1;
  }

  size_t length = 0;

  for (int i = 0; i < count; i++)
  {
    length += iov[i].iov_len;
  }

  return length;
}

static int app_disposals;
static int auth_disposals;
static int handler_calls;

static void
dispose_app(void *data)
{
  app_disposals++;
  free(data);
}

static void
dispose_auth(void *data)
{
  auth_disposals++;
  free(data);
}

static bool
finish_request(http_request_context_t *context, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  handler_calls++;
  db_task_free(task);
  *response = (http_response_t){.status = HTTP_STATUS_OK, .body = context->request->app_data, .body_len = 1};
  return true;
}

static bool
queue_request(http_request_context_t *context, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  handler_calls++;
  db_task_free(task);
  context->current_handler = context->current_handler->next;
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  return db_pool_exec_query(db, "SELECT 1", 8, context) < 0;
}

static http_request_t *
new_request(server_t *server)
{
  connection_t *connection = calloc(1, sizeof(*connection));
  connection->fd = eventfd(0, EFD_NONBLOCK);
  connection->type = FD_TYPE_CLIENT;
  connection->next = server->clients;

  if (server->clients)
  {
    server->clients->previous = connection;
  }

  server->clients = connection;
  http_request_t *request = calloc(1, sizeof(*request));
  request->conn = connection;
  request->uri = "/";
  request->uri_len = 1;
  request->method = HTTP_METHOD_GET;
  request->app_data = malloc(1);
  *(char *)request->app_data = 'x';
  request->dispose_app_data = dispose_app;
  request->auth_data = malloc(1);
  request->dispose_auth_data = dispose_auth;
  connection->state.client.request = request;
  return request;
}

static void
complete_query(db_pool_t *pool)
{
  db_task_t *task = task_queue_pop(pool->task_queue);
  const char *rows[][6] = {{"1"}};
  task->result->success = 1;
  task->result->res = test_mysql_result(rows, 1, 1);
  task_queue_push(pool->done_queue, task);
  eventfd_write(pool->notify_fd, 1);
}

static void
test_request_lifetime(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool asynchronous;
    bool disconnect_first;
    bool second_query;
    bool context_allocation_failure;
    bool enqueue_failure;
    int send_failure;
    bool partial_write;
  } cases[] = {
      {.name = "synchronous response"},
      {.name = "async response", .asynchronous = true},
      {.name = "disconnect before completion", .asynchronous = true, .disconnect_first = true},
      {.name = "two query chain", .asynchronous = true, .second_query = true},
      {.name = "disconnect between queries", .asynchronous = true, .second_query = true, .disconnect_first = true},
      {.name = "context allocation fails", .context_allocation_failure = true},
      {.name = "enqueue fails", .asynchronous = true, .enqueue_failure = true},
      {.name = "partial write retains request", .asynchronous = true, .send_failure = 1, .partial_write = true},
      {.name = "write fails", .asynchronous = true, .send_failure = 2},
      {.name = "partial write registration fails", .asynchronous = true, .send_failure = 1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    app_disposals = auth_disposals = handler_calls = write_calls = 0;
    write_mode = cases[i].send_failure;
    db_pool_t pool = {.task_queue = task_queue_new(), .done_queue = task_queue_new(), .notify_fd = eventfd(0, EFD_NONBLOCK)};
    server_t server = {.epoll_fd = cases[i].partial_write ? epoll_create1(0) : -1, .db_pool = &pool};
    http_server_t http = {0};
    http_handler_t last = {.func = finish_request};
    http_handler_t middle = {.func = queue_request, .next = &last};
    http_handler_t first = {.func = queue_request, .next = cases[i].second_query ? &middle : &last};
    http_server_add_route(&http, HTTP_METHOD_GET, "/", cases[i].asynchronous ? &first : &last);
    http_request_t *request = new_request(&server);
    connection_t *connection = request->conn;

    if (cases[i].partial_write)
    {
      ASSERT_EQ(cases[i].name, 0, add_connection(&server, connection, EPOLLIN));
    }
    bool completed = false;
    test_set_malloc_failure(cases[i].context_allocation_failure ? 0 : -1);
    test_set_calloc_failure(cases[i].enqueue_failure ? 0 : -1);
    http_response_t response = http_server_handle_request(&http, request, &pool, &completed);
    test_set_malloc_failure(-1);
    test_set_calloc_failure(-1);
    ASSERT_EQ(cases[i].name, 0, app_disposals);
    ASSERT_EQ(cases[i].name, 0, auth_disposals);

    if (completed)
    {
      ASSERT_NULL(cases[i].name, request->context);
      ASSERT_EQ(cases[i].name,
                cases[i].context_allocation_failure || cases[i].enqueue_failure ? HTTP_STATUS_INTERNAL_SERVER_ERROR : HTTP_STATUS_OK,
                response.status);
      remove_connection(&server, connection);
    }
    else
    {
      if (cases[i].second_query)
      {
        complete_query(&pool);
        db_handler(&server, NULL);
        ASSERT_EQ(cases[i].name, 0, app_disposals);
        ASSERT_NOT_NULL(cases[i].name, request->context);
      }

      if (cases[i].disconnect_first)
      {
        remove_connection(&server, connection);
        // Reap immediately to expose any later use of the disconnected connection.
        reap_connections(&server);
        ASSERT_NULL(cases[i].name, request->conn);
        ASSERT_EQ(cases[i].name, 0, app_disposals);
      }

      complete_query(&pool);
      db_handler(&server, NULL);
      // A second notification with an empty queue must never block.
      db_handler(&server, NULL);
      ASSERT_EQ(cases[i].name, cases[i].disconnect_first ? 0 : 1, write_calls);

      if (cases[i].partial_write)
      {
        ASSERT_EQ(cases[i].name, 0, app_disposals);
        ASSERT_EQ(cases[i].name, 0, auth_disposals);
        ASSERT_NULL(cases[i].name, request->context);
        ASSERT_NOT_NULL(cases[i].name, connection->state.client.header_buffer);
        write_mode = 0;
        ASSERT_EQ(cases[i].name, 1, connection_send_buffer(connection));
        remove_connection(&server, connection);
      }
    }

    ASSERT_NULL(cases[i].name, server.clients);
    ASSERT_EQ(cases[i].name, 1, app_disposals);
    ASSERT_EQ(cases[i].name, 1, auth_disposals);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    reap_connections(&server);
    task_queue_free(pool.task_queue);
    task_queue_free(pool.done_queue);
    close(pool.notify_fd);

    if (server.epoll_fd >= 0)
    {
      close(server.epoll_fd);
    }
    CHECK_TEST(cases[i].name);
  }
}

static void
test_destroy_pending_server(test_ctx_t *ctx)
{
  ctx->is_canceled = false;
  app_disposals = auth_disposals = handler_calls = write_calls = 0;
  server_t *server = calloc(1, sizeof(*server));
  server->epoll_fd = server->listen_conn.fd = server->signal_conn.fd = -1;
  server->db_pool = calloc(1, sizeof(*server->db_pool));
  server->db_pool->epoll_fd = -1;
  server->db_pool->notify_fd = eventfd(0, EFD_NONBLOCK);
  server->db_pool->task_queue = task_queue_new();
  server->db_pool->done_queue = task_queue_new();
  http_server_t http = {0};
  http_handler_t last = {.func = finish_request};
  http_handler_t first = {.func = queue_request, .next = &last};
  http_server_add_route(&http, HTTP_METHOD_GET, "/", &first);

  for (int i = 0; i < 2; i++)
  {
    http_request_t *request = new_request(server);
    bool complete;
    http_server_handle_request(&http, request, server->db_pool, &complete);
    ASSERT_FALSE("shutdown pending", complete);
    complete_query(server->db_pool);
  }

  destroy_server(server);
  ASSERT_EQ("shutdown pending", 2, app_disposals);
  ASSERT_EQ("shutdown pending", 2, auth_disposals);
  ASSERT_EQ("shutdown pending", 2, handler_calls);
  ASSERT_EQ("shutdown pending", 0, write_calls);
  ASSERT_EQ("shutdown pending", 0, test_mysql_live_results);
  CHECK_TEST("shutdown pending");
}

void
test_server_lifetime(test_ctx_t *ctx)
{
  test_request_lifetime(ctx);
  test_destroy_pending_server(ctx);
}
