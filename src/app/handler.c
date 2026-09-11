#include "handler.h"

#include <assert.h>

bool handle_root(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response)
{
  *out_response = (http_response_t){
    .status = HTTP_STATUS_OK,
      .body = "Welcome to the CTF server!",
      .body_len = 26,
  };

  return true;
}

bool handle_hello_1(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response)
{
  assert(ctx->current_handler->next != NULL);
  ctx->current_handler = ctx->current_handler->next;

  db_pool_exec_query(db, "SELECT * FROM challenges;", 26, (void *)ctx);
  return false;
}

bool handle_hello_2(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response)
{
  *out_response = (http_response_t){
    .status = HTTP_STATUS_OK,
    .body = "Hello, World!",
    .body_len = 13,
  };
  return true;
}