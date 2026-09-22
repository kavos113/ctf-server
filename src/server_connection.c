#include "server.h"
#include "server_p.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>

#include "http_request.h"

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

  if (conn->type == FD_TYPE_CLIENT)
  {
    http_request_t *req = (http_request_t *)conn->state.client.request;
    if (req)
    {
      http_request_dispose(req);
    }
  }
  free(conn);
}