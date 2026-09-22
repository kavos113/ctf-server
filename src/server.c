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
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "db.h"
#include "app/password_worker.h"
#include "http.h"
#include "http_request.h"
#include "http_response.h"
#include "http_server.h"

#define MAX_EVENTS            10
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
  server_t *srv = calloc(1, sizeof(server_t));
  if (!srv)
  {
    perror("malloc");
    return NULL;
  }

  srv->epoll_fd = -1;
  srv->listen_conn.fd = -1;
  srv->signal_conn.fd = -1;
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  srv->listen_conn.fd = listen_fd;
  if (listen_fd < 0)
  {
    perror("socket");
    goto error;
  }

  int optval = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    perror("setsocketopt");
    goto error;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("bind");
    goto error;
  }

  if (listen(listen_fd, max_connections) < 0)
  {
    perror("listen");
    goto error;
  }

  if (set_nonblocking(listen_fd) < 0)
  {
    perror("set_nonblocking");
    goto error;
  }

  connection_t listen_conn = {
      .fd = listen_fd,
      .type = FD_TYPE_LISTEN};

  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0)
  {
    perror("epoll_create1");
    goto error;
  }

  srv->listen_conn = listen_conn;
  srv->epoll_fd = epoll_fd;
  srv->port = port;

  if (setup_shutdown(srv) < 0)
  {
    goto error;
  }

  srv->http_server = calloc(1, sizeof(http_server_t));

  if (!srv->http_server)
  {
    goto error;
  }

  srv->db_pool = db_pool_new_from_env(srv->epoll_fd, NUM_DB_WORKER_THREADS);
  if (!srv->db_pool)
  {
    goto error;
  }

  return srv;

error:
  destroy_server(srv);
  return NULL;
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

      if (conn->fd < 0)
      {
        continue;
      }

      if (conn->type == FD_TYPE_CLIENT)
      {
        if (events[i].events & (EPOLLERR | EPOLLHUP))
        {
          remove_connection(srv, conn);
          continue;
        }

        if (events[i].events & EPOLLOUT)
        {
          if (connection_send_buffer(conn) != 0)
          {
            remove_connection(srv, conn);
          }

          continue;
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

      case FD_TYPE_PASSWORD:
        password_worker_handler(srv, conn);
        break;

      case FD_TYPE_DB:
        db_handler(srv, conn);
        break;
      }

      if (!is_running)
      {
        break;
      }
    }

    reap_connections(srv);
  }
}

void
destroy_server(server_t *srv)
{
  if (!srv)
  {
    return;
  }

  while (srv->clients)
  {
    remove_connection(srv, srv->clients);
  }

  if (srv->password_worker)
  {
    password_worker_stop(srv->password_worker);
    password_worker_handler(srv, NULL);
    password_worker_free(srv->password_worker);
  }

  if (srv->db_pool)
  {
    db_pool_stop(srv->db_pool);
    db_handler(srv, NULL);
    db_pool_free(srv->db_pool);
  }

  reap_connections(srv);

  if (srv->signal_conn.fd >= 0)
  {
    close(srv->signal_conn.fd);
  }

  if (srv->listen_conn.fd >= 0)
  {
    close(srv->listen_conn.fd);
  }

  if (srv->epoll_fd >= 0)
  {
    close(srv->epoll_fd);
  }

  free(srv->http_server);
  free(srv);
}

int
setup_shutdown(server_t *srv)
{
  struct sigaction action = {.sa_handler = SIG_IGN};
  sigemptyset(&action.sa_mask);

  if (sigaction(SIGPIPE, &action, NULL) < 0)
  {
    return -1;
  }

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);

  if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
  {
    return -1;
  }

  srv->signal_conn.fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
  srv->signal_conn.type = FD_TYPE_SIGNAL;

  if (srv->signal_conn.fd < 0)
  {
    return -1;
  }

  return add_connection(srv, &srv->signal_conn, EPOLLIN);
}

