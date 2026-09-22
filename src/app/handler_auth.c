#include "handler_auth.h"
#include "json_auth.h"
#include "repository.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

typedef struct
{
  auth_request_t input;
  char user_id[AUTH_ID_LENGTH + 1];
  char session_id[AUTH_ID_LENGTH + 1];
  int64_t issued_at;
  bool found_user;
  char *token;
  char response[AUTH_TOKEN_MAX + 64];
  size_t response_len;
} auth_state;

static int64_t
current_time(const auth_runtime_t *runtime)
{
  return runtime->now ? runtime->now(runtime->clock_data) : (int64_t)time(NULL);
}

static void
free_auth_state(void *data)
{
  auth_state *state = data;
  free_auth_request(&state->input);
  auth_token_free(state->token);
  sodium_memzero(state, sizeof(*state));
  free(state);
}

string_t
auth_request_user(const http_request_t *request)
{
  const auth_identity_t *identity = request->auth_data;

  if (!identity || !identity->verified)
  {
    return (string_t){0};
  }

  return string_from_cstr(identity->claims.user_id);
}

bool
auth_unauthorized(http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_UNAUTHORIZED, .bearer_challenge = true, .no_store = true};
  return true;
}

static auth_state *
read_credentials(http_request_context_t *ctx, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  const auth_runtime_t *runtime = ctx->app_context;

  if (!runtime || !runtime->config || !runtime->password_worker)
  {
    return NULL;
  }

  auth_state *state = calloc(1, sizeof(*state));

  if (!state)
  {
    return NULL;
  }

  ctx->request->app_data = state;
  ctx->request->dispose_app_data = free_auth_state;
  int result = json_to_auth_request(ctx->request->body, ctx->request->content_length, &state->input);

  if (result != 0)
  {
    response->status = result == -2 ? HTTP_STATUS_INTERNAL_SERVER_ERROR : HTTP_STATUS_BAD_REQUEST;
    return NULL;
  }

  return state;
}

static bool
submit_password(http_request_context_t *ctx, password_operation operation, const char *hash,
                http_response_t *response)
{
  auth_runtime_t *runtime = ctx->app_context;
  auth_state *state = ctx->request->app_data;
  password_submit_result result = password_worker_submit(runtime->password_worker, operation,
                                                         &state->input.password, hash, ctx);

  if (result != PASSWORD_QUEUED)
  {
    response->status = result == PASSWORD_QUEUE_FULL || result == PASSWORD_WORKER_STOPPED
                           ? HTTP_STATUS_SERVICE_UNAVAILABLE
                           : HTTP_STATUS_INTERNAL_SERVER_ERROR;
    return true;
  }

  ctx->current_handler = ctx->current_handler->next;
  return false;
}

static void
json_response(auth_state *state, http_status status, http_response_t *response)
{
  *response = (http_response_t){.status = status, .body = state->response, .body_len = state->response_len, .content_type = "application/json", .no_store = true};
}

bool
handle_signup_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  if (!read_credentials(ctx, response))
  {
    return true;
  }

  const char query[] = "SELECT id, username, password_hash FROM users";
  ctx->current_handler = ctx->current_handler->next;
  return db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0;
}

bool
handle_signup_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  auth_state *state = ctx->request->app_data;
  user_t *users;
  size_t count;
  bool bound = bind_users(task->result, &users, &count);
  db_task_free(task);

  if (!bound)
  {
    return true;
  }

  bool duplicate = false;

  for (size_t i = 0; i < count; i++)
  {
    duplicate |= string_equals(users[i].username, state->input.username);
  }

  free_users(users, count);

  if (duplicate)
  {
    response->status = HTTP_STATUS_CONFLICT;
    return true;
  }

  return submit_password(ctx, PASSWORD_HASH, NULL, response);
}

bool
handle_signup_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  auth_runtime_t *runtime = ctx->app_context;
  auth_state *state = ctx->request->app_data;
  const password_result_t *result = ctx->worker_result;

  if (!result || result->status != AUTH_OK || result->cancelled ||
      auth_generate_id(state->user_id, runtime->random, runtime->random_data) != AUTH_OK)
  {
    return true;
  }

  state->response_len = snprintf(state->response, sizeof(state->response),
                                 "{\"id\":\"%s\",\"username\":\"%s\"}", state->user_id, state->input.username.ptr);
  const char query[] = "INSERT INTO users (id, username, password_hash, created_at) VALUES (?, ?, ?, UTC_TIMESTAMP())";
  const db_param_t params[] = {
      {.type = DB_PARAM_STRING, .value.string = {.ptr = state->user_id, .len = AUTH_ID_LENGTH}},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = state->input.username.ptr, .len = state->input.username.len}},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = result->hash, .len = strlen(result->hash)}},
  };
  ctx->current_handler = ctx->current_handler->next;
  return db_exec_query_param(db, query, sizeof(query) - 1, params, 3, ctx) < 0;
}

