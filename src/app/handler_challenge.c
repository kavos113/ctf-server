#include "handler.h"
#include "handler_auth.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "json.h"
#include "repository.h"

bool
handle_get_challenges_1(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response)
{
  const auth_runtime_t *runtime = ctx->app_context;

  if (runtime && runtime->contest_start_at)
  {
    int64_t now = runtime->now ? runtime->now(runtime->clock_data) : (int64_t)time(NULL);

    if (now < 0 || now < runtime->contest_start_at)
    {
      *out_response = (http_response_t){
          .status = now < 0 ? HTTP_STATUS_INTERNAL_SERVER_ERROR : HTTP_STATUS_FORBIDDEN,
          .no_store = true,
      };
      return true;
    }
  }

  assert(ctx->current_handler->next != NULL);
  ctx->current_handler = ctx->current_handler->next;

  *out_response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  const char query[] = "SELECT id, creator_id, name, description, genre FROM challenges;";
  return db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0;
}

bool
handle_get_challenges_2(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response)
{
  assert(ctx->current_handler->next == NULL);
  assert(task != NULL);
  assert(task->result != NULL);

  if (!task->result->success)
  {
    *out_response = (http_response_t){
        .status = HTTP_STATUS_INTERNAL_SERVER_ERROR,
        .body = "db error",
        .body_len = 8,
    };
    fprintf(stderr, "Database query failed: %s\n", task->result->err_msg);
    db_task_free(task);
    return true;
  }

  size_t rows;
  challenge_t *challenges = bind_challenges_without_flag(task->result, &rows);
  if (!challenges)
  {
    *out_response = (http_response_t){
        .status = HTTP_STATUS_OK,
        .body = "[]",
        .body_len = 2,
        .content_type = "application/json",
    };
    db_task_free(task);
    fprintf(stderr, "Failed to bind challenges from database result\n");
    return true;
  }

  string_t json_str;
  challenges_to_json_without_flag(challenges, rows, &json_str, false);
  if (!json_str.ptr)
  {
    *out_response = (http_response_t){
        .status = HTTP_STATUS_INTERNAL_SERVER_ERROR,
        .body = "json error",
        .body_len = 10,
    };
    fprintf(stderr, "Failed to convert challenges to JSON\n");
    db_task_free(task);
    free_challenges(challenges, rows);
    return true;
  }

  ctx->request->app_data = json_str.ptr;
  ctx->request->dispose_app_data = free;
  *out_response = (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = json_str.ptr,
      .body_len = json_str.len,
      .content_type = "application/json",
  };

  db_task_free(task);
  free_challenges(challenges, rows);

  return true;
}

bool
handle_get_own_challenges_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  if (!auth_request_user(ctx->request).ptr)
  {
    return auth_unauthorized(response);
  }

  assert(ctx->current_handler->next != NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  const char query[] = "SELECT id, creator_id, name, description, flag, genre FROM challenges";

  if (db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0)
  {
    return true;
  }

  ctx->current_handler = ctx->current_handler->next;
  return false;
}

bool
handle_get_own_challenges_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR, .no_store = true};
  string_t user = auth_request_user(ctx->request);

  if (!user.ptr)
  {
    db_task_free(task);
    return auth_unauthorized(response);
  }

  if (!task->result || !task->result->success || !task->result->res ||
      mysql_num_fields(task->result->res) != 6)
  {
    db_task_free(task);
    return true;
  }

  bool empty = mysql_num_rows(task->result->res) == 0;
  size_t count;
  challenge_t *challenges = bind_challenges(task->result, &count);
  db_task_free(task);

  if (!challenges && !empty)
  {
    return true;
  }

  challenge_t *selected = count ? calloc(count, sizeof(*selected)) : NULL;

  if (count && !selected)
  {
    free_challenges(challenges, count);
    return true;
  }

  size_t selected_count = 0;

  for (size_t i = 0; i < count; i++)
  {
    if (string_equals(challenges[i].creator_id, user))
    {
      selected[selected_count++] = challenges[i];
    }
  }

  string_t json = {0};

  if (selected_count)
  {
    challenges_to_json(selected, selected_count, &json, false);
  }

  free(selected); // Strings belong to challenges.
  free_challenges(challenges, count);

  if (selected_count && !json.ptr)
  {
    return true;
  }

  ctx->request->app_data = json.ptr;
  ctx->request->dispose_app_data = free;
  *response = (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = json.ptr ? json.ptr : "[]",
      .body_len = json.ptr ? json.len : 2,
      .content_type = "application/json",
      .no_store = true,
  };
  return true;
}

