#include "test.h"
#include "util.h"

#include <app/handler.h>
#include <app/json.h>
#include <limits.h>

#define UPDATE_JSON                                                                                \
  "{\"name\":\"new\\\"name\",\"description\":\"\",\"flag\":\"flag\\n\",\"genre\":\"web\"}"

typedef enum
{
  RESULT_OWN,
  RESULT_OTHER,
  RESULT_MISSING,
  RESULT_DB_ERROR,
  RESULT_BAD_SHAPE,
  RESULT_NULL_OWNER,
  RESULT_INVALID_ID,
} result_kind;

#include "mysql_result_mock.h"

typedef test_mysql_rows mock_rows;

static int fail_malloc_after = -1;
void *__real_malloc(size_t size);

void
test_set_malloc_failure(int after)
{
  fail_malloc_after = after;
}

void *
__wrap_malloc(size_t size)
{
  if (fail_malloc_after == 0)
  {
    return NULL;
  }

  if (fail_malloc_after > 0)
  {
    fail_malloc_after--;
  }

  return __real_malloc(size);
}

static MYSQL_RES *
make_result(result_kind kind)
{
  mock_rows *rows = calloc(1, sizeof(*rows));
  rows->fields = kind == RESULT_BAD_SHAPE ? 1 : 2;
  rows->count = kind == RESULT_MISSING ? 0 : 2;

  if (rows->count)
  {
    rows->rows[0][0] = string_from_cstr_dup("2").ptr;
    rows->rows[0][1] = string_from_cstr_dup("other").ptr;
    rows->rows[1][0] = string_from_cstr_dup(kind == RESULT_INVALID_ID ? "invalid" : "1").ptr;

    if (kind != RESULT_NULL_OWNER)
    {
      rows->rows[1][1] = string_from_cstr_dup(kind == RESULT_OTHER ? "other" : "dummy").ptr;
    }
  }

  test_mysql_live_results++;
  return (MYSQL_RES *)rows;
}

typedef struct
{
  const char *name;
  const char *id;
  const char *body;
  bool duplicate_id;
  result_kind result;
  bool zero_affected;
  bool excess_affected;
  bool fail_calloc;
  int calloc_after;
  bool fail_malloc;
  bool complete;
  http_status status;
} write_case;

static void
check_queued_write(test_ctx_t *ctx, const char *name, db_task_t *task, bool put)
{
  ASSERT_NOT_NULL(name, task);

  if (!task)
  {
    return;
  }

  ASSERT_TRUE(name, task->is_param_query);
  ASSERT_EQ(name, put ? (size_t)6 : (size_t)2, task->param_count);

  if (put)
  {
    ASSERT_STR_EQ(name,
                  "UPDATE challenges SET name = ?, description = ?, flag = ?, genre = ? "
                  "WHERE id = ? AND creator_id = ?",
                  task->query);
    ASSERT_EQ(name, (db_param_type)DB_PARAM_STRING, task->params[0].type);
    ASSERT_STR_EQ(name, "new\\\"name", task->params[0].value.string.ptr);
    ASSERT_EQ(name, (size_t)0, task->params[1].value.string.len);
    ASSERT_STR_EQ(name, "flag\\n", task->params[2].value.string.ptr);
    ASSERT_EQ(name, (int64_t)CTF_GENRE_WEB, task->params[3].value.integer);
    ASSERT_EQ(name, (int64_t)1, task->params[4].value.integer);
    ASSERT_STR_EQ(name, "dummy", task->params[5].value.string.ptr);
  }
  else
  {
    ASSERT_STR_EQ(name, "DELETE FROM challenges WHERE id = ? AND creator_id = ?", task->query);
    ASSERT_EQ(name, (int64_t)1, task->params[0].value.integer);
    ASSERT_STR_EQ(name, "dummy", task->params[1].value.string.ptr);
  }
}

