#include "http_server.h"
#include "http_server_p.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void
http_request_context_dispose(http_request_context_t *ctx)
{
  http_request_t *request = ctx->request;
  request->context = NULL;

  if (!request->conn)
  {
    http_request_dispose(request);
  }

  free(ctx);
}

void
http_server_add_route(
    http_server_t *server,
    http_method method,
    const char *path,
    http_handler_t *handler)
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
http_server_handle_request(const http_server_t *server, http_request_t *req, db_pool_t *db, bool *is_complete)
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
      http_request_context_t *ctx = malloc(sizeof(http_request_context_t));
      if (!ctx)
      {
        *is_complete = true;
        return (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
      }

      req->context = ctx;
      ctx->request = req;
      ctx->app_context = server->app_context;
      ctx->worker_result = NULL;
      ctx->current_handler = route->handler;

      http_response_t response;
      bool completed = route->handler->func(ctx, db, NULL, &response);

      if (completed)
      {
        *is_complete = true;
        http_request_context_dispose(ctx);
        return response;
      }
      else
      {
        *is_complete = false;
        return (http_response_t){
            .status = HTTP_STATUS_OK,
        };
      }
    }
  }

  *is_complete = true;
  return (http_response_t){
      .status = HTTP_STATUS_NOT_FOUND,
      .body = "Not Found",
      .body_len = 9};
}