bool
handle_post_challenges_1(struct http_request_context *ctx,
                         db_pool_t *db,
                         db_task_t *task,
                         http_response_t *out_response)
{
  if (!auth_request_user(ctx->request).ptr)
  {
    return auth_unauthorized(out_response);
  }

  assert(ctx->current_handler->next != NULL);

  create_challenge_request_t request;
  int result = json_to_create_challenge_request(ctx->request->body, ctx->request->content_length, &request);

  if (result != 0)
  {
    *out_response = (http_response_t){
        .status = result == -2 ? HTTP_STATUS_INTERNAL_SERVER_ERROR : HTTP_STATUS_BAD_REQUEST,
    };
    return true;
  }

  const char query[] =
      "INSERT INTO challenges (creator_id, name, description, flag, genre) VALUES (?, ?, ?, ?, ?)";
  string_t user = auth_request_user(ctx->request);
  const db_param_t params[] = {
      {
          .type = DB_PARAM_STRING,
          .value.string = {.ptr = user.ptr, .len = user.len},
      },
      {
          .type = DB_PARAM_STRING,
          .value.string = {.ptr = request.name.ptr, .len = request.name.len},
      },
      {
          .type = DB_PARAM_STRING,
          .value.string = {.ptr = request.description.ptr, .len = request.description.len},
      },
      {
          .type = DB_PARAM_STRING,
          .value.string = {.ptr = request.flag.ptr, .len = request.flag.len},
      },
      {
          .type = DB_PARAM_INT64,
          .value.integer = request.genre,
      },
  };

  if (db_exec_query_param(db, query, sizeof(query) - 1, params, 5, ctx) < 0)
  {
    *out_response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
    return true;
  }

  ctx->current_handler = ctx->current_handler->next;
  return false;
}

bool
handle_post_challenges_2(struct http_request_context *ctx,
                         db_pool_t *db,
                         db_task_t *task,
                         http_response_t *out_response)
{
  assert(ctx->current_handler->next == NULL);
  assert(task != NULL);
  assert(task->result != NULL);

  *out_response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};

  if (task->result->success &&
      task->result->affected == 1 &&
      task->result->insert_id > 0 &&
      task->result->insert_id <= INT_MAX)
  {
    challenge_t challenge = {
        .id = (int)task->result->insert_id,
        .creator_id = {
            .ptr = (char *)task->params[0].value.string.ptr,
            .len = task->params[0].value.string.len,
        },
        .name = {
            .ptr = (char *)task->params[1].value.string.ptr,
            .len = task->params[1].value.string.len,
        },
        .description = {
            .ptr = (char *)task->params[2].value.string.ptr,
            .len = task->params[2].value.string.len,
        },
        .flag = {
            .ptr = (char *)task->params[3].value.string.ptr,
            .len = task->params[3].value.string.len,
        },
        .genre = (ctf_genre)task->params[4].value.integer,
    };
    string_t json = {0};
    challenge_to_json(&challenge, &json, false);

    if (json.ptr)
    {
      ctx->request->app_data = json.ptr;
      ctx->request->dispose_app_data = free;

      *out_response = (http_response_t){
          .status = HTTP_STATUS_CREATED,
          .body = json.ptr,
          .body_len = json.len,
          .content_type = "application/json",
      };
    }
  }

  free(task->result);
  free(task);
  return true;
}

typedef struct
{
  int id;
  create_challenge_request_t input;
  string_t response;
} challenge_write_state;

static void
free_challenge_write_state(void *data)
{
  challenge_write_state *state = data;
  free(state->response.ptr);
  free(state);
}

static void
free_challenge_task(db_task_t *task)
{
  if (task->result)
  {
    if (task->result->res)
    {
      mysql_free_result(task->result->res);
    }

    free(task->result);
  }

  free(task);
}

static bool
parse_challenge_id(const char *text, size_t len, int *out)
{
  if (!text || !len || text[0] < '1' || text[0] > '9')
  {
    return false;
  }

  int value = 0;

  for (size_t i = 0; i < len; i++)
  {
    if (text[i] < '0' || text[i] > '9')
    {
      return false;
    }

    int digit = text[i] - '0';

    if (value > (INT_MAX - digit) / 10)
    {
      return false;
    }

    value = value * 10 + digit;
  }

  *out = value;
  return true;
}

