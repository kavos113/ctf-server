#include "server.h"
#include "server_p.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>

#include "http_request.h"
#include <unistd.h>

int
add_connection(server_t *server, connection_t *conn, uint32_t event_mask)
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
remove_connection(server_t *server, connection_t *conn)
{
  if (conn->fd < 0)
  {
    return;
  }

  epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
  close(conn->fd);
  conn->fd = -1;

  if (conn->type == FD_TYPE_CLIENT)
  {
    if (conn->previous)
    {
      conn->previous->next = conn->next;
    }
    else
    {
      server->clients = conn->next;
    }

    if (conn->next)
    {
      conn->next->previous = conn->previous;
    }

    http_request_t *req = conn->state.client.request;
    conn->state.client.request = NULL;

    if (req)
    {
      req->conn = NULL;

      if (!req->context)
      {
        http_request_dispose(req);
      }
    }

    free(conn->state.client.header_buffer);
    conn->state.client.header_buffer = NULL;
  }

  conn->retired_next = server->retired_clients;
  server->retired_clients = conn;
}

void
reap_connections(server_t *server)
{
  while (server->retired_clients)
  {
    connection_t *conn = server->retired_clients;
    server->retired_clients = conn->retired_next;
    free(conn);
  }
}