static void
run_write_cases(test_ctx_t *ctx, bool put, size_t stage, const write_case *cases, size_t count)
{
  for (size_t i = 0; i < count; i++)
  {
    ctx->is_canceled = false;
    const write_case *tc = &cases[i];
    http_handler_t handlers[4] = {
        {.func = put ? handle_put_challenges_1 : handle_delete_challenges_1},
        {.func = put ? handle_put_challenges_2 : handle_delete_challenges_2},
        {.func = put ? handle_put_challenges_3 : handle_delete_challenges_3},
        {.func = put ? handle_put_challenges_4 : handle_delete_challenges_4},
    };

    for (size_t j = 0; j < 3; j++)
    {
      handlers[j].next = &handlers[j + 1];
    }

    db_pool_t pool = {.task_queue = task_queue_new()};
    http_request_t *request = calloc(1, sizeof(*request));
    const char *id = stage == 1 ? tc->id : "1";
    char *id_bytes = NULL;

    if (id)
    {
      size_t len = strlen(id);
      id_bytes = malloc(len ? len : 1);
      memcpy(id_bytes, id, len);
      request->query_params[0] = (http_param_t){
          .name = "id",
          .name_len = 2,
          .value = id_bytes,
          .value_len = len,
      };
      request->query_param_count = tc->duplicate_id ? 2 : 1;
      request->query_params[1] = request->query_params[0];
    }

    const char *body = stage == 1 ? tc->body : UPDATE_JSON;
    char *body_bytes = NULL;

    if (body)
    {
      request->content_length = strlen(body);
      body_bytes = malloc(request->content_length ? request->content_length : 1);
      memcpy(body_bytes, body, request->content_length);
      request->body = body_bytes;
    }

    http_request_context_t context = {.request = request, .current_handler = &handlers[0]};
    http_response_t response;
    bool complete = false;

    for (size_t current = 1; current <= stage; current++)
    {
      db_task_t *task = NULL;

      if (current > 1)
      {
        ASSERT_NOT_NULL(tc->name, pool.task_queue->head);

        if (!pool.task_queue->head)
        {
          break;
        }

        task = task_queue_pop(pool.task_queue);
        ASSERT_TRUE(tc->name, task->data == &context);
        result_kind kind = current == stage ? tc->result : RESULT_OWN;

        if (current == 3)
        {
          check_queued_write(ctx, tc->name, task, put);
          task->result->affected = stage == 4 || tc->zero_affected ? 0
                                   : tc->excess_affected           ? 2
                                                                   : 1;
        }
        else
        {
          ASSERT_TRUE(tc->name, !task->is_param_query);
          ASSERT_STR_EQ(tc->name, "SELECT id, creator_id FROM challenges", task->query);
          task->result = calloc(1, sizeof(*task->result));
          task->result->res = make_result(kind);
        }

        task->result->success = kind != RESULT_DB_ERROR;
      }

      if (current == stage && tc->fail_calloc)
      {
        test_set_calloc_failure(tc->calloc_after);
      }

      if (current == stage && tc->fail_malloc)
      {
        fail_malloc_after = 0;
      }

      ASSERT_TRUE(tc->name, context.current_handler == &handlers[current - 1]);
      complete = context.current_handler->func(&context, &pool, task, &response);
      test_set_calloc_failure(-1);
      fail_malloc_after = -1;

      if (current < stage)
      {
        ASSERT_TRUE(tc->name, !complete);
      }

      if (complete)
      {
        ASSERT_TRUE(tc->name, context.current_handler == &handlers[current - 1]);
        break;
      }

      ASSERT_TRUE(tc->name, current < 4);
      ASSERT_TRUE(tc->name, context.current_handler == &handlers[current]);
    }

    ASSERT_EQ(tc->name, tc->complete, complete);
    ASSERT_EQ(tc->name, 0, test_mysql_live_results);

    if (complete)
    {
      ASSERT_EQ(tc->name, tc->status, response.status);
      ASSERT_NULL(tc->name, pool.task_queue->head);

      if (tc->status == HTTP_STATUS_OK && put)
      {
        ASSERT_NOT_NULL(tc->name, response.body);

        if (response.body)
        {
          ASSERT_STR_EQ(tc->name, "application/json", response.content_type);
          ASSERT_EQ(tc->name, strlen(response.body), response.body_len);
          ASSERT_STR_EQ(tc->name,
                        "{\"id\":1,\"creator_id\":\"dummy\",\"name\":\"new\\\"name\","
                        "\"description\":\"\",\"flag\":\"flag\\n\",\"genre\":\"web\"}",
                        response.body);
        }
      }
      else
      {
        ASSERT_EQ(tc->name, (size_t)0, response.body_len);
      }
    }
    else if (stage == 2)
    {
      // Parameters must be owned by the DB task, not the request body.
      memset(body_bytes, 'x', request->content_length);
      check_queued_write(ctx, tc->name, pool.task_queue->head, put);
    }
    else
    {
      ASSERT_NOT_NULL(tc->name, pool.task_queue->head);

      if (pool.task_queue->head)
      {
        ASSERT_STR_EQ(
            tc->name, "SELECT id, creator_id FROM challenges", pool.task_queue->head->query);
      }
    }

    http_request_dispose(request);
    free(id_bytes);
    free(body_bytes);
    task_queue_free(pool.task_queue);
    CHECK_TEST(tc->name);
  }
}

