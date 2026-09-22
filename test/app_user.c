#include "mysql_result_mock.h"
#include "test.h"
#include "util.h"

#include <app/handler.h>
#include <app/handler_auth.h>
#include <app/json.h>
#include <app/repository.h>
#include <app/score.h>
#include <jansson.h>

#define USER_ID  "00000000000000000000000000000001"
#define OTHER_ID "00000000000000000000000000000002"

static void
test_bind_public_users(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *id;
    const char *username;
    bool success;
    bool db_failure;
    bool allocation_failure;
    bool string_failure;
    size_t rows;
    unsigned fields;
  } cases[] = {
      {.name = "zero initial score", .id = USER_ID, .username = "Alice", .rows = 1, .fields = 2, .success = true},
      {.name = "multiple owned rows", .id = USER_ID, .username = "Alice", .rows = 2, .fields = 2, .success = true},
      {.name = "empty", .fields = 2, .success = true},
      {.name = "DB failure", .fields = 2, .db_failure = true},
      {.name = "wrong columns", .fields = 3},
      {.name = "null id", .username = "Alice", .rows = 1, .fields = 2},
      {.name = "invalid id", .id = "dummy", .username = "Alice", .rows = 1, .fields = 2},
      {.name = "null username", .id = USER_ID, .rows = 1, .fields = 2},
      {.name = "invalid username", .id = USER_ID, .username = "Ali\"ce", .rows = 1, .fields = 2},
      {.name = "array allocation", .id = USER_ID, .username = "Alice", .rows = 1, .fields = 2, .allocation_failure = true},
      {.name = "second string allocation", .id = USER_ID, .username = "Alice", .rows = 2, .fields = 2, .string_failure = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const char *rows[][6] = {{cases[i].id, cases[i].username}, {cases[i].id, cases[i].username}};
    db_result_t result = {.success = !cases[i].db_failure, .res = test_mysql_result(rows, cases[i].rows, cases[i].fields)};
    public_user_t *users = NULL;
    size_t count = 99;
    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    test_set_malloc_failure(cases[i].string_failure ? 1 : -1);
    ASSERT_EQ(cases[i].name, cases[i].success, bind_public_users(&result, &users, &count));
    test_set_calloc_failure(-1);
    test_set_malloc_failure(-1);
    mysql_free_result(result.res);
    ASSERT_EQ(cases[i].name, cases[i].success ? cases[i].rows : 0, count);

    if (users)
    {
      ASSERT_STR_EQ(cases[i].name, cases[i].id, users[0].id);
      ASSERT_STR_EQ(cases[i].name, cases[i].username, users[0].username.ptr);
      ASSERT_EQ(cases[i].name, INT64_C(0), users[0].score);
    }

    free_public_users(users, count);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_bind_score_answers(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *user;
    const char *challenge;
    const char *time;
    size_t rows;
    unsigned fields;
    bool success;
    bool allocation_failure;
  } cases[] = {
      {.name = "valid", .user = USER_ID, .challenge = "1", .time = "999", .rows = 2, .fields = 3, .success = true},
      {.name = "empty", .fields = 3, .success = true},
      {.name = "wrong fields", .fields = 2},
      {.name = "null user", .challenge = "1", .time = "999", .rows = 1, .fields = 3},
      {.name = "invalid user", .user = "user1", .challenge = "1", .time = "999", .rows = 1, .fields = 3},
      {.name = "null challenge", .user = USER_ID, .time = "999", .rows = 1, .fields = 3},
      {.name = "zero challenge", .user = USER_ID, .challenge = "0", .time = "999", .rows = 1, .fields = 3},
      {.name = "negative challenge", .user = USER_ID, .challenge = "-1", .time = "999", .rows = 1, .fields = 3},
      {.name = "overflow challenge", .user = USER_ID, .challenge = "9223372036854775808", .time = "999", .rows = 1, .fields = 3},
      {.name = "out of DB range", .user = USER_ID, .challenge = "2147483648", .time = "999", .rows = 1, .fields = 3},
      {.name = "null time", .user = USER_ID, .challenge = "1", .rows = 1, .fields = 3},
      {.name = "empty time", .user = USER_ID, .challenge = "1", .time = "", .rows = 1, .fields = 3},
      {.name = "negative time", .user = USER_ID, .challenge = "1", .time = "-1", .rows = 1, .fields = 3},
      {.name = "bad time", .user = USER_ID, .challenge = "1", .time = "999x", .rows = 1, .fields = 3},
      {.name = "array allocation", .user = USER_ID, .challenge = "1", .time = "999", .rows = 1, .fields = 3, .allocation_failure = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const char *rows[][6] = {{cases[i].user, cases[i].challenge, cases[i].time}, {cases[i].user, cases[i].challenge, cases[i].time}};
    db_result_t result = {.success = true, .res = test_mysql_result(rows, cases[i].rows, cases[i].fields)};
    score_answer_t *answers;
    size_t count;
    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    ASSERT_EQ(cases[i].name, cases[i].success, bind_score_answers(&result, &answers, &count));
    test_set_calloc_failure(-1);
    mysql_free_result(result.res);
    ASSERT_EQ(cases[i].name, cases[i].success ? cases[i].rows : 0, count);

    if (count)
    {
      ASSERT_STR_EQ(cases[i].name, USER_ID, answers[0].user_id);
      ASSERT_EQ(cases[i].name, INT64_C(1), answers[0].challenge_id);
      ASSERT_EQ(cases[i].name, INT64_C(999), answers[0].created_at);
    }

    free(answers);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_calculate_user_scores(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    score_answer_t answers[6];
    size_t count;
    bool end_enabled;
    int64_t end_at;
    int64_t alice;
    int64_t bob;
    const char *first;
  } cases[] = {
      {.name = "no correct answers"},
      {.name = "before end", .answers = {{.user_id = USER_ID, .challenge_id = 1, .created_at = 999}}, .count = 1, .end_enabled = true, .end_at = 1000, .alice = 100},
      {.name = "exact end", .answers = {{.user_id = USER_ID, .challenge_id = 1, .created_at = 1000}}, .count = 1, .end_enabled = true, .end_at = 1000},
      {.name = "after end", .answers = {{.user_id = USER_ID, .challenge_id = 1, .created_at = 1001}}, .count = 1, .end_enabled = true, .end_at = 1000},
      {.name = "end disabled", .answers = {{.user_id = USER_ID, .challenge_id = 1, .created_at = 1001}}, .count = 1, .end_at = 1000, .alice = 100},
      {.name = "explicit epoch end", .answers = {{.user_id = USER_ID, .challenge_id = 1, .created_at = 0}}, .count = 1, .end_enabled = true},
      {.name = "duplicates before and after", .answers = {{.user_id = USER_ID, .challenge_id = 1, .created_at = 1001}, {.user_id = USER_ID, .challenge_id = 1, .created_at = 999}, {.user_id = USER_ID, .challenge_id = 1, .created_at = 998}}, .count = 3, .end_enabled = true, .end_at = 1000, .alice = 100},
      {.name = "duplicates after only", .answers = {{.user_id = USER_ID, .challenge_id = 1, .created_at = 1001}, {.user_id = USER_ID, .challenge_id = 1, .created_at = 1000}}, .count = 2, .end_enabled = true, .end_at = 1000},
      {.name = "unsorted distinct problems", .answers = {{.user_id = USER_ID, .challenge_id = 2}, {.user_id = USER_ID, .challenge_id = 1}, {.user_id = USER_ID, .challenge_id = 2}}, .count = 3, .alice = 200},
      {.name = "same problem different users and tie", .answers = {{.user_id = OTHER_ID, .challenge_id = 1}, {.user_id = USER_ID, .challenge_id = 1}}, .count = 2, .alice = 100, .bob = 100},
      {.name = "score descending", .answers = {{.user_id = OTHER_ID, .challenge_id = 2}, {.user_id = USER_ID, .challenge_id = 1}, {.user_id = OTHER_ID, .challenge_id = 1}}, .count = 3, .alice = 100, .bob = 200, .first = OTHER_ID},
      {.name = "new user absent from earlier SELECT", .answers = {{.user_id = "ffffffffffffffffffffffffffffffff", .challenge_id = 1}, {.user_id = "00000000000000000000000000000000", .challenge_id = 1}}, .count = 2},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    public_user_t users[] = {{.id = OTHER_ID, .score = 900}, {.id = USER_ID, .score = 900}};
    score_answer_t answers[6];
    memcpy(answers, cases[i].answers, sizeof(answers));
    calculate_user_scores(users, 2, answers, cases[i].count, cases[i].end_enabled, cases[i].end_at);
    ASSERT_STR_EQ(cases[i].name, cases[i].first ? cases[i].first : USER_ID, users[0].id);

    for (size_t j = 0; j < 2; j++)
    {
      ASSERT_EQ(cases[i].name, strcmp(users[j].id, USER_ID) == 0 ? cases[i].alice : cases[i].bob, users[j].score);
    }

    CHECK_TEST(cases[i].name);
  }

  ctx->is_canceled = false;
  calculate_user_scores(NULL, 0, NULL, 0, true, 0);
  CHECK_TEST("empty ranking");
}
static void
test_public_users_to_json(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    size_t count;
    bool allocation_failure;
    const char *expected;
  } cases[] = {
      {.name = "empty", .expected = "[]"},
      {.name = "one user", .count = 1, .expected = "[{\"id\":\"" USER_ID "\",\"username\":\"Alice\",\"score\":214748364700}]"},
      {.name = "multiple users", .count = 2, .expected = "[{\"id\":\"" USER_ID "\",\"username\":\"Alice\",\"score\":214748364700},{\"id\":\"" OTHER_ID "\",\"username\":\"Bob\",\"score\":0}]"},
      {.name = "allocation failure", .count = 2, .allocation_failure = true},
  };
  public_user_t users[] = {
      {.id = USER_ID, .username = {.ptr = "Alice", .len = 5}, .score = INT64_C(214748364700)},
      {.id = OTHER_ID, .username = {.ptr = "Bob", .len = 3}},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    string_t json;
    test_set_malloc_failure(cases[i].allocation_failure ? 0 : -1);
    public_users_to_json(cases[i].count ? users : NULL, cases[i].count, &json);
    test_set_malloc_failure(-1);

    if (cases[i].expected)
    {
      ASSERT_NOT_NULL(cases[i].name, json.ptr);
      ASSERT_STR_EQ(cases[i].name, cases[i].expected, json.ptr);
      ASSERT_EQ(cases[i].name, strlen(cases[i].expected), json.len);
    }
    else
    {
      ASSERT_NULL(cases[i].name, json.ptr);
      ASSERT_EQ(cases[i].name, (size_t)0, json.len);
    }

    free(json.ptr);
    CHECK_TEST(cases[i].name);
  }
}

typedef struct
{
  const char *name;
  bool empty;
  bool no_answers;
  bool db_failure;
  bool missing_result;
  bool allocation_failure;
  bool json_failure;
  bool enqueue_failure;
  bool invalid_time;
  bool end_enabled;
  bool complete;
  http_status expected;
} user_handler_case;

static void
run_user_handlers(test_ctx_t *ctx, unsigned target, const user_handler_case *cases, size_t case_count)
{
  const char *user_rows[][6] = {{OTHER_ID, "Bob"}, {USER_ID, "Alice"}};
  const char *answer_rows[][6] = {
      {OTHER_ID, "1", "1000"},
      {USER_ID, "1", "999"},
      {USER_ID, "1", "1001"},
      {OTHER_ID, "2", "1001"},
      {USER_ID, "3", "1001"},
  };

  for (size_t i = 0; i < case_count; i++)
  {
    ctx->is_canceled = false;
    http_request_t *request = calloc(1, sizeof(*request));
    http_handler_t third = {.func = handle_get_users_3};
    http_handler_t second = {.func = handle_get_users_2, .next = &third};
    http_handler_t first = {.func = handle_get_users_1, .next = &second};
    auth_runtime_t runtime = {.contest_end_at = 1000, .contest_end_enabled = cases[i].end_enabled};
    http_request_context_t context = {.request = request, .current_handler = &first, .app_context = &runtime};
    db_pool_t db = {.task_queue = task_queue_new()};
    http_response_t response;
    bool complete = false;

    for (unsigned step = 1; step <= target && !complete; step++)
    {
      db_task_t *task = NULL;

      if (step > 1)
      {
        task = task_queue_pop(db.task_queue);
        ASSERT_NOT_NULL(cases[i].name, task);

        if (!task)
        {
          break;
        }

        task->result->success = !(step == target && cases[i].db_failure);

        if (step == target && cases[i].missing_result)
        {
          free(task->result);
          task->result = NULL;
        }
        else if (step == 2)
        {
          task->result->res = test_mysql_result(user_rows, cases[i].empty ? 0 : 2, 2);
        }
        else
        {
          task->result->res = test_mysql_result(answer_rows, cases[i].empty || cases[i].no_answers ? 0 : 5, 3);

          if (cases[i].invalid_time)
          {
            ((test_mysql_rows *)task->result->res)->rows[0][2][0] = 'x';
          }
        }
      }

      db.task_queue->stop = step == target && cases[i].enqueue_failure;
      test_set_calloc_failure(step == target && cases[i].allocation_failure ? 0 : -1);
      test_set_malloc_failure(step == target && cases[i].json_failure ? 0 : -1);
      complete = context.current_handler->func(&context, &db, task, &response);
      test_set_calloc_failure(-1);
      test_set_malloc_failure(-1);

      if (!complete)
      {
        ASSERT_NOT_NULL(cases[i].name, db.task_queue->head);

        if (db.task_queue->head)
        {
          ASSERT_STR_EQ(cases[i].name,
                        step == 1 ? "SELECT id,username FROM users" : "SELECT user_id,challenge_id,TIMESTAMPDIFF(SECOND,'1970-01-01',created_at) FROM answers WHERE is_correct=1",
                        db.task_queue->head->query);
          ASSERT_TRUE(cases[i].name, db.task_queue->head->data == &context);
        }
      }
    }

    ASSERT_EQ(cases[i].name, cases[i].complete, complete);

    if (complete)
    {
      ASSERT_EQ(cases[i].name, cases[i].expected, response.status);
      ASSERT_NULL(cases[i].name, db.task_queue->head);

      if (response.status == HTTP_STATUS_OK)
      {
        ASSERT_STR_EQ(cases[i].name, "application/json", response.content_type);
        json_t *body = json_loadb(response.body, response.body_len, JSON_REJECT_DUPLICATES, NULL);
        ASSERT_NOT_NULL(cases[i].name, body);
        ASSERT_EQ(cases[i].name, cases[i].empty ? (size_t)0 : (size_t)2, json_array_size(body));

        if (!cases[i].empty)
        {
          json_t *alice = json_array_get(body, 0);
          json_t *bob = json_array_get(body, 1);
          ASSERT_STR_EQ(cases[i].name, USER_ID, json_string_value(json_object_get(alice, "id")));
          ASSERT_STR_EQ(cases[i].name, "Alice", json_string_value(json_object_get(alice, "username")));
          ASSERT_STR_EQ(cases[i].name, OTHER_ID, json_string_value(json_object_get(bob, "id")));
          ASSERT_EQ(cases[i].name, (json_int_t)(cases[i].no_answers ? 0 : cases[i].end_enabled ? 100
                                                                                               : 200),
                    json_integer_value(json_object_get(alice, "score")));
          ASSERT_EQ(cases[i].name, (json_int_t)(cases[i].no_answers || cases[i].end_enabled ? 0 : 200), json_integer_value(json_object_get(bob, "score")));
          ASSERT_EQ(cases[i].name, (size_t)3, json_object_size(alice));
        }

        json_decref(body);
      }
      else
      {
        ASSERT_EQ(cases[i].name, (size_t)0, response.body_len);
      }
    }

    task_queue_free(db.task_queue);
    http_request_dispose(request);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_handle_get_users_1(test_ctx_t *ctx)
{
  const user_handler_case cases[] = {
      {.name = "queue users"},
      {.name = "state allocation", .allocation_failure = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "users enqueue failure", .enqueue_failure = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_user_handlers(ctx, 1, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_get_users_2(test_ctx_t *ctx)
{
  const user_handler_case cases[] = {
      {.name = "queue correct answers"},
      {.name = "empty users", .empty = true},
      {.name = "user SELECT failure", .db_failure = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "missing users result", .missing_result = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "user binding allocation", .allocation_failure = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "answers enqueue failure", .enqueue_failure = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_user_handlers(ctx, 2, cases, sizeof(cases) / sizeof(cases[0]));
}

static void
test_handle_get_users_3(test_ctx_t *ctx)
{
  const user_handler_case cases[] = {
      {.name = "scores without deadline", .complete = true, .expected = HTTP_STATUS_OK},
      {.name = "scores exclude end and later", .end_enabled = true, .complete = true, .expected = HTTP_STATUS_OK},
      {.name = "empty ranking", .empty = true, .complete = true, .expected = HTTP_STATUS_OK},
      {.name = "no correct answers keeps users", .no_answers = true, .complete = true, .expected = HTTP_STATUS_OK},
      {.name = "answer SELECT failure", .db_failure = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "missing answers result", .missing_result = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "answer binding allocation", .allocation_failure = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "invalid answer time", .invalid_time = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "JSON allocation", .json_failure = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  run_user_handlers(ctx, 3, cases, sizeof(cases) / sizeof(cases[0]));
}

void
test_app_user(test_ctx_t *ctx)
{
  test_bind_public_users(ctx);
  test_bind_score_answers(ctx);
  test_calculate_user_scores(ctx);
  test_public_users_to_json(ctx);
  test_handle_get_users_1(ctx);
  test_handle_get_users_2(ctx);
  test_handle_get_users_3(ctx);
}
