#include "handler.h"
#include "json.h"
#include "json_p.h"
#include "repository.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  submit_answer_request_t input;
  int insert_id;
  string_t response;
} answer_submit_state;

typedef struct
{
  int challenge_id;
  string_t response;
} answer_list_state;

static void
free_submit_state(void *data)
{
  answer_submit_state *state = data;
  free(state->response.ptr);
  free(state);
}

static void
free_list_state(void *data)
{
  answer_list_state *state = data;
  free(state->response.ptr);
  free(state);
}

static void
free_answer_task(db_task_t *task)
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
answer_filter(const http_request_t *request, int *id)
{
  bool found = false;
  *id = 0;

  for (size_t i = 0; i < request->query_param_count; i++)
  {
    const http_param_t *param = &request->query_params[i];

    if (param->name_len != sizeof("challenge_id") - 1 ||
        memcmp(param->name, "challenge_id", sizeof("challenge_id") - 1) != 0)
    {
      continue;
    }

    if (found || !param->value || !param->value_len || param->value[0] < '1' ||
        param->value[0] > '9')
    {
      return false;
    }

    json_parser_t parser = {
        .cur = param->value, .end = param->value + param->value_len, .error = -1};

    if (!read_positive_integer(&parser, id) || parser.cur != parser.end)
    {
      return false;
    }

    found = true;
  }

  return true;
}

static http_status
find_answer_challenge(const db_result_t *result, int target)
{
  if (!result || !result->success || !result->res || mysql_num_fields(result->res) != 1)
  {
    return HTTP_STATUS_INTERNAL_SERVER_ERROR;
  }

  if (!target)
  {
    return HTTP_STATUS_OK;
  }

  MYSQL_ROW row;

  while ((row = mysql_fetch_row(result->res)))
  {
    unsigned long *lengths = mysql_fetch_lengths(result->res);

    if (!lengths || !row[0])
    {
      return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    json_parser_t parser = {.cur = row[0], .end = row[0] + lengths[0], .error = -1};
    int id;

    if (!read_positive_integer(&parser, &id) || parser.cur != parser.end)
    {
      return HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }

    if (id == target)
    {
      return HTTP_STATUS_OK;
    }
  }

  return HTTP_STATUS_NOT_FOUND;
}

bool
handle_post_answers_1(http_request_context_t *ctx,
                      db_pool_t *db,
                      db_task_t *task,
                      http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  answer_submit_state *state = calloc(1, sizeof(*state));

  if (!state)
  {
    return true;
  }

  ctx->request->app_data = state;
  ctx->request->dispose_app_data = free_submit_state;
  int parsed = json_to_submit_answer_request(
      ctx->request->body, ctx->request->content_length, &state->input);

  if (parsed != 0)
  {
    response->status = parsed == -2 ? HTTP_STATUS_INTERNAL_SERVER_ERROR : HTTP_STATUS_BAD_REQUEST;
    return true;
  }

  const char query[] = "SELECT id, creator_id, name, description, flag, genre FROM challenges";
  ctx->current_handler = ctx->current_handler->next;
  db_pool_exec_query(db, query, sizeof(query) - 1, ctx);
  return false;
}

bool
handle_post_answers_2(http_request_context_t *ctx,
                      db_pool_t *db,
                      db_task_t *task,
                      http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  answer_submit_state *state = ctx->request->app_data;
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};

  if (!task->result || !task->result->success || !task->result->res ||
      mysql_num_fields(task->result->res) != 6)
  {
    free_answer_task(task);
    return true;
  }

  bool empty = mysql_num_rows(task->result->res) == 0;
  size_t count;
  challenge_t *challenges = bind_challenges(task->result, &count);
  free_answer_task(task);

  if (!challenges)
  {
    response->status = empty ? HTTP_STATUS_NOT_FOUND : HTTP_STATUS_INTERNAL_SERVER_ERROR;
    return true;
  }

  response->status = HTTP_STATUS_NOT_FOUND;
  int correct = -1;

  for (size_t i = 0; i < count; i++)
  {
    if (challenges[i].id == state->input.challenge_id)
    {
      correct = json_string_equal_decoded(challenges[i].flag, state->input.answer);
      response->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
      break;
    }
  }

  free_challenges(challenges, count);

  if (correct < 0)
  {
    return true;
  }

  const char query[] =
      "INSERT INTO answers (challenge_id, user_id, answer, is_correct, created_at) "
      "VALUES (?, ?, ?, ?, UTC_TIMESTAMP())";
  const db_param_t params[] = {
      {.type = DB_PARAM_INT64, .value.integer = state->input.challenge_id},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = "dummy", .len = 5}},
      {.type = DB_PARAM_STRING,
       .value.string = {.ptr = state->input.answer.ptr, .len = state->input.answer.len}},
      {.type = DB_PARAM_INT64, .value.integer = correct},
  };

  if (db_exec_query_param(db, query, sizeof(query) - 1, params, 4, ctx) < 0)
  {
    return true;
  }

  ctx->current_handler = ctx->current_handler->next;
  return false;
}

