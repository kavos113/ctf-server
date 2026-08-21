#include "server.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>

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

server *
create_server(int port, int max_connections)
{
  server *srv = malloc(sizeof(server));
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
serve(server *srv)
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

  while (1)
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

      // accept new connections
      if (conn->type == FD_TYPE_LISTEN)
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

          struct epoll_event client_event;
          client_event.events = EPOLLIN | EPOLLET; // Edge-triggered
          client_event.data.ptr = client_conn;
          if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) < 0)
          {
            perror("epoll_ctl");
            close(client_fd);
            free(client_conn);
            continue;
          }

          printf("Accepted connection on fd %d\n", client_fd);
        }
      }
      else if (conn->type == FD_TYPE_CLIENT)
      {
        char buffer[1024];
        ssize_t bytes_read = recv(conn->fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read > 0)
        {
          buffer[bytes_read] = '\0';

          if (strstr(buffer, "\r\n\r\n") != NULL)
          {
            // Send HTTP response
            send(conn->fd, RESPONSE, sizeof(RESPONSE) - 1, 0);

            epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);
            free(conn);
          }
        }
        else if (bytes_read == 0)
        {
          // Client closed connection
          epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
          close(conn->fd);
          free(conn);
        }
        else
        {
          if (errno != EAGAIN && errno != EWOULDBLOCK)
          {
            perror("recv");
            epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);
            free(conn);
          }
        }
      }
    }
  }
}

void
destroy_server(server *srv)
{
  close(srv->epoll_fd);
  close(srv->listen_conn.fd);

  free(srv);
}