bool
handle_signup_4(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  bool success = task->result && task->result->success;
  uint64_t affected = success ? task->result->affected : 0;
  db_task_free(task);

  if (success)
  {
    if (affected == 1)
    {
      json_response(ctx->request->app_data, HTTP_STATUS_CREATED, response);
    }

    return true;
  }

  const char query[] = "SELECT id, username, password_hash FROM users";
  ctx->current_handler = ctx->current_handler->next;
  return db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0;
}

bool
handle_signup_5(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  auth_state *state = ctx->request->app_data;
  user_t *users;
  size_t count;
  bool bound = bind_users(task->result, &users, &count);
  db_task_free(task);

  if (!bound)
  {
    return true;
  }

  for (size_t i = 0; i < count; i++)
  {
    if (string_equals(users[i].username, state->input.username))
    {
      response->status = HTTP_STATUS_CONFLICT;
      break;
    }
  }

  free_users(users, count);
  return true;
}

bool
handle_login_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  if (!read_credentials(ctx, response))
  {
    return true;
  }

  const char query[] = "SELECT id, username, password_hash FROM users";
  ctx->current_handler = ctx->current_handler->next;
  return db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0;
}

bool
handle_login_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  auth_state *state = ctx->request->app_data;
  user_t *users;
  size_t count;
  bool bound = bind_users(task->result, &users, &count);
  db_task_free(task);

  if (!bound)
  {
    return true;
  }

  const char *hash = NULL;

  for (size_t i = 0; i < count; i++)
  {
    if (string_equals(users[i].username, state->input.username))
    {
      memcpy(state->user_id, users[i].id, sizeof(state->user_id));
      state->found_user = true;
      hash = users[i].password_hash.ptr;
      break;
    }
  }

  bool complete = submit_password(ctx, PASSWORD_VERIFY, hash, response);
  free_users(users, count);
  return complete;
}

bool
handle_login_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  const password_result_t *result = ctx->worker_result;
  auth_state *state = ctx->request->app_data;

  if (!result || result->cancelled || result->status == AUTH_ERROR)
  {
    return true;
  }

  if (result->status != AUTH_OK || !state->found_user)
  {
    return auth_unauthorized(response);
  }

  const char query[] = "DELETE FROM auth_sessions WHERE expires_at <= UTC_TIMESTAMP() - INTERVAL 30 SECOND";
  ctx->current_handler = ctx->current_handler->next;
  return db_exec_query_param(db, query, sizeof(query) - 1, NULL, 0, ctx) < 0;
}

bool
handle_login_4(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  bool success = task->result && task->result->success;
  db_task_free(task);

  if (!success)
  {
    return true;
  }

  auth_state *state = ctx->request->app_data;
  auth_runtime_t *runtime = ctx->app_context;
  state->issued_at = current_time(runtime);

  if (auth_generate_id(state->session_id, runtime->random, runtime->random_data) != AUTH_OK ||
      auth_token_issue(runtime->config, string_from_cstr(state->user_id), string_from_cstr(state->session_id),
                       state->issued_at, &state->token) != AUTH_OK)
  {
    return true;
  }

  state->response_len = snprintf(state->response, sizeof(state->response), "{\"token\":\"%s\"}", state->token);
  const char query[] = "INSERT INTO auth_sessions (id,user_id,issued_at,expires_at) VALUES "
                       "(?,?,DATE_ADD('1970-01-01', INTERVAL ? SECOND),DATE_ADD('1970-01-01', INTERVAL ? SECOND))";
  const db_param_t params[] = {
      {.type = DB_PARAM_STRING, .value.string = {.ptr = state->session_id, .len = AUTH_ID_LENGTH}},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = state->user_id, .len = AUTH_ID_LENGTH}},
      {.type = DB_PARAM_INT64, .value.integer = state->issued_at},
      {.type = DB_PARAM_INT64, .value.integer = state->issued_at + runtime->config->ttl},
  };
  ctx->current_handler = ctx->current_handler->next;
  return db_exec_query_param(db, query, sizeof(query) - 1, params, 4, ctx) < 0;
}

