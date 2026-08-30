#ifndef HTTP_SERVER_P_H
#define HTTP_SERVER_P_H

#include "http_request.h"
#include "http_server.h"

typedef struct
{
  const http_request_t *request;
  http_handler_await_t handler_await;

  // 結果付きdb_task_tなどが入る
  void *data;
} http_server_request_context_t;

#endif // HTTP_SERVER_P_H