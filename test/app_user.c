#include "mysql_result_mock.h"
#include "test.h"
#include "util.h"

#include <app/handler.h>
#include <app/json.h>
#include <app/repository.h>
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
    const char *score;
    int64_t expected_score;
    bool success;
    bool db_failure;
    bool allocation_failure;
    bool string_failure;
    size_t rows;
    unsigned fields;
  } cases[] = {
      {.name = "zero", .id = USER_ID, .username = "Alice", .score = "0", .rows = 1, .fields = 3, .success = true},
      {.name = "multiple owned rows", .id = USER_ID, .username = "Alice", .score = "200", .expected_score = 200, .rows = 2, .fields = 3, .success = true},
      {.name = "64 bit score", .id = USER_ID, .username = "Alice", .score = "214748364700", .expected_score = INT64_C(214748364700), .rows = 1, .fields = 3, .success = true},
      {.name = "maximum integer", .id = USER_ID, .username = "Alice", .score = "9223372036854775807", .expected_score = INT64_MAX, .rows = 1, .fields = 3, .success = true},
      {.name = "empty", .fields = 3, .success = true},
      {.name = "DB failure", .fields = 3, .db_failure = true},
      {.name = "wrong columns", .fields = 2},
      {.name = "null id", .username = "Alice", .score = "100", .rows = 1, .fields = 3},
      {.name = "invalid id", .id = "dummy", .username = "Alice", .score = "100", .rows = 1, .fields = 3},
      {.name = "null username", .id = USER_ID, .score = "100", .rows = 1, .fields = 3},
      {.name = "invalid username", .id = USER_ID, .username = "Ali\"ce", .score = "100", .rows = 1, .fields = 3},
      {.name = "null score", .id = USER_ID, .username = "Alice", .rows = 1, .fields = 3},
      {.name = "empty score", .id = USER_ID, .username = "Alice", .score = "", .rows = 1, .fields = 3},
      {.name = "negative score", .id = USER_ID, .username = "Alice", .score = "-100", .rows = 1, .fields = 3},
      {.name = "overflow score", .id = USER_ID, .username = "Alice", .score = "9223372036854775808", .rows = 1, .fields = 3},
      {.name = "fractional score", .id = USER_ID, .username = "Alice", .score = "100.0", .rows = 1, .fields = 3},
      {.name = "trailing data", .id = USER_ID, .username = "Alice", .score = "100x", .rows = 1, .fields = 3},
      {.name = "array allocation", .id = USER_ID, .username = "Alice", .score = "100", .rows = 1, .fields = 3, .allocation_failure = true},
      {.name = "second string allocation", .id = USER_ID, .username = "Alice", .score = "100", .rows = 2, .fields = 3, .string_failure = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const char *rows[][6] = {{cases[i].id, cases[i].username, cases[i].score}, {cases[i].id, cases[i].username, cases[i].score}};
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
      ASSERT_EQ(cases[i].name, cases[i].expected_score, users[0].score);
    }

    free_public_users(users, count);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }
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

