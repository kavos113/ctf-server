#ifndef SERVER_H
#define SERVER_H

#include <sys/uio.h>

#define MAX_IOV 4

typedef enum
{
  FD_TYPE_LISTEN,
  FD_TYPE_CLIENT,
  FD_TYPE_SIGNAL
} fd_type_t;

// connection_t represents event notified from epoll
typedef struct
{
  int fd;
  fd_type_t type;

  struct iovec iov[MAX_IOV];
  int iov_count;
  int iov_index;
} connection_t;

struct http_server_t;

typedef struct
{
  int port;
  int epoll_fd;
  connection_t listen_conn;
  connection_t signal_conn;
  struct http_server_t *http_server;
} server_t;

server_t *create_server(int port, int max_connections);

void serve(server_t *srv);

// destroy and free
void destroy_server(server_t *srv);

#endif // SERVER_H