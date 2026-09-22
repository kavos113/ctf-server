#include "mysql_result_mock.h"
#include "test.h"
#include "util.h"

#include <app/handler.h>
#include <app/handler_auth.h>
#include <app/json.h>
#include <app/json_p.h>
#include <app/repository.h>
#include <limits.h>
#include <stdint.h>

#define ANSWER_JSON "{\"challenge_id\":1,\"answer\":\"flag\"}"
#define DATE        "2026-09-22T12:34:56Z"

typedef enum
{
  POST_ANSWER,
  GET_ANSWERS,
  GET_OWN_ANSWERS,
} test_route;

typedef struct
{
  const char *name;
  const char *body;
  bool missing_body;
  const char *filter;
  bool duplicate_filter;
  bool empty;
  bool db_error;
  bool invalid_shape;
  bool invalid_flag;
  bool escaped_flag;
  bool incorrect;
  bool no_insert_id;
  bool overflow_insert_id;
  bool zero_affected;
  bool fail_calloc;
  int calloc_after;
  bool fail_malloc;
  int malloc_after;
  bool complete;
  http_status status;
  size_t entries;
} answer_case;

static MYSQL_RES *
answer_fixture(bool empty)
{
  const char *values[][6] = {
      {"10", "1", "0123456789abcdef0123456789abcdef", "wrong", "0", DATE},
      {"11", "1", "other", "flag", "1", DATE},
      {"12", "1", "0123456789abcdef0123456789abcdef", "flag", "1", DATE},
      {"13", "2", "0123456789abcdef0123456789abcdef", "second", "1", DATE},
      {"14", "1", "0123456789abcdef0123456789abcdef", "flag", "1", DATE},
  };
  MYSQL_RES *result = test_mysql_result(values, empty ? 0 : 5, 6);
  test_mysql_rows *rows = (test_mysql_rows *)result;
  rows->fields = 7;

  for (size_t i = 0; i < rows->count; i++)
  {
    rows->rows[i][6] = string_from_cstr_dup(i == 1 ? "Bob" : "Alice").ptr;
  }

  return result;
}

static MYSQL_RES *
challenge_fixture(bool empty, const char *flag)
{
  const char *values[][6] = {
      {"1", "0123456789abcdef0123456789abcdef", "first", "description", flag, "0"},
      {"2", "other", "second", "description", "second", "1"},
  };
  return test_mysql_result(values, empty ? 0 : 2, 6);
}

static void
check_answer_insert(test_ctx_t *ctx, const answer_case *tc, db_task_t *task, const char *body)
{
  ASSERT_NOT_NULL(tc->name, task);

  if (!task)
  {
    return;
  }

  ASSERT_TRUE(tc->name, task->is_param_query);
  ASSERT_EQ(tc->name, (size_t)4, task->param_count);
  ASSERT_STR_EQ(tc->name,
                "INSERT INTO answers (challenge_id, user_id, answer, is_correct, created_at) "
                "VALUES (?, ?, ?, ?, UTC_TIMESTAMP())",
                task->query);
  ASSERT_EQ(tc->name, (int64_t)1, task->params[0].value.integer);
  ASSERT_STR_EQ(tc->name, "0123456789abcdef0123456789abcdef", task->params[1].value.string.ptr);
  ASSERT_EQ(tc->name, (int64_t)!tc->incorrect, task->params[3].value.integer);
  submit_answer_request_t input;
  ASSERT_EQ(tc->name, 0, json_to_submit_answer_request(body, strlen(body), &input));
  ASSERT_TRUE(tc->name,
              string_equals(input.answer,
                            string_from_cstr_n(task->params[2].value.string.ptr,
                                               task->params[2].value.string.len)));
}