static bool
read_challenge_id(const http_request_t *request, int *id)
{
  bool found = false;

  for (size_t i = 0; i < request->query_param_count; i++)
  {
    const http_param_t *param = &request->query_params[i];

    if (param->name_len != 2 || memcmp(param->name, "id", 2) != 0)
    {
      continue;
    }

    if (found || !parse_challenge_id(param->value, param->value_len, id))
    {
      return false;
    }

    found = true;
  }

  return found;
}

static challenge_write_state *
new_challenge_write_state(http_request_context_t *ctx, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  int id;

  if (!read_challenge_id(ctx->request, &id))
  {
    response->status = HTTP_STATUS_BAD_REQUEST;
    return NULL;
  }

  challenge_write_state *state = calloc(1, sizeof(*state));

  if (!state)
  {
    return NULL;
  }

  state->id = id;
  ctx->request->app_data = state;
  ctx->request->dispose_app_data = free_challenge_write_state;
  return state;
}

static bool
select_challenge_owners(http_request_context_t *ctx, db_pool_t *db)
{
  assert(ctx->current_handler->next != NULL);
  const char query[] = "SELECT id, creator_id FROM challenges";
  ctx->current_handler = ctx->current_handler->next;
  return db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0;
}

static http_status
challenge_write_access(const db_result_t *result, int target_id, string_t user)
{
  if (!result || !result->success || !result->res || mysql_num_fields(result->res) != 2)
  {
    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
  }

  MYSQL_ROW row;

  while ((row = mysql_fetch_row(result->res)))
  {
    unsigned long *lengths = mysql_fetch_lengths(result->res);
    int id;

    if (!lengths || !row[1] || !parse_challenge_id(row[0], lengths[0], &id))
    {
      return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    if (id == target_id)
    {
      return lengths[1] == user.len && memcmp(row[1], user.ptr, user.len) == 0 ? HTTP_STATUS_OK
                                                                          : HTTP_STATUS_FORBIDDEN;
    }
  }

  return HTTP_STATUS_NOT_FOUND;
}

static void
updated_challenge_response(challenge_write_state *state, string_t user, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  challenge_t challenge = {
      .id = state->id,
      .creator_id = user,
      .name = state->input.name,
      .description = state->input.description,
      .flag = state->input.flag,
      .genre = state->input.genre,
  };
  challenge_to_json(&challenge, &state->response, false);

  if (state->response.ptr)
  {
    *response = (http_response_t){
        .status = HTTP_STATUS_OK,
        .body = state->response.ptr,
        .body_len = state->response.len,
        .content_type = "application/json",
    };
  }
}

bool
handle_put_challenges_1(http_request_context_t *ctx,
                        db_pool_t *db,
                        db_task_t *task,
                        http_response_t *out_response)
{
  if (!auth_request_user(ctx->request).ptr)
  {
    return auth_unauthorized(out_response);
  }

  challenge_write_state *state = new_challenge_write_state(ctx, out_response);

  if (!state)
  {
    return true;
  }

  int parsed = json_to_create_challenge_request(
      ctx->request->body, ctx->request->content_length, &state->input);

  if (parsed != 0)
  {
    out_response->status =
        parsed == -2 ? HTTP_STATUS_INTERNAL_SERVER_ERROR : HTTP_STATUS_BAD_REQUEST;
    return true;
  }

  return select_challenge_owners(ctx, db);
}

bool
handle_put_challenges_2(http_request_context_t *ctx,
                        db_pool_t *db,
                        db_task_t *task,
                        http_response_t *out_response)
{
  assert(ctx->current_handler->next != NULL);
  challenge_write_state *state = ctx->request->app_data;
  http_status access = challenge_write_access(task->result, state->id, auth_request_user(ctx->request));
  free_challenge_task(task);
  *out_response = (http_response_t){.status = access};

  if (access != HTTP_STATUS_OK)
  {
    return true;
  }

  const char query[] = "UPDATE challenges SET name = ?, description = ?, flag = ?, genre = ? "
                       "WHERE id = ? AND creator_id = ?";
  string_t user = auth_request_user(ctx->request);
  const db_param_t params[] = {
      {
          .type = DB_PARAM_STRING,
          .value.string = {.ptr = state->input.name.ptr, .len = state->input.name.len},
      },
      {
          .type = DB_PARAM_STRING,
          .value.string = {.ptr = state->input.description.ptr,
                           .len = state->input.description.len},
      },
      {
          .type = DB_PARAM_STRING,
          .value.string = {.ptr = state->input.flag.ptr, .len = state->input.flag.len},
      },
      {.type = DB_PARAM_INT64, .value.integer = state->input.genre},
      {.type = DB_PARAM_INT64, .value.integer = state->id},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = user.ptr, .len = user.len}},
  };

  if (db_exec_query_param(db, query, sizeof(query) - 1, params, 6, ctx) < 0)
  {
    out_response->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    return true;
  }

  ctx->current_handler = ctx->current_handler->next;
  return false;
}

