#ifndef APP_HANDLER_H
#define APP_HANDLER_H

#include <stdbool.h>

#include "db.h"
#include "http_response.h"
#include "http_server.h"

bool handle_get_users_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_users_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_users_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);

bool handle_root(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);

bool handle_hello_1(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_hello_2(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);

bool handle_get_challenges_1(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_get_challenges_2(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);

bool handle_get_own_challenges_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_own_challenges_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);

bool handle_post_challenges_1(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_post_challenges_2(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);

bool handle_put_challenges_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_put_challenges_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_put_challenges_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_put_challenges_4(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_delete_challenges_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_delete_challenges_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_delete_challenges_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);
bool handle_delete_challenges_4(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response);

bool handle_post_answers_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_post_answers_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_post_answers_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_post_answers_4(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_post_answers_5(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_answers_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_answers_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_answers_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_own_answers_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_own_answers_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_get_own_answers_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);

#endif // APP_HANDLER_H