static void
run_answer_cases(
    test_ctx_t *ctx, test_route route, size_t target, const answer_case *cases, size_t count)
{
  for (size_t i = 0; i < count; i++)
  {
    ctx->is_canceled = false;
    const answer_case *tc = &cases[i];
    http_handler_t handlers[5] = {0};
    size_t handler_count = route == POST_ANSWER ? 5 : 3;

    if (route == POST_ANSWER)
    {
      handlers[0].func = handle_post_answers_1;
      handlers[1].func = handle_post_answers_2;
      handlers[2].func = handle_post_answers_3;
      handlers[3].func = handle_post_answers_4;
      handlers[4].func = handle_post_answers_5;
    }
    else if (route == GET_ANSWERS)
    {
      handlers[0].func = handle_get_answers_1;
      handlers[1].func = handle_get_answers_2;
      handlers[2].func = handle_get_answers_3;
    }
    else
    {
      handlers[0].func = handle_get_own_answers_1;
      handlers[1].func = handle_get_own_answers_2;
      handlers[2].func = handle_get_own_answers_3;
    }

    for (size_t j = 0; j + 1 < handler_count; j++)
    {
      handlers[j].next = &handlers[j + 1];
    }

    db_pool_t pool = {.task_queue = task_queue_new()};
    http_request_t *request = calloc(1, sizeof(*request));
    auth_identity_t identity = {.claims.user_id = "0123456789abcdef0123456789abcdef", .verified = true};
    request->auth_data = &identity;
    const char *body = tc->body ? tc->body : ANSWER_JSON;
    char *input = NULL;

    if (!tc->missing_body)
    {
      request->content_length = strlen(body);
      input = malloc(request->content_length);
      memcpy(input, body, request->content_length);
      request->body = input;
    }

    if (tc->filter)
    {
      request->query_params[0] = (http_param_t){
          .name = "challenge_id",
          .name_len = sizeof("challenge_id") - 1,
          .value = tc->filter,
          .value_len = strlen(tc->filter),
      };
      request->query_params[1] = request->query_params[0];
      request->query_param_count = tc->duplicate_filter ? 2 : 1;
    }

    http_request_context_t context = {.request = request, .current_handler = &handlers[0]};
    http_response_t response;
    bool complete = false;
    char *saved_answer = NULL;
    int saved_correct = 0;
    size_t current = 1;

    while (current <= target)
    {
      db_task_t *task = NULL;
      bool final_stage = current == target;

      if (current > 1)
      {
        ASSERT_NOT_NULL(tc->name, pool.task_queue->head);

        if (!pool.task_queue->head)
        {
          break;
        }

        task = task_queue_pop(pool.task_queue);
        ASSERT_TRUE(tc->name, task->data == &context);

        if (task->is_param_query)
        {
          check_answer_insert(ctx, tc, task, body);
          task->result->affected = final_stage && tc->zero_affected ? 0 : 1;
          task->result->insert_id = final_stage && tc->no_insert_id         ? 0
                                    : final_stage && tc->overflow_insert_id ? UINT64_MAX
                                                                            : 12;
          saved_answer = string_from_cstr_dup(task->params[2].value.string.ptr).ptr;
          saved_correct = (int)task->params[3].value.integer;
        }
        else
        {
          task->result = calloc(1, sizeof(*task->result));
          ASSERT_NULL(tc->name, strstr(task->query, "WHERE"));

          if (strstr(task->query, "FROM answers"))
          {
            ASSERT_STR_EQ(tc->name,
                          "SELECT a.id,a.challenge_id,a.user_id,a.answer,a.is_correct,"
                          "DATE_FORMAT(a.created_at,'%Y-%m-%dT%H:%i:%sZ'),u.username "
                          "FROM answers a JOIN users u ON u.id=a.user_id ORDER BY a.id",
                          task->query);
            task->result->res = answer_fixture(final_stage && tc->empty);
            test_mysql_rows *rows = (test_mysql_rows *)task->result->res;

            if (saved_answer && rows->count)
            {
              free(rows->rows[2][3]);
              rows->rows[2][3] = string_from_cstr_dup(saved_answer).ptr;
              rows->rows[2][4][0] = saved_correct ? '1' : '0';
            }
          }
          else
          {
            ASSERT_STR_EQ(tc->name,
                          "SELECT id, creator_id, name, description, flag, genre FROM challenges",
                          task->query);
            const char *flag = final_stage && tc->invalid_flag ? "\\ud800"
                               : tc->escaped_flag              ? "fl\\u0061g"
                                                               : "flag";
            task->result->res = challenge_fixture(final_stage && tc->empty, flag);
          }

          if (final_stage && tc->invalid_shape)
          {
            ((test_mysql_rows *)task->result->res)->fields = 0;
          }
        }

        task->result->success = !((final_stage && tc->db_error) ||
                                  (route == POST_ANSWER && target == 5 && current == 3));
      }

      if (final_stage && tc->fail_calloc)
      {
        test_set_calloc_failure(tc->calloc_after);
      }

      if (final_stage && tc->fail_malloc)
      {
        test_set_malloc_failure(tc->malloc_after);
      }

      ASSERT_TRUE(tc->name, context.current_handler == &handlers[current - 1]);
      complete = context.current_handler->func(&context, &pool, task, &response);
      test_set_calloc_failure(-1);
      test_set_malloc_failure(-1);

      if (complete)
      {
        ASSERT_TRUE(tc->name, final_stage);
        ASSERT_TRUE(tc->name, context.current_handler == &handlers[current - 1]);
        break;
      }

      size_t next =
          route == POST_ANSWER && current == 3 && (target == 5 || (target == 3 && tc->db_error))
              ? 5
              : current + 1;
      ASSERT_TRUE(tc->name, next <= handler_count);
      ASSERT_TRUE(tc->name, context.current_handler == &handlers[next - 1]);
      current = next;
    }

    ASSERT_EQ(tc->name, tc->complete, complete);
    ASSERT_EQ(tc->name, 0, test_mysql_live_results);

    if (complete)
    {
      ASSERT_EQ(tc->name, tc->status, response.status);
      ASSERT_NULL(tc->name, pool.task_queue->head);

      if (response.status == HTTP_STATUS_OK)
      {
        ASSERT_NOT_NULL(tc->name, response.body);

        if (response.body)
        {
          ASSERT_STR_EQ(tc->name, "application/json", response.content_type);
          ASSERT_EQ(tc->name, strlen(response.body), response.body_len);
          ASSERT_NULL(tc->name, strstr(response.body, "created_at"));
          ASSERT_NULL(tc->name, strstr(response.body, "\"user_id\""));

          if (response.body_len > 2)
          {
            ASSERT_NOT_NULL(tc->name, strstr(response.body, "\"username\":"));
          }

          if (route == POST_ANSWER)
          {
            ASSERT_NOT_NULL(tc->name, strstr(response.body, saved_answer));
            ASSERT_NOT_NULL(tc->name, strstr(response.body, "\"username\":\"Alice\""));
            ASSERT_NOT_NULL(
                tc->name,
                strstr(response.body, saved_correct ? "\"correct\":true" : "\"correct\":false"));
            ASSERT_NOT_NULL(tc->name, strstr(response.body, DATE));
          }
          else
          {
            size_t entries = 0;
            const char *entry = response.body;

            while ((entry = strstr(entry, "\"challenge_id\":")))
            {
              entries++;
              entry++;
            }

            ASSERT_EQ(tc->name, tc->entries, entries);

            if (route == GET_ANSWERS)
            {
              ASSERT_NULL(tc->name, strstr(response.body, "\"answer\""));
              ASSERT_NULL(tc->name, strstr(response.body, "\"correct\""));
            }
            else
            {
              ASSERT_NULL(tc->name, strstr(response.body, "\"username\":\"Bob\""));

              if (tc->entries && (!tc->filter || strcmp(tc->filter, "1") == 0))
              {
                ASSERT_NOT_NULL(tc->name,
                                strstr(response.body, "\"answer\":\"wrong\",\"correct\":false"));
              }
            }
          }
        }
      }
    }
    else
    {
      ASSERT_NOT_NULL(tc->name, pool.task_queue->head);

      if (route == POST_ANSWER && target == 2)
      {
        // INSERT parameters must survive changes to the original borrowed body.
        memset(input, 'x', request->content_length);
        check_answer_insert(ctx, tc, pool.task_queue->head, body);
      }
    }

    free(saved_answer);
    http_request_dispose(request);
    free(input);
    task_queue_free(pool.task_queue);
    CHECK_TEST(tc->name);
  }
}

