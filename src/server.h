#ifndef SERVER_H
#define SERVER_H

typedef enum
{
  FD_TYPE_LISTEN,
  FD_TYPE_CLIENT
} fd_type_t;

typedef struct
{
  int fd;
  fd_type_t type;
} connection_t;

typedef struct
{
  int port;
  int epoll_fd;
  connection_t listen_conn;
} server_t;

server_t *create_server(int port, int max_connections);

void serve(server_t *srv);

// destroy and free
void destroy_server(server_t *srv);

#endif // SERVER_H