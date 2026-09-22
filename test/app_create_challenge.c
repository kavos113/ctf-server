#include "test.h"
#include "util.h"

#include <app/handler.h>
#include <app/handler_auth.h>
#include <app/json.h>
#include <http_request.h>
#include <http_response.h>

#define VALID_FIELDS "\"name\":\"test\",\"description\":\"\",\"flag\":\"flag\",\"genre\":\"web\""
#define VALID_JSON   "{" VALID_FIELDS "}"

static void
test_json_to_create_challenge_request(test_ctx_t *ctx)
{
  struct test_case
  {
    const char *name;
    const char *json;
    int expected;
  } cases[] = {
      {
          .name = "valid",
          .json = VALID_JSON,
          .expected = 0,
      },
      {
          .name = "ignored fields",
          .json = "{" VALID_FIELDS ",\"id\":123,\"creator_id\":\"other\",\"x\":[true,false,null,-1."
                  "5e+2,{\"x\":\"}\\\"\"}]}",
          .expected = 0,
      },
      {
          .name = "escaped slices",
          .json = "{\"name\":\"a\\\"b\",\"description\":\"\\u65e5\\n\\\\\",\"flag\":\"\\u0061\","
                  "\"genre\":\"misc\"}",
          .expected = 0,
      },
      {
          .name = "Japanese",
          .json = "{\"name\":\"問題\",\"description\":\"説明\",\"flag\":\"旗\",\"genre\":\"web\"}",
          .expected = 0,
      },
      {
          .name = "empty input",
          .json = "",
          .expected = -1,
      },
      {
          .name = "missing fields",
          .json = "{}",
          .expected = -1,
      },
      {
          .name = "missing description",
          .json = "{\"name\":\"n\",\"flag\":\"f\",\"genre\":\"web\"}",
          .expected = -1,
      },
      {
          .name = "wrong type",
          .json = "{\"name\":1,\"description\":\"\",\"flag\":\"f\",\"genre\":\"web\"}",
          .expected = -1,
      },
      {
          .name = "null description",
          .json = "{\"name\":\"n\",\"description\":null,\"flag\":\"f\",\"genre\":\"web\"}",
          .expected = -1,
      },
      {
          .name = "empty name",
          .json = "{\"name\":\"\",\"description\":\"\",\"flag\":\"f\",\"genre\":\"web\"}",
          .expected = -1,
      },
      {
          .name = "empty flag",
          .json = "{\"name\":\"n\",\"description\":\"\",\"flag\":\"\",\"genre\":\"web\"}",
          .expected = -1,
      },
      {
          .name = "unknown genre",
          .json = "{\"name\":\"n\",\"description\":\"\",\"flag\":\"f\",\"genre\":\"unknown\"}",
          .expected = -1,
      },
      {
          .name = "duplicate field",
          .json = "{" VALID_FIELDS ",\"name\":\"other\"}",
          .expected = -1,
      },
      {
          .name = "duplicate unknown key",
          .json = "{" VALID_FIELDS ",\"x\":0,\"x\":1}",
          .expected = -1,
      },
      {
          .name = "nested duplicate",
          .json = "{" VALID_FIELDS ",\"x\":{\"a\":0,\"a\":1}}",
          .expected = -1,
      },
      {
          .name = "trailing comma",
          .json = "{" VALID_FIELDS ",}",
          .expected = -1,
      },
      {
          .name = "trailing data",
          .json = VALID_JSON "true",
          .expected = -1,
      },
      {
          .name = "array trailing comma",
          .json = "{" VALID_FIELDS ",\"x\":[1,]}",
          .expected = -1,
      },
      {
          .name = "bad literal",
          .json = "{" VALID_FIELDS ",\"x\":undefined}",
          .expected = -1,
      },
      {
          .name = "leading zero",
          .json = "{" VALID_FIELDS ",\"x\":01}",
          .expected = -1,
      },
      {
          .name = "missing fraction",
          .json = "{" VALID_FIELDS ",\"x\":1.}",
          .expected = -1,
      },
      {
          .name = "missing exponent",
          .json = "{" VALID_FIELDS ",\"x\":1e+}",
          .expected = -1,
      },
      {
          .name = "invalid UTF-8",
          .json = "{" VALID_FIELDS ",\"x\":\"\xc0\xaf\"}",
          .expected = -1,
      },
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const struct test_case *tc = &cases[i];
    size_t len = strlen(tc->json);
    char *input = malloc(len ? len : 1);
    memcpy(input, tc->json, len);
    create_challenge_request_t request;

    ASSERT_EQ(tc->name, tc->expected, json_to_create_challenge_request(input, len, &request));

    if (tc->expected == 0)
    {
      ASSERT_FALSE(tc->name, request.is_string_allocated);
      ASSERT_TRUE(tc->name,
                  request.name.ptr >= input && request.name.ptr + request.name.len <= input + len);
      ASSERT_TRUE(tc->name,
                  request.description.ptr >= input &&
                      request.description.ptr + request.description.len <= input + len);
      ASSERT_TRUE(tc->name,
                  request.flag.ptr >= input && request.flag.ptr + request.flag.len <= input + len);
      ASSERT_EQ(tc->name, 0, memcmp(input, tc->json, len));

      if (strcmp(tc->name, "escaped slices") == 0)
      {
        ASSERT_TRUE(tc->name, string_equals_cstr(request.name, "a\\\"b"));
        ASSERT_TRUE(tc->name, string_equals_cstr(request.description, "\\u65e5\\n\\\\"));
        ASSERT_TRUE(tc->name, string_equals_cstr(request.flag, "\\u0061"));
      }
    }

    free(input);
    CHECK_TEST(tc->name);
  }

  for (size_t len = 0; len < sizeof(VALID_JSON) - 1; len++)
  {
    ctx->is_canceled = false;
    create_challenge_request_t request;

    ASSERT_EQ("truncated JSON", -1, json_to_create_challenge_request(VALID_JSON, len, &request));
    CHECK_TEST("truncated JSON");
  }

  struct boundary_case
  {
    const char *field;
    const char *unit;
    size_t count;
    int expected;
  } boundaries[] = {
      {.field = "name", .unit = "a", .count = 255, .expected = 0},
      {.field = "name", .unit = "a", .count = 256, .expected = -1},
      {.field = "name", .unit = "日", .count = 255, .expected = 0},
      {.field = "name", .unit = "日", .count = 256, .expected = -1},
      {.field = "flag", .unit = "a", .count = 255, .expected = 0},
      {.field = "flag", .unit = "a", .count = 256, .expected = -1},
      {.field = "flag", .unit = "\\u0061", .count = 42, .expected = 0},
      {.field = "flag", .unit = "\\u0061", .count = 43, .expected = -1},
      {.field = "description", .unit = "a", .count = 65535, .expected = 0},
      {.field = "description", .unit = "a", .count = 65536, .expected = -1},
  };

  for (size_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); i++)
  {
    ctx->is_canceled = false;
    const struct boundary_case *tc = &boundaries[i];
    size_t width = strlen(tc->unit);
    char *value = malloc(tc->count * width + 1);

    for (size_t j = 0; j < tc->count; j++)
    {
      memcpy(value + j * width, tc->unit, width);
    }

    value[tc->count * width] = '\0';
    char *json = malloc(tc->count * width + 128);
    int len = sprintf(json,
                      "{\"name\":\"%s\",\"description\":\"%s\",\"flag\":\"%s\",\"genre\":\"misc\"}",
                      strcmp(tc->field, "name") == 0 ? value : "n",
                      strcmp(tc->field, "description") == 0 ? value : "",
                      strcmp(tc->field, "flag") == 0 ? value : "f");
    create_challenge_request_t request;

    ASSERT_EQ(tc->field, tc->expected, json_to_create_challenge_request(json, len, &request));

    free(json);
    free(value);
    CHECK_TEST(tc->field);
  }

  for (ctf_genre genre = CTF_GENRE_WEB; genre <= CTF_GENRE_MISC; genre++)
  {
    ctx->is_canceled = false;
    string_t name = ctf_genre_to_string(genre);
    char json[128];
    int len = snprintf(json,
                       sizeof(json),
                       "{\"name\":\"n\",\"description\":\"\",\"flag\":\"f\",\"genre\":\"%.*s\"}",
                       (int)name.len,
                       name.ptr);
    create_challenge_request_t request;

    ASSERT_EQ("genre", 0, json_to_create_challenge_request(json, len, &request));
    ASSERT_EQ("genre", genre, request.genre);
    CHECK_TEST("genre");
  }
}

