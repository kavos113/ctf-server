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
http_server_handle_request(const http_server_t *server, http_request_t *req)
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
      route->handler_async(req);

      http_server_request_context_t *context = malloc(sizeof(http_server_request_context_t));
      context->request = req;
      context->handler_await = route->handler_await;

      req->conn->data = (void *)context;

      return (http_response_t){
          .status = HTTP_STATUS_OK,
      };
    }
  }

  return (http_response_t){
      .status = HTTP_STATUS_NOT_FOUND,
      .body = "Not Found",
      .body_len = 9};
}

http_response_t
http_server_finish_request(const http_server_t *server, connection_t *conn)
{
  http_server_request_context_t *ctx = (http_server_request_context_t *)conn->data;
  return ctx->handler_await(ctx->request, ctx->data);
}