bool
handle_post_answers_3(http_request_context_t *ctx,
                      db_pool_t *db,
                      db_task_t *task,
                      http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  assert(ctx->current_handler->next->next != NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  answer_submit_state *state = ctx->request->app_data;
  bool success = task->result && task->result->success;
  uint64_t affected = success ? task->result->affected : 0;
  uint64_t insert_id = success ? task->result->insert_id : 0;
  free_answer_task(task);

  if (!success)
  {
    // Skip the normal response handler; the last handler checks for deletion.
    const char query[] = "SELECT id FROM challenges";
    ctx->current_handler = ctx->current_handler->next->next;
    db_pool_exec_query(db, query, sizeof(query) - 1, ctx);
    return false;
  }

  if (affected != 1 || insert_id == 0 || insert_id > INT_MAX)
  {
    return true;
  }

  state->insert_id = (int)insert_id;
  const char query[] = "SELECT id, challenge_id, user_id, answer, is_correct, "
                       "DATE_FORMAT(created_at, '%Y-%m-%dT%H:%i:%sZ') FROM answers ORDER BY id";
  ctx->current_handler = ctx->current_handler->next;
  db_pool_exec_query(db, query, sizeof(query) - 1, ctx);
  return false;
}

bool
handle_post_answers_4(http_request_context_t *ctx,
                      db_pool_t *db,
                      db_task_t *task,
                      http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  answer_submit_state *state = ctx->request->app_data;
  answer_t *answers;
  size_t count;
  bool bound = bind_answers(task->result, &answers, &count);
  free_answer_task(task);

  if (!bound)
  {
    return true;
  }

  response->status = HTTP_STATUS_NOT_FOUND;

  for (size_t i = 0; i < count; i++)
  {
    if (answers[i].id != state->insert_id)
    {
      continue;
    }

    response->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    answer_to_json(&answers[i], &state->response);

    if (state->response.ptr)
    {
      *response = (http_response_t){
          .status = HTTP_STATUS_OK,
          .body = state->response.ptr,
          .body_len = state->response.len,
          .content_type = "application/json",
      };
    }

    break;
  }

  free_answers(answers, count);
  return true;
}

bool
handle_post_answers_5(http_request_context_t *ctx,
                      db_pool_t *db,
                      db_task_t *task,
                      http_response_t *response)
{
  assert(ctx->current_handler->next == NULL);
  answer_submit_state *state = ctx->request->app_data;
  http_status status = find_answer_challenge(task->result, state->input.challenge_id);
  free_answer_task(task);
  *response = (http_response_t){
      .status = status == HTTP_STATUS_NOT_FOUND ? HTTP_STATUS_NOT_FOUND
                                                : HTTP_STATUS_INTERNAL_SERVER_ERROR,
  };
  return true;
}

bool
handle_get_answers_1(http_request_context_t *ctx,
                     db_pool_t *db,
                     db_task_t *task,
                     http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  int challenge_id;

  if (!answer_filter(ctx->request, &challenge_id))
  {
    response->status = HTTP_STATUS_BAD_REQUEST;
    return true;
  }

  answer_list_state *state = calloc(1, sizeof(*state));

  if (!state)
  {
    return true;
  }

  state->challenge_id = challenge_id;
  ctx->request->app_data = state;
  ctx->request->dispose_app_data = free_list_state;
  const char query[] = "SELECT id FROM challenges";
  ctx->current_handler = ctx->current_handler->next;
  db_pool_exec_query(db, query, sizeof(query) - 1, ctx);
  return false;
}

bool
handle_get_answers_2(http_request_context_t *ctx,
                     db_pool_t *db,
                     db_task_t *task,
                     http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  answer_list_state *state = ctx->request->app_data;
  http_status status = find_answer_challenge(task->result, state->challenge_id);
  free_answer_task(task);
  *response = (http_response_t){.status = status};

  if (status != HTTP_STATUS_OK)
  {
    return true;
  }

  const char query[] = "SELECT id, challenge_id, user_id, answer, is_correct, "
                       "DATE_FORMAT(created_at, '%Y-%m-%dT%H:%i:%sZ') FROM answers ORDER BY id";
  ctx->current_handler = ctx->current_handler->next;
  db_pool_exec_query(db, query, sizeof(query) - 1, ctx);
  return false;
}

bool
handle_get_answers_3(http_request_context_t *ctx,
                     db_pool_t *db,
                     db_task_t *task,
                     http_response_t *response)
{
  assert(ctx->current_handler->next == NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  answer_list_state *state = ctx->request->app_data;
  answer_t *answers;
  size_t count;
  bool bound = bind_answers(task->result, &answers, &count);
  free_answer_task(task);

  if (!bound)
  {
    return true;
  }

  answer_t *selected = count ? calloc(count, sizeof(*selected)) : NULL;

  if (count && !selected)
  {
    free_answers(answers, count);
    return true;
  }

  size_t selected_count = 0;

  for (size_t i = 0; i < count; i++)
  {
    if (answers[i].is_corrected &&
        (!state->challenge_id || answers[i].challenge_id == state->challenge_id))
    {
      selected[selected_count++] = answers[i];
    }
  }

  answers_to_json(selected, selected_count, &state->response, true);
  free(selected); // Shallow views; strings belong to answers.
  free_answers(answers, count);

  if (state->response.ptr)
  {
    *response = (http_response_t){
        .status = HTTP_STATUS_OK,
        .body = state->response.ptr,
        .body_len = state->response.len,
        .content_type = "application/json",
    };
  }

  return true;
}

bool
handle_get_own_answers_1(http_request_context_t *ctx,
                         db_pool_t *db,
                         db_task_t *task,
                         http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  int challenge_id;

  if (!answer_filter(ctx->request, &challenge_id))
  {
    response->status = HTTP_STATUS_BAD_REQUEST;
    return true;
  }

  answer_list_state *state = calloc(1, sizeof(*state));

  if (!state)
  {
    return true;
  }

  state->challenge_id = challenge_id;
  ctx->request->app_data = state;
  ctx->request->dispose_app_data = free_list_state;
  const char query[] = "SELECT id FROM challenges";
  ctx->current_handler = ctx->current_handler->next;
  db_pool_exec_query(db, query, sizeof(query) - 1, ctx);
  return false;
}

bool
handle_get_own_answers_2(http_request_context_t *ctx,
                         db_pool_t *db,
                         db_task_t *task,
                         http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  answer_list_state *state = ctx->request->app_data;
  http_status status = find_answer_challenge(task->result, state->challenge_id);
  free_answer_task(task);
  *response = (http_response_t){.status = status};

  if (status != HTTP_STATUS_OK)
  {
    return true;
  }

  const char query[] = "SELECT id, challenge_id, user_id, answer, is_correct, "
                       "DATE_FORMAT(created_at, '%Y-%m-%dT%H:%i:%sZ') FROM answers ORDER BY id";
  ctx->current_handler = ctx->current_handler->next;
  db_pool_exec_query(db, query, sizeof(query) - 1, ctx);
  return false;
}

bool
handle_get_own_answers_3(http_request_context_t *ctx,
                         db_pool_t *db,
                         db_task_t *task,
                         http_response_t *response)
{
  assert(ctx->current_handler->next == NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  answer_list_state *state = ctx->request->app_data;
  answer_t *answers;
  size_t count;
  bool bound = bind_answers(task->result, &answers, &count);
  free_answer_task(task);

  if (!bound)
  {
    return true;
  }

  answer_t *selected = count ? calloc(count, sizeof(*selected)) : NULL;

  if (count && !selected)
  {
    free_answers(answers, count);
    return true;
  }

  size_t selected_count = 0;

  for (size_t i = 0; i < count; i++)
  {
    if (string_equals_cstr(answers[i].user_id, "dummy") &&
        (!state->challenge_id || answers[i].challenge_id == state->challenge_id))
    {
      selected[selected_count++] = answers[i];
    }
  }

  answers_to_json(selected, selected_count, &state->response, false);
  free(selected); // Shallow views; strings belong to answers.
  free_answers(answers, count);

  if (state->response.ptr)
  {
    *response = (http_response_t){
        .status = HTTP_STATUS_OK,
        .body = state->response.ptr,
        .body_len = state->response.len,
        .content_type = "application/json",
    };
  }

  return true;
}