bool
handle_login_5(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  bool success = task->result && task->result->success && task->result->affected == 1;
  db_task_free(task);

  if (success)
  {
    json_response(ctx->request->app_data, HTTP_STATUS_OK, response);
  }

  return true;
}

bool
handle_auth_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  auth_runtime_t *runtime = ctx->app_context;
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};

  if (!runtime || !runtime->config)
  {
    return true;
  }

  const http_header_t *authorization = NULL;

  for (size_t i = 0; i < ctx->request->header_count; i++)
  {
    const http_header_t *header = &ctx->request->headers[i];

    if (header->name_len == 13 && strncasecmp(header->name, "Authorization", 13) == 0)
    {
      if (authorization)
      {
        return auth_unauthorized(response);
      }

      authorization = header;
    }
  }

  if (!authorization || authorization->value_len < 8 ||
      strncasecmp(authorization->value, "Bearer", 6) != 0 || authorization->value[6] != ' ')
  {
    return auth_unauthorized(response);
  }

  size_t offset = 6;

  while (offset < authorization->value_len && authorization->value[offset] == ' ')
  {
    offset++;
  }

  string_t token = {.ptr = (char *)authorization->value + offset, .len = authorization->value_len - offset};
  auth_claims_t claims;
  int64_t now = current_time(runtime);

  if (now < 0)
  {
    return true;
  }

  auth_result result = auth_token_verify(runtime->config, token, now, &claims);

  if (result != AUTH_OK)
  {
    return result == AUTH_INVALID ? auth_unauthorized(response) : true;
  }

  auth_identity_t *identity = calloc(1, sizeof(*identity));

  if (!identity)
  {
    return true;
  }

  identity->claims = claims;
  ctx->request->auth_data = identity;
  ctx->request->dispose_auth_data = free;
  const char query[] = "SELECT s.id,u.id,TIMESTAMPDIFF(SECOND,'1970-01-01',s.issued_at),"
                       "TIMESTAMPDIFF(SECOND,'1970-01-01',s.expires_at) FROM auth_sessions s "
                       "JOIN users u ON u.id=s.user_id WHERE s.expires_at>UTC_TIMESTAMP()-INTERVAL 30 SECOND";
  _Static_assert(sizeof(query) <= DEFAULT_QUERY_SIZE, "authentication SELECT exceeds DB query buffer");
  ctx->current_handler = ctx->current_handler->next;
  return db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0;
}

bool
handle_auth_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  auth_session_t *sessions;
  size_t count;
  bool bound = bind_auth_sessions(task->result, &sessions, &count);
  db_task_free(task);

  if (!bound)
  {
    return true;
  }

  auth_identity_t *identity = ctx->request->auth_data;
  const auth_runtime_t *runtime = ctx->app_context;
  int64_t now = current_time(runtime);
  bool found = false;

  for (size_t i = 0; i < count; i++)
  {
    found |= strcmp(sessions[i].id, identity->claims.session_id) == 0 &&
             strcmp(sessions[i].user_id, identity->claims.user_id) == 0 &&
             sessions[i].issued_at == identity->claims.issued_at &&
             sessions[i].expires_at == identity->claims.expires_at;
  }

  free_auth_sessions(sessions);

  if (now < 0)
  {
    return true;
  }

  if (!found || now >= identity->claims.expires_at + AUTH_CLOCK_SKEW ||
      now + AUTH_CLOCK_SKEW < identity->claims.not_before)
  {
    return auth_unauthorized(response);
  }

  identity->verified = true;
  ctx->current_handler = ctx->current_handler->next;
  return ctx->current_handler->func(ctx, db, NULL, response);
}

bool
handle_logout_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  if (!auth_request_user(ctx->request).ptr)
  {
    return auth_unauthorized(response);
  }

  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  const auth_identity_t *identity = ctx->request->auth_data;
  const char query[] = "DELETE FROM auth_sessions WHERE id=? AND user_id=?";
  const db_param_t params[] = {
      {.type = DB_PARAM_STRING, .value.string = {.ptr = identity->claims.session_id, .len = AUTH_ID_LENGTH}},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = identity->claims.user_id, .len = AUTH_ID_LENGTH}},
  };
  ctx->current_handler = ctx->current_handler->next;
  return db_exec_query_param(db, query, sizeof(query) - 1, params, 2, ctx) < 0;
}

bool
handle_logout_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  bool success = task->result && task->result->success && task->result->affected <= 1;
  db_task_free(task);
  *response = (http_response_t){.status = success ? HTTP_STATUS_OK : HTTP_STATUS_INTERNAL_SERVER_ERROR,
                                .no_store = true};
  return true;
}
