#define _POSIX_C_SOURCE 200809L

#include "server.h"
#include "server_p.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "http.h"
#include "http_request.h"
#include "http_response.h"
#include "http_server.h"
#include "db.h"

#define MAX_EVENTS 10
#define NUM_DB_WORKER_THREADS 1

static int
set_nonblocking(int sockfd)
{
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1)
  {
    perror("fcntl");
    return -1;
  }
  if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
  {
    perror("fcntl");
    return -1;
  }
  return 0;
}

server_t *
create_server(int port, int max_connections)
{
  server_t *srv = malloc(sizeof(server_t));
  if (!srv)
  {
    perror("malloc");
    return NULL;
  }

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0)
  {
    perror("socket");
    return NULL;
  }

  int optval = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    perror("setsocketopt");
    return NULL;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("bind");
    return NULL;
  }

  if (listen(listen_fd, max_connections) < 0)
  {
    perror("listen");
    return NULL;
  }

  if (set_nonblocking(listen_fd) < 0)
  {
    perror("set_nonblocking");
    return NULL;
  }

  connection_t listen_conn = {
      .fd = listen_fd,
      .type = FD_TYPE_LISTEN};

  int epoll_fd = epoll_create1(0);
  if (srv->epoll_fd < 0)
  {
    perror("epoll_create1");
    return NULL;
  }

  srv->listen_conn = listen_conn;
  srv->epoll_fd = epoll_fd;
  srv->port = port;

  setup_shutdown(srv);

  srv->http_server = malloc(sizeof(http_server_t));
  memset(srv->http_server, 0, sizeof(http_server_t));

  srv->db_pool = db_pool_new_from_env(srv->epoll_fd, NUM_DB_WORKER_THREADS);
  if (!srv->db_pool)
  {
    return NULL;
  }

  return srv;
}

void
serve(server_t *srv)
{
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.ptr = &srv->listen_conn;

  if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, srv->listen_conn.fd, &event) < 0)
  {
    perror("epoll_ctl");
    return;
  }

  struct epoll_event events[MAX_EVENTS];
  printf("Server listening on port %d\n", srv->port);

  int is_running = 1;

  while (is_running)
  {
    int n_fds = epoll_wait(srv->epoll_fd, events, MAX_EVENTS, -1);
    if (n_fds < 0)
    {
      if (errno == EINTR)
      {
        continue; // Interrupted by signal, retry
      }
      perror("epoll_wait");
      return;
    }

    for (int i = 0; i < n_fds; i++)
    {
      connection_t *conn = (connection_t *)events[i].data.ptr;

      // handle async writev
      if (events[i].events & EPOLLOUT)
      {
        int res = connection_send_buffer(conn);
        if (res != 0)
        {
          remove_connection(srv, conn);
        }
      }

      switch (conn->type)
      {
      case FD_TYPE_LISTEN:
        listen_handler(srv);
        break;

      case FD_TYPE_CLIENT:
        client_handler(srv, conn);
        break;

      case FD_TYPE_SIGNAL:
        printf("graceful shutdown...\n");
        is_running = 0;
        break;

      case FD_TYPE_DB:
        db_handler(srv, conn);
        break;
      }
    }
  }
}

void
destroy_server(server_t *srv)
{
  if (!srv)
  {
    return;
  }

  if (srv->epoll_fd >= 0)
  {
    close(srv->epoll_fd);
    srv->epoll_fd = -1;
  }

  if (srv->listen_conn.fd >= 0)
  {
    close(srv->listen_conn.fd);
    srv->listen_conn.fd = -1;
  }

  if (srv->http_server)
  {
    free(srv->http_server);
    srv->http_server = NULL;
  }

  free(srv);

  printf("[Server] shutdown completed.\n");
}

void
setup_shutdown(server_t *srv)
{
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &mask, NULL);

  connection_t *conn = malloc(sizeof(connection_t));
  conn->fd = signalfd(-1, &mask, SFD_NONBLOCK);
  conn->type = FD_TYPE_SIGNAL;

  add_connection(srv, conn, EPOLLIN);
}

void
listen_handler(const server_t *srv)
{
  while (1)
  {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(srv->listen_conn.fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // No more incoming connections
        break;
      }
      perror("accept");
      return;
    }

    if (set_nonblocking(client_fd) < 0)
    {
      perror("set_nonblocking");
      close(client_fd);
      continue;
    }

    connection_t *client_conn = calloc(1, sizeof(connection_t));
    if (!client_conn)
    {
      perror("malloc");
      close(client_fd);
      continue;
    }
    client_conn->fd = client_fd;
    client_conn->type = FD_TYPE_CLIENT;

    if (add_connection(srv, client_conn, EPOLLIN | EPOLLET) < 0) // Edge-triggered
    {
      close(client_fd);
      free(client_conn);
      continue;
    }

    printf("Accepted connection on fd %d\n", client_fd);
  }
}

