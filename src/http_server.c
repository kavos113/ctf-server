#include "http_server.h"
#include "http_server_p.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void
http_server_add_route(
    http_server_t *server,
    http_method method,
    const char *path,
    http_handler_async_t handler_async,
    http_handler_await_t handler_await)
{
  if (server->route_count >= MAX_ROUTES)
  {
    return;
  }

  http_route_t *route = &server->routes[server->route_count++];
  route->method = method;
  route->path = path;
  route->path_len = strlen(path);
  route->handler_async = handler_async;
  route->handler_await = handler_await;
}

// TODO: wildcard path
http_response_t
http_server_handle_request(const http_server_t *server, http_request_t *req, db_pool_t* db, int *is_complete)
{
  for (size_t i = 0; i < server->route_count; i++)
  {
    const http_route_t *route = &server->routes[i];

    if (route->method != req->method)
    {
      continue;
    }

    if (route->path_len != req->uri_len)
    {
      continue;
    }

    if (strncmp(route->path, req->uri, route->path_len) == 0)
    {
      // do not need db connection
      if (!route->handler_async)
      {
        http_response_t res = route->handler_await(req, NULL);

        *is_complete = 1;
        return res;
      }

      route->handler_async(req, db);

      *is_complete = 0;
      return (http_response_t){
          .status = HTTP_STATUS_OK,
      };
    }
  }

  *is_complete = 1;
  return (http_response_t){
      .status = HTTP_STATUS_NOT_FOUND,
      .body = "Not Found",
      .body_len = 9};
}

