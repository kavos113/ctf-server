#include "handler.h"

#include <assert.h>
#include <limits.h>
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

bool
handle_post_challenges_1(struct http_request_context *ctx,
                         db_pool_t *db,
                         db_task_t *task,
                         http_response_t *out_response)
{
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
  const db_param_t params[] = {
      {
          .type = DB_PARAM_STRING,
          .value.string = {.ptr = "dummy", .len = 5},
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
