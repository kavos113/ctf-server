#include "handler.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "repository.h"

bool
handle_get_challenges_1(struct http_request_context *ctx, db_pool_t *db, db_task_t *task, http_response_t *out_response)
{
  assert(ctx->current_handler->next != NULL);
  ctx->current_handler = ctx->current_handler->next;

  db_pool_exec_query(db, "SELECT id, creator_id, name, description, genre FROM challenges;", 64, (void *)ctx);
  return false;
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
    return true;
  }

  MYSQL_RES *res = task->result->res;

  size_t rows;
  challenge_t *challenges = bind_challenges_without_flag(task->result, &rows);
  if (!challenges)
  {
    *out_response = (http_response_t){
        .status = HTTP_STATUS_OK,
        .body = "[]",
        .body_len = 2,
    };
    mysql_free_result(res);
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
    mysql_free_result(res);
    free(challenges);
    return true;
  }

  *out_response = (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = json_str.ptr,
      .body_len = json_str.len,
  };

  mysql_free_result(res);
  free(challenges);

  return true;
}