void
client_handler(const server_t *srv, connection_t *conn)
{
  http_request_t *req = malloc(sizeof(http_request_t));

  http_response_t response = parse_http_request(conn, req);

  fprintf(stderr, "[HTTP Request] version: %s, method: %s, uri: ",
          http_version_to_string(req->version), http_method_to_string(req->method));
  fwrite(req->uri, sizeof(char), req->uri_len, stderr);
  fprintf(stderr, "\n");

  http_request_register_dispose(conn, req);

  if (!is_error_status(response.status))
  {
    int is_complete = 0;
    response = http_server_handle_request(srv->http_server, req, srv->db_pool, &is_complete);

    if (!is_complete)
    {
      return;
    }
  }

  start_send_http_response(srv, conn, response);
}

void
db_handler(const server_t *srv, connection_t *conn)
{
  db_task_t *task = db_pool_get_latest_completed_task(srv->db_pool);

  fprintf(stderr, "[DEBUG] db response: %*.s", (int)task->result_len, task->result_body);

  http_server_request_context_t *ctx = (http_server_request_context_t *)task->data;
  http_response_t response = ctx->handler_await(ctx->request, task);

  start_send_http_response(srv, ctx->request->conn, response);
}

int
add_connection(const server_t *server, connection_t *conn, uint32_t event_mask)
{
  struct epoll_event event;
  event.events = event_mask;
  event.data.ptr = conn;

  if (epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, conn->fd, &event) < 0)
  {
    perror("epoll_ctl");
    return -1;
  }

  return 0;
}

void
remove_connection(const server_t *server, connection_t *conn)
{
  epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
  close(conn->fd);

  for (int i = 0; i < conn->owned_count; i++)
  {
    if (conn->owned_ptr[i])
    {
      free(conn->owned_ptr[i]);
    }
  }
  free(conn);
}

// return true if send all
static bool
advance_iovec(connection_t *conn, size_t send_bytes)
{
  while (conn->iov_index < conn->iov_count && send_bytes > 0)
  {
    struct iovec *cur = &conn->iov[conn->iov_index];

    if (send_bytes >= cur->iov_len)
    {
      send_bytes -= cur->iov_len;
      conn->iov_index++;
    }
    else
    {
      cur->iov_base = (char *)cur->iov_base + send_bytes;
      cur->iov_len -= send_bytes;
      send_bytes = 0;
    }
  }

  return conn->iov_index >= conn->iov_count;
}

int
connection_send_buffer(connection_t *conn)
{
  while (conn->iov_index < conn->iov_count)
  {
    struct iovec *cur = &conn->iov[conn->iov_index];
    int cur_count = conn->iov_count - conn->iov_index;

    ssize_t n = writev(conn->fd, cur, cur_count);

    if (n > 0)
    {
      if (advance_iovec(conn, (size_t)n))
      {
        return 1;
      }
    }
    else if (n < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        return 0;
      }
      return -1;
    }
    else
    {
      return 0;
    }
  }

  return 1;
}

void start_send_http_response(const server_t *server, connection_t *conn, http_response_t response)
{
  char *header_buf;
  size_t header_buf_len;
  error err = http_response_build_header(&response, &header_buf, &header_buf_len);
  if (err.code != ERR_NONE)
  {
    http_response_internal_server_error(&header_buf, &header_buf_len);
  }

  printf("[HTTP Response] status: %d, header_len: %zu, body_len: %zu\n", response.status, header_buf_len, response.body_len);

  conn->iov[0].iov_base = header_buf;
  conn->iov[0].iov_len = header_buf_len;
  conn->iov_count = 1;
  conn->iov_index = 0;
  if (response.body_len > 0 && response.body != NULL)
  {
    conn->iov[1].iov_base = (char *)response.body;
    conn->iov[1].iov_len = response.body_len;
    conn->iov_count = 2;
  }

  int res = connection_send_buffer(conn);
  if (res == 0)
  {
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.ptr = conn;

    epoll_ctl(conn->fd, EPOLL_CTL_MOD, conn->fd, &event);
    return;
  }

  remove_connection(server, conn);
}