#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>

#include "db.h"
#include "http.h"
#include "http_request.h"
#include "http_response.h"

struct http_handler;
struct http_request_context;

// これ以上関数呼び出しが必要ないとき（responseを返すとき）はtrueを返す
typedef bool (*http_handler_func_t)(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);

typedef struct http_handler
{
  http_handler_func_t func;
  struct http_handler *next;
} http_handler_t;

typedef struct http_request_context
{
  http_request_t *request;

  // handler実行中はその実行しているhandlerが入る（なので，handler内でnextを処理する）
  http_handler_t *current_handler;
} http_request_context_t;

typedef struct
{
  http_method method;
  const char *path;
  size_t path_len;

  // handlerは全部mallocする
  http_handler_t *handler;
} http_route_t;

#define MAX_ROUTES 64

struct http_server_t
{
  http_route_t routes[MAX_ROUTES];
  size_t route_count;
};
typedef struct http_server_t http_server_t;

void http_server_add_route(
    http_server_t *server,
    http_method method,
    const char *path,
    http_handler_t *handler);

http_response_t http_server_handle_request(const http_server_t *server, http_request_t *req, db_pool_t *db, bool *is_complete);

#endif // HTTP_SERVER_H