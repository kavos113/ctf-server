#ifndef APP_HANDLER_AUTH_H
#define APP_HANDLER_AUTH_H

#include "auth.h"
#include "http_server.h"
#include "password_worker.h"

typedef struct
{
  const auth_config_t *config;
  int64_t contest_start_at; // Unix seconds; zero means no restriction.
  int64_t contest_end_at; // Answers at or after this UTC second do not score.
  bool contest_end_enabled;
  password_worker_t *password_worker;
  int64_t (*now)(void *data); // NULL: system UTC clock.
  void *clock_data;
  auth_random_fn random; // NULL: libsodium.
  void *random_data;
} auth_runtime_t;

typedef struct
{
  auth_claims_t claims;
  bool verified; // Set only after the session/user DB check.
} auth_identity_t;

string_t auth_request_user(const http_request_t *request);
bool auth_unauthorized(http_response_t *response);

bool handle_signup_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_signup_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_signup_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_signup_4(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_signup_5(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_login_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_login_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_login_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_login_4(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_login_5(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_auth_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_auth_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_logout_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);
bool handle_logout_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response);

#endif // APP_HANDLER_AUTH_H
