#include "server.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>

#define MAX_EVENTS 10

static const char RESPONSE[] = "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 13\r\n"
    "\r\n"
    "Hello, World!";

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

void setup_shutdown(server_t *srv);

void listen_handler(const server_t *srv);
void client_handler(const server_t *srv, connection_t *conn);

int add_connection(const server_t *server, connection_t *conn, uint32_t event_mask);
void remove_connection(const server_t *server, connection_t *conn);

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

  connection_t listen_conn = {listen_fd, FD_TYPE_LISTEN};

  int epoll_fd = epoll_create1(0);
  if (srv->epoll_fd < 0)
  {
    perror("epoll_create1");
    return NULL;
  }

  srv->listen_conn = listen_conn;
  srv->epoll_fd = epoll_fd;
  srv->port = port;

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

      switch (conn->type)
      {
      case FD_TYPE_LISTEN:
        listen_handler(srv);
        break;

      case FD_TYPE_CLIENT:
        client_handler(srv, conn);
        break;

      case FD_TYPE_SIGNAL:
        printf("graceful shutdown...");
        is_running = 0;
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

  free(srv);

  printf("[Server] shutdown completed.");
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

    connection_t *client_conn = malloc(sizeof(connection_t));
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
  char buffer[1024];

  while (1)
  {
    ssize_t bytes_read = recv(conn->fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0)
    {
      buffer[bytes_read] = '\0';

      if (strstr(buffer, "\r\n\r\n") != NULL)
      {
        // Send HTTP response
        send(conn->fd, RESPONSE, sizeof(RESPONSE) - 1, 0);

        remove_connection(srv, conn);
        break;
      }
    }
    else if (bytes_read == 0)
    {
      remove_connection(srv, conn);
      break;
    }
    else
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
      {
        perror("recv");
        remove_connection(srv, conn);
        break;
      }
    }
  }
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
  free(conn);
}

