#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "http.h"
#include "http_request.h"
#include "http_response.h"

typedef http_response_t (*http_handler_t)(const http_request_t *req);

typedef struct
{
  http_method method;
  const char *path;
  size_t path_len;
  http_handler_t handler;
} http_route_t;

#define MAX_ROUTES 64

typedef struct
{
  http_route_t routes[MAX_ROUTES];
  size_t route_count;
} http_server_t;

void http_server_add_route(http_server_t *server, http_method method, const char *path, http_handler_t handler);
http_response_t http_server_handle_request(http_server_t *server, http_request_t *req);

#endif // HTTP_SERVER_H