static void
test_handle_post_answers_1(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "queue SELECT", .complete = false},
      {.name = "state allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "missing body",
       .missing_body = true,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid body", .body = "{}", .complete = true, .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "parser allocation",
       .fail_calloc = true,
       .calloc_after = 1,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_answer_cases(ctx, POST_ANSWER, 1, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_post_answers_2(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "continue", .complete = false},
      {.name = "DB failure",
       .db_error = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "bad result",
       .invalid_shape = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "missing challenge",
       .empty = true,
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "escaped flag", .escaped_flag = true, .complete = false},
      {.name = "invalid stored flag",
       .invalid_flag = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "wrong answer still saved",
       .body = "{\"challenge_id\":1,\"answer\":\"wrong\"}",
       .incorrect = true,
       .complete = false},
      {.name = "challenge array allocation",
       .fail_calloc = true,
       .calloc_after = 0,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "INSERT allocation 1",
       .fail_calloc = true,
       .calloc_after = 1,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "INSERT result allocation",
       .fail_calloc = true,
       .calloc_after = 2,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "challenge string allocation",
       .fail_malloc = true,
       .malloc_after = 3,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "target absent from nonempty result",
       .body = "{\"challenge_id\":3,\"answer\":\"flag\"}",
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
  };
  run_answer_cases(ctx, POST_ANSWER, 2, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_post_answers_3(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "successful INSERT", .complete = false},
      {.name = "failed INSERT rechecks", .db_error = true, .complete = false},
      {.name = "no_insert_id",
       .no_insert_id = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "overflow_insert_id",
       .overflow_insert_id = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "zero_affected",
       .zero_affected = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_answer_cases(ctx, POST_ANSWER, 3, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_post_answers_4(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "saved correct answer", .complete = true, .status = HTTP_STATUS_OK},
      {.name = "saved incorrect answer",
       .body = "{\"challenge_id\":1,\"answer\":\"wrong\"}",
       .incorrect = true,
       .complete = true,
       .status = HTTP_STATUS_OK},
      {.name = "preserve escaped answer",
       .body = "{\"challenge_id\":1,\"answer\":\"fl\\u0061g\"}",
       .complete = true,
       .status = HTTP_STATUS_OK},
      {.name = "deleted before reread",
       .empty = true,
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "read failed",
       .db_error = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "invalid shape",
       .invalid_shape = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "array allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "string allocation",
       .fail_malloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "JSON allocation",
       .fail_malloc = true,
       .malloc_after = 15,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_answer_cases(ctx, POST_ANSWER, 4, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_post_answers_5(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "deleted challenge",
       .empty = true,
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "other INSERT failure",
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "recheck failed",
       .db_error = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "challenge binding allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "challenge binding partial allocation",
       .fail_malloc = true,
       .malloc_after = 3,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_answer_cases(ctx, POST_ANSWER, 5, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_get_answers_1(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "queue SELECT", .complete = false},
      {.name = "state allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "valid filter", .filter = "1", .complete = false},
      {.name = "invalid filter ",
       .filter = "",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter 0",
       .filter = "0",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter -1",
       .filter = "-1",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter 2147483648",
       .filter = "2147483648",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter  1",
       .filter = " 1",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter 1x",
       .filter = "1x",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter 1.0",
       .filter = "1.0",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "duplicate filter",
       .filter = "1",
       .duplicate_filter = true,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
  };
  run_answer_cases(ctx, GET_ANSWERS, 1, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_get_answers_2(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "continue", .complete = false},
      {.name = "DB failure",
       .db_error = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "bad result",
       .invalid_shape = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "missing challenge",
       .filter = "3",
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "empty challenges no filter", .empty = true, .complete = false},
      {.name = "challenge binding allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "challenge binding partial allocation",
       .fail_malloc = true,
       .malloc_after = 3,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_answer_cases(ctx, GET_ANSWERS, 2, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_get_answers_3(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "all selected including repeated solves",
       .complete = true,
       .status = HTTP_STATUS_OK,
       .entries = 4},
      {.name = "filter first challenge",
       .filter = "1",
       .complete = true,
       .status = HTTP_STATUS_OK,
       .entries = 3},
      {.name = "filter second challenge",
       .filter = "2",
       .complete = true,
       .status = HTTP_STATUS_OK,
       .entries = 1},
      {.name = "empty answer list",
       .empty = true,
       .complete = true,
       .status = HTTP_STATUS_OK,
       .entries = 0},
      {.name = "selection allocation",
       .fail_calloc = true,
       .calloc_after = 1,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "read failed",
       .db_error = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "invalid shape",
       .invalid_shape = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "array allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "string allocation",
       .fail_malloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "JSON allocation",
       .fail_malloc = true,
       .malloc_after = 15,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_answer_cases(ctx, GET_ANSWERS, 3, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_get_own_answers_1(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "queue SELECT", .complete = false},
      {.name = "state allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "valid filter", .filter = "1", .complete = false},
      {.name = "invalid filter ",
       .filter = "",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter 0",
       .filter = "0",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter -1",
       .filter = "-1",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter 2147483648",
       .filter = "2147483648",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter  1",
       .filter = " 1",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter 1x",
       .filter = "1x",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid filter 1.0",
       .filter = "1.0",
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
      {.name = "duplicate filter",
       .filter = "1",
       .duplicate_filter = true,
       .complete = true,
       .status = HTTP_STATUS_BAD_REQUEST},
  };
  run_answer_cases(ctx, GET_OWN_ANSWERS, 1, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_get_own_answers_2(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "continue", .complete = false},
      {.name = "DB failure",
       .db_error = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "bad result",
       .invalid_shape = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "missing challenge",
       .filter = "3",
       .complete = true,
       .status = HTTP_STATUS_NOT_FOUND},
      {.name = "empty challenges no filter", .empty = true, .complete = false},
      {.name = "challenge binding allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "challenge binding partial allocation",
       .fail_malloc = true,
       .malloc_after = 3,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_answer_cases(ctx, GET_OWN_ANSWERS, 2, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_get_own_answers_3(test_ctx_t *ctx)
{
  const answer_case cases[] = {
      {.name = "all selected including repeated solves",
       .complete = true,
       .status = HTTP_STATUS_OK,
       .entries = 4},
      {.name = "filter first challenge",
       .filter = "1",
       .complete = true,
       .status = HTTP_STATUS_OK,
       .entries = 3},
      {.name = "filter second challenge",
       .filter = "2",
       .complete = true,
       .status = HTTP_STATUS_OK,
       .entries = 1},
      {.name = "empty answer list",
       .empty = true,
       .complete = true,
       .status = HTTP_STATUS_OK,
       .entries = 0},
      {.name = "selection allocation",
       .fail_calloc = true,
       .calloc_after = 1,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "read failed",
       .db_error = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "invalid shape",
       .invalid_shape = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "array allocation",
       .fail_calloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "string allocation",
       .fail_malloc = true,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "JSON allocation",
       .fail_malloc = true,
       .malloc_after = 15,
       .complete = true,
       .status = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_answer_cases(ctx, GET_OWN_ANSWERS, 3, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_read_positive_integer(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *input;
    bool success;
    int value;
    size_t consumed;
  } cases[] = {
      {.name = "one", .input = "1", .success = true, .value = 1, .consumed = 1},
      {.name = "maximum", .input = "2147483647", .success = true, .value = INT_MAX, .consumed = 10},
      {.name = "delimiter", .input = "12,", .success = true, .value = 12, .consumed = 2},
      {.name = "whitespace", .input = " 12", .success = true, .value = 12, .consumed = 3},
      {.name = "overflow", .input = "2147483648", .success = false},
      {.name = "large overflow", .input = "99999999999999999999999", .success = false},
      {.name = "zero", .input = "0", .success = false},
      {.name = "negative", .input = "-1", .success = false},
      {.name = "leading zero", .input = "01", .success = false},
      {.name = "empty", .input = "", .success = false},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    size_t len = strlen(cases[i].input);
    char *input = malloc(len ? len : 1);
    memcpy(input, cases[i].input, len);
    json_parser_t parser = {.cur = input, .end = input + len, .error = -1};
    int value = 0;
    ASSERT_EQ(cases[i].name, cases[i].success, read_positive_integer(&parser, &value));

    if (cases[i].success)
    {
      ASSERT_EQ(cases[i].name, cases[i].value, value);
      ASSERT_EQ(cases[i].name, cases[i].consumed, (size_t)(parser.cur - input));
    }

    free(input);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_json_string_equal_decoded(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *left;
    const char *right;
    int expected;
  } cases[] = {
      {.name = "literal", .left = "flag", .right = "flag", .expected = 1},
      {.name = "case matters", .left = "flag", .right = "Flag", .expected = 0},
      {.name = "no trim", .left = "flag ", .right = "flag", .expected = 0},
      {.name = "unicode escape", .left = "fl\\u0061g", .right = "flag", .expected = 1},
      {.name = "quote", .left = "\\\"", .right = "\\u0022", .expected = 1},
      {.name = "slash", .left = "\\/", .right = "/", .expected = 1},
      {.name = "backslash", .left = "\\\\", .right = "\\u005c", .expected = 1},
      {.name = "newline", .left = "\\n", .right = "\\u000a", .expected = 1},
      {.name = "controls",
       .left = "\\b\\f\\r\\t",
       .right = "\\u0008\\u000c\\u000d\\u0009",
       .expected = 1},
      {.name = "Japanese", .left = "日本", .right = "\\u65e5\\u672c", .expected = 1},
      {.name = "surrogate pair", .left = "😀", .right = "\\ud83d\\ude00", .expected = 1},
      {.name = "embedded NUL", .left = "a\\u0000b", .right = "a\\u0000b", .expected = 1},
      {.name = "NUL suffix matters", .left = "a\\u0000b", .right = "a\\u0000c", .expected = 0},
      {.name = "no normalization", .left = "é", .right = "e\\u0301", .expected = 0},
      {.name = "unpaired high", .left = "\\ud800", .right = "x", .expected = -1},
      {.name = "unpaired low", .left = "\\udc00", .right = "x", .expected = -1},
      {.name = "bad surrogate pair", .left = "\\ud800\\u0041", .right = "x", .expected = -1},
      {.name = "truncated escape", .left = "\\u123", .right = "x", .expected = -1},
      {.name = "invalid hex", .left = "\\uGGGG", .right = "x", .expected = -1},
      {.name = "invalid UTF8", .left = "\xc0\xaf", .right = "x", .expected = -1},
      {.name = "validate after mismatch", .left = "x\\q", .right = "y", .expected = -1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    ASSERT_EQ(cases[i].name,
              cases[i].expected,
              json_string_equal_decoded(string_from_cstr(cases[i].left),
                                        string_from_cstr(cases[i].right)));
    CHECK_TEST(cases[i].name);
  }
}

static void
test_json_to_submit_answer_request(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *json;
    int expected;
  } cases[] = {
      {.name = "valid answer", .json = ANSWER_JSON, .expected = 0},
      {.name = "unknown fields",
       .json = "{\"challenge_id\":1,\"answer\":\"a\",\"x\":[{},true]}",
       .expected = 0},
      {.name = "maximum ID",
       .json = "{\"challenge_id\":2147483647,\"answer\":\"a\"}",
       .expected = 0},
      {.name = "overflow ID",
       .json = "{\"challenge_id\":2147483648,\"answer\":\"a\"}",
       .expected = -1},
      {.name = "zero ID", .json = "{\"challenge_id\":0,\"answer\":\"a\"}", .expected = -1},
      {.name = "fractional ID", .json = "{\"challenge_id\":1.0,\"answer\":\"a\"}", .expected = -1},
      {.name = "string ID", .json = "{\"challenge_id\":\"1\",\"answer\":\"a\"}", .expected = -1},
      {.name = "missing ID", .json = "{\"answer\":\"a\"}", .expected = -1},
      {.name = "missing answer", .json = "{\"challenge_id\":1}", .expected = -1},
      {.name = "empty answer", .json = "{\"challenge_id\":1,\"answer\":\"\"}", .expected = -1},
      {.name = "duplicate answer",
       .json = "{\"challenge_id\":1,\"answer\":\"a\",\"answer\":\"b\"}",
       .expected = -1},
      {.name = "unpaired surrogate",
       .json = "{\"challenge_id\":1,\"answer\":\"\\ud800\"}",
       .expected = -1},
      {.name = "trailing input", .json = ANSWER_JSON "x", .expected = -1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    size_t len = strlen(cases[i].json);
    char *input = malloc(len);
    memcpy(input, cases[i].json, len);
    submit_answer_request_t request;
    ASSERT_EQ(
        cases[i].name, cases[i].expected, json_to_submit_answer_request(input, len, &request));

    if (cases[i].expected == 0)
    {
      ASSERT_TRUE(cases[i].name, request.answer.ptr >= input && request.answer.ptr < input + len);
      ASSERT_TRUE(cases[i].name, !request.is_string_allocated);
    }

    free(input);
    CHECK_TEST(cases[i].name);
  }

  const struct
  {
    const char *name;
    const char *unit;
    size_t count;
    int expected;
  } limits[] = {
      {.name = "255 ASCII", .unit = "a", .count = 255, .expected = 0},
      {.name = "256 ASCII", .unit = "a", .count = 256, .expected = -1},
      {.name = "255 Japanese", .unit = "あ", .count = 255, .expected = 0},
      {.name = "256 Japanese", .unit = "あ", .count = 256, .expected = -1},
      {.name = "252 stored escape characters", .unit = "\\u0061", .count = 42, .expected = 0},
      {.name = "258 stored escape characters", .unit = "\\u0061", .count = 43, .expected = -1},
  };

  for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); i++)
  {
    ctx->is_canceled = false;
    char body[2048];
    size_t offset = (size_t)snprintf(body, sizeof(body), "{\"challenge_id\":1,\"answer\":\"");

    for (size_t j = 0; j < limits[i].count; j++)
    {
      size_t len = strlen(limits[i].unit);
      memcpy(body + offset, limits[i].unit, len);
      offset += len;
    }

    memcpy(body + offset, "\"}", 2);
    offset += 2;
    submit_answer_request_t request;
    ASSERT_EQ(
        limits[i].name, limits[i].expected, json_to_submit_answer_request(body, offset, &request));
    CHECK_TEST(limits[i].name);
  }
}

static void
test_bind_answers(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool empty;
    bool invalid_shape;
    bool null_cell;
    bool null_username;
    bool invalid_username;
    bool invalid_id;
    bool invalid_correct;
    bool fail_calloc;
    bool fail_malloc;
    int malloc_after;
    bool success;
  } cases[] = {
      {.name = "valid rows", .success = true},
      {.name = "empty rows", .empty = true, .success = true},
      {.name = "invalid shape", .invalid_shape = true},
      {.name = "NULL column", .null_cell = true},
      {.name = "NULL username", .null_username = true},
      {.name = "invalid username", .invalid_username = true},
      {.name = "invalid ID", .invalid_id = true},
      {.name = "invalid correct", .invalid_correct = true},
      {.name = "array allocation", .fail_calloc = true},
      {.name = "first string allocation", .fail_malloc = true},
      {.name = "partial row allocation", .fail_malloc = true, .malloc_after = 2},
      {.name = "partial array allocation", .fail_malloc = true, .malloc_after = 4},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    db_result_t result = {.success = 1, .res = answer_fixture(cases[i].empty)};
    test_mysql_rows *mock = (test_mysql_rows *)result.res;

    if (cases[i].invalid_shape)
    {
      mock->fields = 5;
    }

    if (cases[i].null_cell)
    {
      free(mock->rows[0][2]);
      mock->rows[0][2] = NULL;
    }

    if (cases[i].null_username)
    {
      free(mock->rows[0][6]);
      mock->rows[0][6] = NULL;
    }

    if (cases[i].invalid_username)
    {
      mock->rows[0][6][0] = '"';
    }

    if (cases[i].invalid_id)
    {
      mock->rows[0][0][0] = 'x';
    }

    if (cases[i].invalid_correct)
    {
      mock->rows[0][4][0] = '2';
    }

    answer_t *answers;
    size_t count;

    if (cases[i].fail_calloc)
    {
      test_set_calloc_failure(0);
    }

    if (cases[i].fail_malloc)
    {
      test_set_malloc_failure(cases[i].malloc_after);
    }

    ASSERT_EQ(cases[i].name, cases[i].success, bind_answers(&result, &answers, &count));
    test_set_calloc_failure(-1);
    test_set_malloc_failure(-1);
    mysql_free_result(result.res);

    if (cases[i].success)
    {
      ASSERT_EQ(cases[i].name, cases[i].empty ? (size_t)0 : (size_t)5, count);

      if (count)
      {
        ASSERT_STR_EQ(cases[i].name, "wrong", answers[0].answer.ptr);
        ASSERT_STR_EQ(cases[i].name, "Alice", answers[0].username.ptr);
        ASSERT_TRUE(cases[i].name, answers[0].is_string_allocated);
      }
    }
    else
    {
      ASSERT_NULL(cases[i].name, answers);
      ASSERT_EQ(cases[i].name, (size_t)0, count);
    }

    free_answers(answers, count);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }
}

#define PRIVATE_ANSWER                                                                             \
  "{\"challenge_id\":1,\"answer\":\"a\\n\\\"b\",\"correct\":false,\"username\":\"Alice\","          \
  "\"answered_at\":\"" DATE "\"}"
#define PUBLIC_ANSWER "{\"challenge_id\":1,\"username\":\"Alice\",\"answered_at\":\"" DATE "\"}"

static const answer_t serialized_answer = {
    .challenge_id = 1,
    .user_id = {.ptr = "0123456789abcdef0123456789abcdef", .len = 32},
    .username = {.ptr = "Alice", .len = 5},
    .answer = {.ptr = "a\\n\\\"b", .len = 6},
    .created_at = {.ptr = DATE, .len = sizeof(DATE) - 1},
};

static void
test_answer_to_json(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool fail;
  } cases[] = {
      {.name = "answer JSON", .fail = false},
      {.name = "answer JSON allocation", .fail = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    string_t json;
    test_set_malloc_failure(cases[i].fail ? 0 : -1);
    answer_to_json(&serialized_answer, &json);
    test_set_malloc_failure(-1);

    if (cases[i].fail)
    {
      ASSERT_NULL(cases[i].name, json.ptr);
    }
    else
    {
      ASSERT_STR_EQ(cases[i].name, PRIVATE_ANSWER, json.ptr);
      ASSERT_EQ(cases[i].name, strlen(PRIVATE_ANSWER), json.len);
    }

    free(json.ptr);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_answers_to_json(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool public_view;
    size_t count;
    const char *expected;
  } cases[] = {
      {.name = "empty public", .public_view = true, .count = 0, .expected = "[]"},
      {.name = "empty own", .public_view = false, .count = 0, .expected = "[]"},
      {.name = "public array",
       .public_view = true,
       .count = 2,
       .expected = "[" PUBLIC_ANSWER "," PUBLIC_ANSWER "]"},
      {.name = "own array",
       .public_view = false,
       .count = 2,
       .expected = "[" PRIVATE_ANSWER "," PRIVATE_ANSWER "]"},
  };
  answer_t answers[] = {serialized_answer, serialized_answer};

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    string_t json;
    answers_to_json(answers, cases[i].count, &json, cases[i].public_view);
    ASSERT_STR_EQ(cases[i].name, cases[i].expected, json.ptr);
    ASSERT_EQ(cases[i].name, strlen(cases[i].expected), json.len);
    free(json.ptr);
    CHECK_TEST(cases[i].name);
  }
}

static void
check_challenge_binding(test_ctx_t *ctx, bool include_flag)
{
  const struct
  {
    const char *name;
    bool empty;
    bool null_column;
    bool fail_calloc;
    int malloc_after;
    bool success;
  } cases[] = {
      {.name = "owned challenge strings", .malloc_after = -1, .success = true},
      {.name = "empty challenges", .empty = true, .malloc_after = -1, .success = true},
      {.name = "NULL challenge column", .null_column = true, .malloc_after = -1},
      {.name = "challenge array failure", .fail_calloc = true, .malloc_after = -1},
      {.name = "first challenge string failure", .malloc_after = 0},
      {.name = "partial challenge string failure", .malloc_after = 2},
      {.name = "later challenge failure", .malloc_after = 5},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const char *values[][6] = {
        {"1", "0123456789abcdef0123456789abcdef", "first", "description", include_flag ? "fl\\u0061g" : "0", "0"},
        {"2", "other", "second", "description", include_flag ? "second" : "1", "1"},
    };
    db_result_t result = {
        .success = 1,
        .res = test_mysql_result(values, cases[i].empty ? 0 : 2, include_flag ? 6 : 5),
    };
    test_mysql_rows *rows = (test_mysql_rows *)result.res;

    if (cases[i].null_column)
    {
      free(rows->rows[0][3]);
      rows->rows[0][3] = NULL;
    }

    test_set_calloc_failure(cases[i].fail_calloc ? 0 : -1);
    test_set_malloc_failure(cases[i].malloc_after);
    size_t count = 999;
    challenge_t *challenges = include_flag ? bind_challenges(&result, &count)
                                           : bind_challenges_without_flag(&result, &count);
    test_set_calloc_failure(-1);
    test_set_malloc_failure(-1);
    mysql_free_result(result.res);

    if (cases[i].success && !cases[i].empty)
    {
      ASSERT_NOT_NULL(cases[i].name, challenges);
      ASSERT_EQ(cases[i].name, (size_t)2, count);

      if (challenges)
      {
        ASSERT_STR_EQ(cases[i].name, "first", challenges[0].name.ptr);
        ASSERT_TRUE(cases[i].name, challenges[0].is_string_allocated);

        if (include_flag)
        {
          ASSERT_STR_EQ(cases[i].name, "fl\\u0061g", challenges[0].flag.ptr);
        }
        else
        {
          ASSERT_NULL(cases[i].name, challenges[0].flag.ptr);
        }
      }
    }
    else
    {
      ASSERT_NULL(cases[i].name, challenges);
      ASSERT_EQ(cases[i].name, (size_t)0, count);
    }

    free_challenges(challenges, count);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_bind_challenges(test_ctx_t *ctx)
{
  check_challenge_binding(ctx, true);
}

static void
test_bind_challenges_without_flag(test_ctx_t *ctx)
{
  check_challenge_binding(ctx, false);
}

void
test_app_answer(test_ctx_t *ctx)
{
  test_read_positive_integer(ctx);
  test_json_string_equal_decoded(ctx);
  test_json_to_submit_answer_request(ctx);
  test_bind_answers(ctx);
  test_bind_challenges(ctx);
  test_bind_challenges_without_flag(ctx);
  test_answer_to_json(ctx);
  test_answers_to_json(ctx);
  test_handle_post_answers_1(ctx);
  test_handle_post_answers_2(ctx);
  test_handle_post_answers_3(ctx);
  test_handle_post_answers_4(ctx);
  test_handle_post_answers_5(ctx);
  test_handle_get_answers_1(ctx);
  test_handle_get_answers_2(ctx);
  test_handle_get_answers_3(ctx);
  test_handle_get_own_answers_1(ctx);
  test_handle_get_own_answers_2(ctx);
  test_handle_get_own_answers_3(ctx);
}
