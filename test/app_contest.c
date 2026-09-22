#include "mysql_result_mock.h"
#include "test.h"
#include "util.h"

#include <app/contest.h>
#include <app/handler.h>
#include <app/handler_auth.h>
#include <jansson.h>

#define USER_ID  "0123456789abcdef0123456789abcdef"
#define OTHER_ID "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

static int64_t
fixed_now(void *data)
{
  return *(int64_t *)data;
}

static void
test_contest_start_parse(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *value;
    bool valid;
    int64_t expected;
  } cases[] = {
      {.name = "unset", .valid = true},
      {.name = "empty", .value = "", .valid = true},
      {.name = "epoch", .value = "1970-01-01T00:00:00Z", .valid = true},
      {.name = "UTC", .value = "2026-10-01T01:00:00Z", .valid = true, .expected = 1790816400},
      {.name = "JST", .value = "2026-10-01T10:00:00+09:00", .valid = true, .expected = 1790816400},
      {.name = "negative offset", .value = "2026-09-30T20:00:00-05:00", .valid = true, .expected = 1790816400},
      {.name = "offset minutes", .value = "1970-01-01T05:30:00+05:30", .valid = true},
      {.name = "leap year", .value = "2000-02-29T00:00:00Z", .valid = true, .expected = 951782400},
      {.name = "nonleap century", .value = "2100-02-29T00:00:00Z"},
      {.name = "nonleap year", .value = "2026-02-29T00:00:00Z"},
      {.name = "invalid month", .value = "2026-13-01T00:00:00Z"},
      {.name = "zero month", .value = "2026-00-01T00:00:00Z"},
      {.name = "invalid day", .value = "2026-04-31T00:00:00Z"},
      {.name = "zero day", .value = "2026-01-00T00:00:00Z"},
      {.name = "invalid hour", .value = "2026-01-01T24:00:00Z"},
      {.name = "invalid minute", .value = "2026-01-01T00:60:00Z"},
      {.name = "leap second unsupported", .value = "2026-01-01T00:00:60Z"},
      {.name = "timezone required", .value = "2026-01-01T00:00:00"},
      {.name = "fraction unsupported", .value = "2026-01-01T00:00:00.1Z"},
      {.name = "trailing space", .value = "2026-01-01T00:00:00Z "},
      {.name = "missing separator", .value = "2026-01-01 00:00:00Z"},
      {.name = "invalid digit", .value = "2026-01-01T0x:00:00Z"},
      {.name = "invalid zone", .value = "2026-01-01T00:00:00X"},
      {.name = "invalid offset hour", .value = "2026-01-01T00:00:00+24:00"},
      {.name = "invalid offset minute", .value = "2026-01-01T00:00:00+09:60"},
      {.name = "invalid offset sign", .value = "2026-01-01T00:00:00Z09:00"},
      {.name = "before epoch", .value = "1969-12-31T23:59:59Z"},
      {.name = "offset before epoch", .value = "1970-01-01T00:00:00+09:00"},
      {.name = "Unix seconds rejected", .value = "1790816400"},
      {.name = "large year", .value = "9999-12-31T23:59:59Z", .valid = true, .expected = INT64_C(253402300799)},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    int64_t start_at = -1;
    ASSERT_EQ(cases[i].name, cases[i].valid, contest_start_parse(cases[i].value, &start_at));
    ASSERT_EQ(cases[i].name, cases[i].expected, start_at);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_handle_get_challenges_1(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    int64_t start_at;
    int64_t now;
    bool signed_in;
    bool stopped;
    bool complete;
    http_status expected;
  } cases[] = {
      {.name = "disabled", .now = 1000},
      {.name = "before start", .start_at = 1000, .now = 999, .complete = true, .expected = HTTP_STATUS_FORBIDDEN},
      {.name = "signed in before start", .start_at = 1000, .now = 999, .signed_in = true, .complete = true, .expected = HTTP_STATUS_FORBIDDEN},
      {.name = "exact start", .start_at = 1000, .now = 1000},
      {.name = "after start", .start_at = 1000, .now = 1001},
      {.name = "clock failure", .start_at = 1000, .now = -1, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "enqueue failure", .stopped = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    int64_t now = cases[i].now;
    auth_runtime_t runtime = {.contest_start_at = cases[i].start_at, .now = fixed_now, .clock_data = &now};
    auth_identity_t identity = {.claims.user_id = USER_ID, .verified = cases[i].signed_in};
    http_request_t request = {.auth_data = &identity};
    http_handler_t last = {.func = handle_get_challenges_2};
    http_handler_t first = {.func = handle_get_challenges_1, .next = &last};
    http_request_context_t context = {.request = &request, .app_context = &runtime, .current_handler = &first};
    db_pool_t db = {.task_queue = task_queue_new()};
    db.task_queue->stop = cases[i].stopped;
    http_response_t response;
    ASSERT_EQ(cases[i].name, cases[i].complete, handle_get_challenges_1(&context, &db, NULL, &response));

    if (cases[i].complete)
    {
      ASSERT_EQ(cases[i].name, cases[i].expected, response.status);
      ASSERT_NULL(cases[i].name, db.task_queue->head);
      ASSERT_EQ(cases[i].name, (size_t)0, response.body_len);
    }
    else
    {
      ASSERT_TRUE(cases[i].name, context.current_handler == &last);
      db_task_t *task = task_queue_pop(db.task_queue);
      ASSERT_NOT_NULL(cases[i].name, task);
      ASSERT_NULL(cases[i].name, strstr(task->query, "flag"));
      db_task_free(task);
    }

    task_queue_free(db.task_queue);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_handle_get_own_challenges_1(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool verified;
    bool stopped;
    int64_t now;
    bool complete;
    http_status expected;
  } cases[] = {
      {.name = "own before start", .verified = true, .now = 999},
      {.name = "own after start", .verified = true, .now = 1001},
      {.name = "anonymous", .complete = true, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "enqueue failure", .verified = true, .stopped = true, .complete = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    int64_t now = cases[i].now;
    auth_runtime_t runtime = {.contest_start_at = 1000, .now = fixed_now, .clock_data = &now};
    auth_identity_t identity = {.claims.user_id = USER_ID, .verified = cases[i].verified};
    http_request_t request = {.auth_data = &identity};
    http_handler_t last = {.func = handle_get_own_challenges_2};
    http_handler_t first = {.func = handle_get_own_challenges_1, .next = &last};
    http_request_context_t context = {.request = &request, .app_context = &runtime, .current_handler = &first};
    db_pool_t db = {.task_queue = task_queue_new()};
    db.task_queue->stop = cases[i].stopped;
    http_response_t response;
    ASSERT_EQ(cases[i].name, cases[i].complete, handle_get_own_challenges_1(&context, &db, NULL, &response));

    if (cases[i].complete)
    {
      ASSERT_EQ(cases[i].name, cases[i].expected, response.status);
      ASSERT_NULL(cases[i].name, db.task_queue->head);
    }
    else
    {
      ASSERT_TRUE(cases[i].name, context.current_handler == &last);
      db_task_t *task = task_queue_pop(db.task_queue);
      ASSERT_NOT_NULL(cases[i].name, task);
      ASSERT_NOT_NULL(cases[i].name, strstr(task->query, "flag"));
      db_task_free(task);
    }

    task_queue_free(db.task_queue);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_handle_get_own_challenges_2(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool verified;
    bool failed;
    bool allocation_failure;
    unsigned fields;
    size_t rows;
    size_t expected_count;
    http_status expected;
  } cases[] = {
      {.name = "only owned including flag", .verified = true, .fields = 6, .rows = 3, .expected_count = 1, .expected = HTTP_STATUS_OK},
      {.name = "no owned rows", .verified = true, .fields = 6, .rows = 1, .expected = HTTP_STATUS_OK},
      {.name = "empty database", .verified = true, .fields = 6, .expected = HTTP_STATUS_OK},
      {.name = "DB failure", .verified = true, .failed = true, .fields = 6, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "invalid columns", .verified = true, .fields = 5, .rows = 1, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "binding allocation", .verified = true, .fields = 6, .rows = 3, .allocation_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "unverified user", .fields = 6, .rows = 3, .expected = HTTP_STATUS_UNAUTHORIZED},
  };
  const char *rows[][6] = {
      {"1", OTHER_ID, "other", "hidden", "other-secret", "0"},
      {"2", USER_ID, "mine", "my description", "my-secret", "0"},
      {"3", "dummy", "legacy", "hidden", "legacy-secret", "0"},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    auth_identity_t identity = {.claims.user_id = USER_ID, .verified = cases[i].verified};
    http_request_t *request = calloc(1, sizeof(*request));
    request->auth_data = &identity;
    http_request_context_t context = {.request = request};
    db_task_t *task = calloc(1, sizeof(*task));
    task->result = calloc(1, sizeof(*task->result));
    task->result->success = !cases[i].failed;
    task->result->res = test_mysql_result(rows, cases[i].rows, cases[i].fields);
    http_response_t response;
    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    ASSERT_TRUE(cases[i].name, handle_get_own_challenges_2(&context, NULL, task, &response));
    test_set_calloc_failure(-1);
    ASSERT_EQ(cases[i].name, cases[i].expected, response.status);
    ASSERT_TRUE(cases[i].name, response.no_store);

    if (response.status == HTTP_STATUS_OK)
    {
      ASSERT_STR_EQ(cases[i].name, "application/json", response.content_type);
      json_t *body = json_loadb(response.body, response.body_len, 0, NULL);
      ASSERT_NOT_NULL(cases[i].name, body);
      ASSERT_EQ(cases[i].name, cases[i].expected_count, json_array_size(body));

      if (cases[i].expected_count)
      {
        json_t *item = json_array_get(body, 0);
        ASSERT_STR_EQ(cases[i].name, USER_ID, json_string_value(json_object_get(item, "creator_id")));
        ASSERT_STR_EQ(cases[i].name, "my-secret", json_string_value(json_object_get(item, "flag")));
      }

      json_decref(body);
    }
    else
    {
      ASSERT_EQ(cases[i].name, (size_t)0, response.body_len);
    }

    ASSERT_TRUE(cases[i].name, request->auth_data == &identity);
    http_request_dispose(request);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }
}

void
test_app_contest(test_ctx_t *ctx)
{
  test_contest_start_parse(ctx);
  test_handle_get_challenges_1(ctx);
  test_handle_get_own_challenges_1(ctx);
  test_handle_get_own_challenges_2(ctx);
}
