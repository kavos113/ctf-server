#include "handler.h"

http_response_t
handle_root(const http_request_t *req, db_task_t *task)
{
  return (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = "Welcome to the CTF server!",
      .body_len = 26};
}

void handle_hello_async(const http_request_context_t *req, db_pool_t *db)
{
  db_pool_exec_query(db, "SELECT * FROM challenges;", 26, (void *)req);
}

http_response_t
handle_hello(const http_request_t *req, db_task_t *task)
{
  return (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = "Hello, World!",
      .body_len = 13};
}