bool
handle_put_challenges_3(http_request_context_t *ctx,
                        db_pool_t *db,
                        db_task_t *task,
                        http_response_t *out_response)
{
  *out_response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  bool success = task->result && task->result->success;
  uint64_t affected = success ? task->result->affected : 0;
  free_challenge_task(task);

  if (!success || affected > 1)
  {
    return true;
  }

  if (affected == 0)
  {
    return select_challenge_owners(ctx, db);
  }

  updated_challenge_response(ctx->request->app_data, auth_request_user(ctx->request), out_response);
  return true;
}

bool
handle_put_challenges_4(http_request_context_t *ctx,
                        db_pool_t *db,
                        db_task_t *task,
                        http_response_t *out_response)
{
  assert(ctx->current_handler->next == NULL);
  challenge_write_state *state = ctx->request->app_data;
  http_status access = challenge_write_access(task->result, state->id, auth_request_user(ctx->request));
  free_challenge_task(task);
  *out_response = (http_response_t){.status = access};

  if (access == HTTP_STATUS_OK)
  {
    updated_challenge_response(state, auth_request_user(ctx->request), out_response);
  }

  return true;
}

bool
handle_delete_challenges_1(http_request_context_t *ctx,
                           db_pool_t *db,
                           db_task_t *task,
                           http_response_t *out_response)
{
  if (!auth_request_user(ctx->request).ptr)
  {
    return auth_unauthorized(out_response);
  }

  if (!new_challenge_write_state(ctx, out_response))
  {
    return true;
  }

  return select_challenge_owners(ctx, db);
}

bool
handle_delete_challenges_2(http_request_context_t *ctx,
                           db_pool_t *db,
                           db_task_t *task,
                           http_response_t *out_response)
{
  assert(ctx->current_handler->next != NULL);
  challenge_write_state *state = ctx->request->app_data;
  http_status access = challenge_write_access(task->result, state->id, auth_request_user(ctx->request));
  free_challenge_task(task);
  *out_response = (http_response_t){.status = access};

  if (access != HTTP_STATUS_OK)
  {
    return true;
  }

  const char query[] = "DELETE FROM challenges WHERE id = ? AND creator_id = ?";
  string_t user = auth_request_user(ctx->request);
  const db_param_t params[] = {
      {.type = DB_PARAM_INT64, .value.integer = state->id},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = user.ptr, .len = user.len}},
  };

  if (db_exec_query_param(db, query, sizeof(query) - 1, params, 2, ctx) < 0)
  {
    out_response->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    return true;
  }

  ctx->current_handler = ctx->current_handler->next;
  return false;
}

bool
handle_delete_challenges_3(http_request_context_t *ctx,
                           db_pool_t *db,
                           db_task_t *task,
                           http_response_t *out_response)
{
  *out_response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  bool success = task->result && task->result->success;
  uint64_t affected = success ? task->result->affected : 0;
  free_challenge_task(task);

  if (!success || affected > 1)
  {
    return true;
  }

  if (affected == 0)
  {
    return select_challenge_owners(ctx, db);
  }

  out_response->status = HTTP_STATUS_OK;
  return true;
}

bool
handle_delete_challenges_4(http_request_context_t *ctx,
                           db_pool_t *db,
                           db_task_t *task,
                           http_response_t *out_response)
{
  assert(ctx->current_handler->next == NULL);
  challenge_write_state *state = ctx->request->app_data;
  http_status access = challenge_write_access(task->result, state->id, auth_request_user(ctx->request));
  free_challenge_task(task);
  *out_response = (http_response_t){
      .status = access == HTTP_STATUS_OK ? HTTP_STATUS_INTERNAL_SERVER_ERROR : access,
  };
  return true;
}