static int disposed_count;

static void
counted_dispose(void *data)
{
  disposed_count++;
  free(data);
}

static void
test_post_challenges(test_ctx_t *ctx)
{
  const char escaped_json[] = "{\"name\":\"a\\\"b\",\"description\":\"line\\n\\u65e5\",\"flag\":"
                              "\"f\\\\\",\"genre\":\"web\"}";
  char long_json[8192];
  size_t prefix =
      snprintf(long_json,
               sizeof(long_json),
               "{\"name\":\"test\",\"flag\":\"flag\",\"genre\":\"web\",\"description\":\"");
  memset(long_json + prefix, 'd', 5000);
  memcpy(long_json + prefix + 5000, "\"}", 3);

  struct test_case
  {
    const char *name;
    const char *json;
    int allocation_failure;
    bool db_success;
    uint64_t insert_id;
    http_status expected_status;
  } cases[] = {
      {
          .name = "create",
          .json = VALID_JSON,
          .allocation_failure = -1,
          .db_success = true,
          .insert_id = 123,
          .expected_status = HTTP_STATUS_CREATED,
      },
      {
          .name = "escaped response",
          .json = escaped_json,
          .allocation_failure = -1,
          .db_success = true,
          .insert_id = 123,
          .expected_status = HTTP_STATUS_CREATED,
      },
      {
          .name = "long description",
          .json = long_json,
          .allocation_failure = -1,
          .db_success = true,
          .insert_id = 123,
          .expected_status = HTTP_STATUS_CREATED,
      },
      {
          .name = "ignore ownership",
          .json = "{" VALID_FIELDS ",\"id\":42,\"creator_id\":\"other\"}",
          .allocation_failure = -1,
          .db_success = true,
          .insert_id = 123,
          .expected_status = HTTP_STATUS_CREATED,
      },
      {
          .name = "invalid input",
          .json = "{}",
          .allocation_failure = -1,
          .db_success = false,
          .insert_id = 0,
          .expected_status = HTTP_STATUS_BAD_REQUEST,
      },
      {
          .name = "parser allocation failure",
          .json = VALID_JSON,
          .allocation_failure = 0,
          .db_success = false,
          .insert_id = 0,
          .expected_status = HTTP_STATUS_INTERNAL_SERVER_ERROR,
      },
      {
          .name = "task allocation failure",
          .json = VALID_JSON,
          .allocation_failure = 4,
          .db_success = false,
          .insert_id = 0,
          .expected_status = HTTP_STATUS_INTERNAL_SERVER_ERROR,
      },
      {
          .name = "result allocation failure",
          .json = VALID_JSON,
          .allocation_failure = 5,
          .db_success = false,
          .insert_id = 0,
          .expected_status = HTTP_STATUS_INTERNAL_SERVER_ERROR,
      },
      {
          .name = "DB failure",
          .json = VALID_JSON,
          .allocation_failure = -1,
          .db_success = false,
          .insert_id = 0,
          .expected_status = HTTP_STATUS_INTERNAL_SERVER_ERROR,
      },
      {
          .name = "unrepresentable ID",
          .json = VALID_JSON,
          .allocation_failure = -1,
          .db_success = true,
          .insert_id = UINT64_MAX,
          .expected_status = HTTP_STATUS_INTERNAL_SERVER_ERROR,
      },
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const struct test_case *tc = &cases[i];
    db_pool_t pool = {.task_queue = task_queue_new()};
    http_request_t *request = calloc(1, sizeof(*request));
    auth_identity_t identity = {.claims.user_id = "0123456789abcdef0123456789abcdef", .verified = true};
    request->auth_data = &identity;
    request->content_length = strlen(tc->json);
    char *body = malloc(request->content_length);
    memcpy(body, tc->json, request->content_length);
    request->body = body;
    http_handler_t second = {.func = handle_post_challenges_2};
    http_handler_t first = {.func = handle_post_challenges_1, .next = &second};
    http_request_context_t context = {.request = request, .current_handler = &first};
    http_response_t response;

    test_set_calloc_failure(tc->allocation_failure);
    bool complete = handle_post_challenges_1(&context, &pool, NULL, &response);
    test_set_calloc_failure(-1);

    // DB parameters must remain usable even after the original body is overwritten.
    memset(body, 'x', request->content_length);

    if (!complete)
    {
      ASSERT_TRUE(tc->name, context.current_handler == &second);
      db_task_t *task = task_queue_pop(pool.task_queue);

      ASSERT_EQ(tc->name, (size_t)5, task->param_count);
      ASSERT_TRUE(tc->name, task->data == &context);
      ASSERT_STR_EQ(tc->name,
                    "INSERT INTO challenges (creator_id, name, description, flag, genre) VALUES "
                    "(?, ?, ?, ?, ?)",
                    task->query);
      ASSERT_STR_EQ(tc->name, "0123456789abcdef0123456789abcdef", task->params[0].value.string.ptr);

      task->result->success = tc->db_success;
      task->result->affected = tc->db_success ? 1 : 0;
      task->result->insert_id = tc->insert_id;

      ASSERT_TRUE(tc->name, handle_post_challenges_2(&context, &pool, task, &response));
    }

    ASSERT_EQ(tc->name, tc->expected_status, response.status);

    disposed_count = 0;

    if (response.status == HTTP_STATUS_CREATED)
    {
      ASSERT_STR_EQ(tc->name, "application/json", response.content_type);
      ASSERT_TRUE(tc->name, response.body == request->app_data);
      ASSERT_TRUE(tc->name, request->dispose_app_data == free);
      ASSERT_EQ(tc->name, strlen(response.body), response.body_len);
      ASSERT_TRUE(tc->name, strstr(response.body, "\"id\":123,\"creator_id\":\"0123456789abcdef0123456789abcdef\"") != NULL);

      create_challenge_request_t original;
      create_challenge_request_t created;

      ASSERT_EQ(
          tc->name, 0, json_to_create_challenge_request(tc->json, strlen(tc->json), &original));
      ASSERT_EQ(tc->name,
                0,
                json_to_create_challenge_request(response.body, response.body_len, &created));
      ASSERT_TRUE(tc->name, string_equals(original.name, created.name));
      ASSERT_TRUE(tc->name, string_equals(original.description, created.description));
      ASSERT_TRUE(tc->name, string_equals(original.flag, created.flag));

      request->dispose_app_data = counted_dispose;
    }

    free(body);
    http_request_dispose(request);

    ASSERT_EQ(tc->name, (int)(response.status == HTTP_STATUS_CREATED), disposed_count);

    task_queue_free(pool.task_queue);
    CHECK_TEST(tc->name);
  }
}

void
test_app_create_challenge(test_ctx_t *ctx)
{
  test_json_to_create_challenge_request(ctx);
  test_post_challenges(ctx);
}
