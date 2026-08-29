#include "http_server.h"
#include "http_server_p.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

void
http_server_add_route(http_server_t *server, http_method method, const char *path, http_handler_t handler)
{
  if (server->route_count >= MAX_ROUTES)
  {
    return;
  }

  http_route_t *route = &server->routes[server->route_count++];
  route->method = method;
  route->path = path;
  route->path_len = strlen(path);
  route->handler = handler;
}

// TODO: wildcard path
http_response_t
http_server_handle_request(http_server_t *server, http_request_t *req)
{
  normalize_uri(req);

  for (size_t i = 0; i < server->route_count; i++)
  {
    http_route_t *route = &server->routes[i];

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
      return route->handler(req);
    }
  }

  return (http_response_t){
      .status = HTTP_STATUS_NOT_FOUND,
      .body = "Not Found",
      .body_len = strlen("Not Found")};
}