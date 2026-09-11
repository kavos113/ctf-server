#ifndef APP_HANDLER_H
#define APP_HANDLER_H

#include <stdbool.h>

#include "http_response.h"
#include "http_server.h"
#include "db.h"

bool handle_root(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);

bool handle_hello_1(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_hello_2(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);

#endif // APP_HANDLER_H