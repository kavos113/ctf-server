#include "handler.h"
#include "handler_auth.h"
#include "json.h"
#include "repository.h"
#include "score.h"

#include <assert.h>
#include <stdlib.h>

typedef struct
{
  public_user_t *users;
  size_t count;
  string_t response;
} user_list_state;

static void
free_user_list(void *data)
{
  user_list_state *state = data;
  free_public_users(state->users, state->count);
  free(state->response.ptr);
  free(state);
}

bool
handle_get_users_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  user_list_state *state = calloc(1, sizeof(*state));

  if (!state)
  {
    return true;
  }

  ctx->request->app_data = state;
  ctx->request->dispose_app_data = free_user_list;
  const char query[] = "SELECT id,username FROM users";

  if (db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0)
  {
    return true;
  }

  ctx->current_handler = ctx->current_handler->next;
  return false;
}

bool
handle_get_users_2(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  user_list_state *state = ctx->request->app_data;
  bool bound = bind_public_users(task->result, &state->users, &state->count);
  db_task_free(task);

  if (!bound)
  {
    return true;
  }

  const char query[] = "SELECT user_id,challenge_id,TIMESTAMPDIFF(SECOND,'1970-01-01',created_at) "
                       "FROM answers WHERE is_correct=1";

  if (db_pool_exec_query(db, query, sizeof(query) - 1, ctx) < 0)
  {
    return true;
  }

  ctx->current_handler = ctx->current_handler->next;
  return false;
}

bool
handle_get_users_3(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  user_list_state *state = ctx->request->app_data;
  score_answer_t *answers;
  size_t count;
  bool bound = bind_score_answers(task->result, &answers, &count);
  db_task_free(task);

  if (!bound)
  {
    return true;
  }

  const auth_runtime_t *runtime = ctx->app_context;
  calculate_user_scores(state->users, state->count, answers, count,
                        runtime && runtime->contest_end_enabled, runtime ? runtime->contest_end_at : 0);
  free(answers);
  public_users_to_json(state->users, state->count, &state->response);

  if (!state->response.ptr)
  {
    return true;
  }

  *response = (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = state->response.ptr,
      .body_len = state->response.len,
      .content_type = "application/json",
  };
  return true;
}