void
listen_handler(server_t *srv)
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

    client_conn->next = srv->clients;

    if (srv->clients)
    {
      srv->clients->previous = client_conn;
    }

    srv->clients = client_conn;
    printf("Accepted connection on fd %d\n", client_fd);
  }
}

void
db_handler(server_t *srv, connection_t *conn)
{
  eventfd_t count;

  while (eventfd_read(srv->db_pool->notify_fd, &count) < 0 && errno == EINTR)
  {
  }

  db_task_t *task;

  while ((task = db_pool_get_latest_completed_task(srv->db_pool)))
  {
    http_request_context_t *ctx = task->data;

    if (!ctx->request->conn)
    {
      db_task_free(task);
      http_request_context_dispose(ctx);
      continue;
    }

    http_response_t response = {0};
    bool is_complete = ctx->current_handler->func(ctx, srv->db_pool, task, &response);

    if (is_complete)
    {
      start_send_http_response(srv, ctx->request->conn, response);
      http_request_context_dispose(ctx);
    }
  }
}

void
client_handler(server_t *srv, connection_t *conn)
{
  if (conn->state.client.request)
  {
    char byte;
    ssize_t n = recv(conn->fd, &byte, 1, MSG_PEEK);

    if (n == 0 || n > 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
    {
      remove_connection(srv, conn);
    }

    return;
  }

  http_request_t *req = malloc(sizeof(http_request_t));

  if (!req)
  {
    remove_connection(srv, conn);
    return;
  }
  conn->state.client.request = req;

  http_response_t response = parse_http_request(conn, req);

  fprintf(stderr, "[HTTP Request] version: %s, method: %s, uri: ",
          http_version_to_string(req->version), http_method_to_string(req->method));
  fwrite(req->uri, sizeof(char), req->uri_len, stderr);
  fprintf(stderr, "\n");

  if (!is_error_status(response.status))
  {
    bool is_complete = 0;
    response = http_server_handle_request(srv->http_server, req, srv->db_pool, &is_complete);

    if (!is_complete)
    {
      return;
    }
  }

  start_send_http_response(srv, conn, response);
}

// return true if send all
static bool
advance_iovec(client_connection_state_t *conn, size_t send_bytes)
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
  client_connection_state_t *client_state = &conn->state.client;

  while (client_state->iov_index < client_state->iov_count)
  {
    struct iovec *cur = &client_state->iov[client_state->iov_index];
    int cur_count = client_state->iov_count - client_state->iov_index;

    ssize_t n = writev(conn->fd, cur, cur_count);

    if (n > 0)
    {
      if (advance_iovec(client_state, (size_t)n))
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

void
start_send_http_response(server_t *server, connection_t *conn, http_response_t response)
{
  char *header_buf = NULL;
  size_t header_buf_len = 0;
  error err = http_response_build_header(&response, &header_buf, &header_buf_len);
  if (err.code != ERR_NONE)
  {
    remove_connection(server, conn);
    return;
  }

  printf("[HTTP Response] status: %d, header_len: %zu, body_len: %zu\n", response.status, header_buf_len, response.body_len);

  client_connection_state_t *client_state = &conn->state.client;

  client_state->header_buffer = header_buf;
  client_state->iov[0].iov_base = header_buf;
  client_state->iov[0].iov_len = header_buf_len;
  client_state->iov_count = 1;
  client_state->iov_index = 0;
  if (response.body_len > 0 && response.body != NULL)
  {
    client_state->iov[1].iov_base = (char *)response.body;
    client_state->iov[1].iov_len = response.body_len;
    client_state->iov_count = 2;
  }

  int res = connection_send_buffer(conn);
  if (res == 0)
  {
    struct epoll_event event;
    event.events = EPOLLOUT | EPOLLET;
    event.data.ptr = conn;

    if (epoll_ctl(server->epoll_fd, EPOLL_CTL_MOD, conn->fd, &event) < 0)
    {
      remove_connection(server, conn);
    }
    return;
  }

  remove_connection(server, conn);
}