static void
test_handle_get_users_1(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool stopped;
  } cases[] = {
      {.name = "public score query"},
      {.name = "enqueue failure", .stopped = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    http_handler_t last = {.func = handle_get_users_2};
    http_handler_t first = {.func = handle_get_users_1, .next = &last};
    http_request_t request = {0};
    http_request_context_t context = {.request = &request, .current_handler = &first};
    db_pool_t db = {.task_queue = task_queue_new()};
    db.task_queue->stop = cases[i].stopped;
    http_response_t response;
    ASSERT_EQ(cases[i].name, cases[i].stopped, handle_get_users_1(&context, &db, NULL, &response));

    if (cases[i].stopped)
    {
      ASSERT_EQ(cases[i].name, (http_status)HTTP_STATUS_INTERNAL_SERVER_ERROR, response.status);
      ASSERT_NULL(cases[i].name, db.task_queue->head);
      ASSERT_TRUE(cases[i].name, context.current_handler == &first);
    }
    else
    {
      db_task_t *task = task_queue_pop(db.task_queue);
      ASSERT_NOT_NULL(cases[i].name, task);
      ASSERT_TRUE(cases[i].name, context.current_handler == &last);
      ASSERT_STR_EQ(cases[i].name,
                    "SELECT u.id,u.username,COUNT(DISTINCT a.challenge_id)*100 AS score "
                    "FROM users u LEFT JOIN answers a ON a.user_id=u.id AND a.is_correct=1 "
                    "GROUP BY u.id,u.username ORDER BY score DESC,u.id ASC",
                    task->query);
      ASSERT_TRUE(cases[i].name, task->query_len < DEFAULT_QUERY_SIZE);
      ASSERT_TRUE(cases[i].name, task->data == &context);
      db_task_free(task);
    }

    task_queue_free(db.task_queue);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_handle_get_users_2(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    size_t rows;
    bool db_failure;
    bool missing_result;
    bool allocation_failure;
    bool json_failure;
    http_status expected;
  } cases[] = {
      {.name = "ranked results including zero", .rows = 2, .expected = HTTP_STATUS_OK},
      {.name = "empty", .expected = HTTP_STATUS_OK},
      {.name = "DB failure", .db_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "missing result", .missing_result = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "binding allocation", .rows = 2, .allocation_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "JSON allocation", .json_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  const char *rows[][6] = {{USER_ID, "Alice", "200"}, {OTHER_ID, "Bob", "0"}};

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    http_request_t *request = calloc(1, sizeof(*request));
    http_request_context_t context = {.request = request};
    db_task_t *task = calloc(1, sizeof(*task));

    if (!cases[i].missing_result)
    {
      task->result = calloc(1, sizeof(*task->result));
      task->result->success = !cases[i].db_failure;
      task->result->res = test_mysql_result(rows, cases[i].rows, 3);
    }

    http_response_t response;
    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    test_set_malloc_failure(cases[i].json_failure ? 0 : -1);
    ASSERT_TRUE(cases[i].name, handle_get_users_2(&context, NULL, task, &response));
    test_set_calloc_failure(-1);
    test_set_malloc_failure(-1);
    ASSERT_EQ(cases[i].name, cases[i].expected, response.status);

    if (cases[i].expected == HTTP_STATUS_OK)
    {
      ASSERT_STR_EQ(cases[i].name, "application/json", response.content_type);
      ASSERT_TRUE(cases[i].name, request->app_data == response.body);
      ASSERT_TRUE(cases[i].name, request->dispose_app_data == free);
      json_t *body = json_loadb(response.body, response.body_len, JSON_REJECT_DUPLICATES, NULL);
      ASSERT_NOT_NULL(cases[i].name, body);
      ASSERT_EQ(cases[i].name, cases[i].rows, json_array_size(body));

      for (size_t j = 0; j < cases[i].rows; j++)
      {
        json_t *user = json_array_get(body, j);
        ASSERT_EQ(cases[i].name, (size_t)3, json_object_size(user));
        ASSERT_STR_EQ(cases[i].name, rows[j][0], json_string_value(json_object_get(user, "id")));
        ASSERT_STR_EQ(cases[i].name, rows[j][1], json_string_value(json_object_get(user, "username")));
        ASSERT_EQ(cases[i].name, (json_int_t)(j ? 0 : 200), json_integer_value(json_object_get(user, "score")));
      }

      json_decref(body);
    }
    else
    {
      ASSERT_EQ(cases[i].name, (size_t)0, response.body_len);
      ASSERT_NULL(cases[i].name, request->app_data);
    }

    http_request_dispose(request);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }
}

void
test_app_user(test_ctx_t *ctx)
{
  test_bind_public_users(ctx);
  test_public_users_to_json(ctx);
  test_handle_get_users_1(ctx);
  test_handle_get_users_2(ctx);
}
