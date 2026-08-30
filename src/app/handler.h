#ifndef APP_HANDLER_H
#define APP_HANDLER_H

#include "http_request.h"
#include "http_response.h"
#include "http_server.h"
#include "db.h"

http_response_t handle_root(const http_request_t *req, db_task_t *task);

void handle_hello_async(const http_request_context_t *req, db_pool_t *db);
http_response_t handle_hello(const http_request_t *req, db_task_t *task);


#endif // APP_HANDLER_H