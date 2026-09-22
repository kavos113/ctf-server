#include "handler.h"
#include "json.h"
#include "repository.h"

#include <assert.h>
#include <stdlib.h>

bool
handle_get_users_1(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  assert(ctx->current_handler->next != NULL);
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  const char query[] = "SELECT u.id,u.username,COUNT(DISTINCT a.challenge_id)*100 AS score "
                       "FROM users u LEFT JOIN answers a ON a.user_id=u.id AND a.is_correct=1 "
                       "GROUP BY u.id,u.username ORDER BY score DESC,u.id ASC";
  _Static_assert(sizeof(query) <= DEFAULT_QUERY_SIZE, "user scores SELECT exceeds DB query buffer");

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
  *response = (http_response_t){.status = HTTP_STATUS_INTERNAL_SERVER_ERROR};
  public_user_t *users;
  size_t count;
  bool bound = bind_public_users(task->result, &users, &count);
  db_task_free(task);

  if (!bound)
  {
    return true;
  }

  string_t json;
  public_users_to_json(users, count, &json);
  free_public_users(users, count);

  if (!json.ptr)
  {
    return true;
  }

  ctx->request->app_data = json.ptr;
  ctx->request->dispose_app_data = free;
  *response = (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = json.ptr,
      .body_len = json.len,
      .content_type = "application/json",
  };
  return true;
}