static void
test_handle_put_challenges_1(test_ctx_t *ctx)
{
  const write_case cases[] = {
      {.name = "valid request", .id = "1", .body = UPDATE_JSON, .complete = false},
      {.name = "maximum ID", .id = "2147483647", .body = UPDATE_JSON, .complete = false},
      {.name = "missing ID",
       .id = NULL,
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "empty ID",
       .id = "",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "zero ID",
       .id = "0",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "negative ID",
       .id = "-1",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "overflow ID",
       .id = "2147483648",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "huge ID",
       .id = "999999999999999999999999",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "leading whitespace",
       .id = " 1",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "trailing whitespace",
       .id = "1 ",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "fractional ID",
       .id = "1.0",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "trailing bytes",
       .id = "1x",
       .body = UPDATE_JSON,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "duplicate ID",
       .id = "1",
       .body = UPDATE_JSON,
       .duplicate_id = true,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "state allocation failure",
       .id = "1",
       .body = UPDATE_JSON,
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "missing body", .id = "1", .complete = true, .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid body",
       .id = "1",
       .body = "{}",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "parser allocation failure",
       .id = "1",
       .body = UPDATE_JSON,
       .fail_calloc = true,
       .calloc_after = 1,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_write_cases(ctx, true, 1, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_put_challenges_2(test_ctx_t *ctx)
{
  const write_case cases[] = {
      {.name = "queue write", .complete = false},
      {.name = "other owner",
       .result = RESULT_OTHER,
       .complete = true,
       .status = HTTP_STATUS_FORBIDDEN},
      {.name = "missing challenge",
       .result = RESULT_MISSING,
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "SELECT failure",
       .result = RESULT_DB_ERROR,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "bad result shape",
       .result = RESULT_BAD_SHAPE,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "null owner",
       .result = RESULT_NULL_OWNER,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "invalid database ID",
       .result = RESULT_INVALID_ID,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "write allocation failure 0",
       .fail_calloc = true,
       .calloc_after = 0,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "write allocation failure 1",
       .fail_calloc = true,
       .calloc_after = 1,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_write_cases(ctx, true, 2, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_put_challenges_3(test_ctx_t *ctx)
{
  const write_case cases[] = {
      {.name = "write success", .complete = true, .status = HTTP_STATUS_OK},
      {.name = "zero affected requires SELECT", .zero_affected = true, .complete = false},
      {.name = "write failure",
       .result = RESULT_DB_ERROR,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "unexpected affected count",
       .excess_affected = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "JSON allocation failure",
       .fail_malloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_write_cases(ctx, true, 3, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_put_challenges_4(test_ctx_t *ctx)
{
  const write_case cases[] = {
      {.name = "same value update", .complete = true, .status = HTTP_STATUS_OK},
      {.name = "concurrent delete",
       .result = RESULT_MISSING,
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "concurrent owner change",
       .result = RESULT_OTHER,
       .complete = true,
       .status = HTTP_STATUS_FORBIDDEN},
      {.name = "recheck failure",
       .result = RESULT_DB_ERROR,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "JSON allocation failure",
       .fail_malloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_write_cases(ctx, true, 4, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_delete_challenges_1(test_ctx_t *ctx)
{
  const write_case cases[] = {
      {.name = "valid request", .id = "1", .complete = false},
      {.name = "maximum ID", .id = "2147483647", .complete = false},
      {.name = "missing ID", .id = NULL, .complete = true, .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "empty ID", .id = "", .complete = true, .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "zero ID", .id = "0", .complete = true, .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "negative ID", .id = "-1", .complete = true, .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "overflow ID",
       .id = "2147483648",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "huge ID",
       .id = "999999999999999999999999",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "leading whitespace",
       .id = " 1",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "trailing whitespace",
       .id = "1 ",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "fractional ID", .id = "1.0", .complete = true, .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "trailing bytes", .id = "1x", .complete = true, .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "duplicate ID",
       .id = "1",
       .duplicate_id = true,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "state allocation failure",
       .id = "1",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_write_cases(ctx, false, 1, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_delete_challenges_2(test_ctx_t *ctx)
{
  const write_case cases[] = {
      {.name = "queue write", .complete = false},
      {.name = "other owner",
       .result = RESULT_OTHER,
       .complete = true,
       .status = HTTP_STATUS_FORBIDDEN},
      {.name = "missing challenge",
       .result = RESULT_MISSING,
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "SELECT failure",
       .result = RESULT_DB_ERROR,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "bad result shape",
       .result = RESULT_BAD_SHAPE,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "null owner",
       .result = RESULT_NULL_OWNER,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "invalid database ID",
       .result = RESULT_INVALID_ID,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "write allocation failure 0",
       .fail_calloc = true,
       .calloc_after = 0,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "write allocation failure 1",
       .fail_calloc = true,
       .calloc_after = 1,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_write_cases(ctx, false, 2, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_delete_challenges_3(test_ctx_t *ctx)
{
  const write_case cases[] = {
      {.name = "write success", .complete = true, .status = HTTP_STATUS_OK},
      {.name = "zero affected requires SELECT", .zero_affected = true, .complete = false},
      {.name = "write failure",
       .result = RESULT_DB_ERROR,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "unexpected affected count",
       .excess_affected = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_write_cases(ctx, false, 3, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_delete_challenges_4(test_ctx_t *ctx)
{
  const write_case cases[] = {
      {.name = "unexpected remaining row",
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "concurrent delete",
       .result = RESULT_MISSING,
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "concurrent owner change",
       .result = RESULT_OTHER,
       .complete = true,
       .status = HTTP_STATUS_FORBIDDEN},
      {.name = "recheck failure",
       .result = RESULT_DB_ERROR,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_write_cases(ctx, false, 4, cases, sizeof(cases) / sizeof(cases[0]));
}

void
test_app_challenge_write(test_ctx_t *ctx)
{
  test_handle_put_challenges_1(ctx);
  test_handle_put_challenges_2(ctx);
  test_handle_put_challenges_3(ctx);
  test_handle_put_challenges_4(ctx);
  test_handle_delete_challenges_1(ctx);
  test_handle_delete_challenges_2(ctx);
  test_handle_delete_challenges_3(ctx);
  test_handle_delete_challenges_4(ctx);
}
