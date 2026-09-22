#include "test.h"
#include "util.h"

#include <app/json_auth.h>
#include <app/json_p.h>

static void
test_json_string_decode(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *input;
    const char *expected;
    size_t expected_length;
    int result;
  } cases[] = {
      {.name = "empty", .input = "", .expected = "", .result = 0},
      {.name = "ASCII", .input = "abc", .expected = "abc", .expected_length = 3},
      {.name = "escapes", .input = "\\\"\\\\\\/\\b\\f\\n\\r\\t", .expected = "\"\\/\b\f\n\r\t", .expected_length = 8},
      {.name = "unicode", .input = "\\u00e9\\u3042\\ud83d\\ude00", .expected = "éあ😀", .expected_length = 9},
      {.name = "literal UTF-8", .input = "éあ😀", .expected = "éあ😀", .expected_length = 9},
      {.name = "embedded NUL", .input = "a\\u0000b", .expected = "a\0b", .expected_length = 3},
      {.name = "unpaired high", .input = "\\ud800", .result = -1},
      {.name = "unpaired low", .input = "\\udc00", .result = -1},
      {.name = "invalid pair", .input = "\\ud800\\u0041", .result = -1},
      {.name = "invalid escape", .input = "\\x", .result = -1},
      {.name = "overlong UTF-8", .input = "\xc0\xaf", .result = -1},
      {.name = "UTF-8 surrogate", .input = "\xed\xa0\x80", .result = -1},
      {.name = "truncated UTF-8", .input = "\xf0\x9f", .result = -1},
      {.name = "NULL", .input = NULL, .result = -1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    string_t output = {0};
    string_t input = {.ptr = (char *)cases[i].input, .len = cases[i].input ? strlen(cases[i].input) : 0};
    ASSERT_EQ(cases[i].name, cases[i].result, json_string_decode(input, &output));

    if (cases[i].result == 0)
    {
      ASSERT_NOT_NULL(cases[i].name, output.ptr);
      ASSERT_EQ(cases[i].name, cases[i].expected_length, output.len);
      ASSERT_EQ(cases[i].name, 0, memcmp(cases[i].expected, output.ptr, output.len));
      ASSERT_EQ(cases[i].name, 0, (int)output.ptr[output.len]);
    }
    else
    {
      ASSERT_NULL(cases[i].name, output.ptr);
    }

    free(output.ptr);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_json_to_auth_request(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *json;
    const char *username;
    const char *password;
    int result;
  } cases[] = {
      {.name = "normal", .json = "{\"username\":\"Alice_1\",\"password\":\"password\"}", .username = "Alice_1", .password = "password"},
      {.name = "escaped values and keys", .json = "{\"\\u0075sername\":\"\\u0041lice\",\"pass\\u0077ord\":\"p\\u0061ssword\"}", .username = "Alice", .password = "password"},
      {.name = "whitespace is significant", .json = " {\"password\":\" password \",\"username\":\"abc\"} \n", .username = "abc", .password = " password "},
      {.name = "escaped quote", .json = "{\"username\":\"abc\",\"password\":\"pass\\\"word\"}", .username = "abc", .password = "pass\"word"},
      {.name = "surrogate pair", .json = "{\"username\":\"abc\",\"password\":\"\\ud83d\\ude00\\ud83d\\ude00\"}", .username = "abc", .password = "😀😀"},
      {.name = "literal UTF-8", .json = "{\"username\":\"abc\",\"password\":\"éあ😀\"}", .username = "abc", .password = "éあ😀"},
      {.name = "missing password", .json = "{\"username\":\"abc\"}", .result = -1},
      {.name = "missing username", .json = "{\"password\":\"password\"}", .result = -1},
      {.name = "wrong type", .json = "{\"username\":3,\"password\":\"password\"}", .result = -1},
      {.name = "NULL password", .json = "{\"username\":\"abc\",\"password\":null}", .result = -1},
      {.name = "unknown field", .json = "{\"username\":\"abc\",\"password\":\"password\",\"extra\":true}", .result = -1},
      {.name = "duplicate", .json = "{\"username\":\"abc\",\"password\":\"password\",\"password\":\"different\"}", .result = -1},
      {.name = "escaped duplicate", .json = "{\"username\":\"abc\",\"password\":\"password\",\"\\u0070assword\":\"different\"}", .result = -1},
      {.name = "short name", .json = "{\"username\":\"ab\",\"password\":\"password\"}", .result = -1},
      {.name = "non-ASCII name", .json = "{\"username\":\"あああ\",\"password\":\"password\"}", .result = -1},
      {.name = "NUL name", .json = "{\"username\":\"ab\\u0000c\",\"password\":\"password\"}", .result = -1},
      {.name = "short password", .json = "{\"username\":\"abc\",\"password\":\"1234567\"}", .result = -1},
      {.name = "decoded short password", .json = "{\"username\":\"abc\",\"password\":\"\\u0061\"}", .result = -1},
      {.name = "NUL password", .json = "{\"username\":\"abc\",\"password\":\"pass\\u0000word\"}", .result = -1},
      {.name = "unpaired surrogate", .json = "{\"username\":\"abc\",\"password\":\"password\\ud800\"}", .result = -1},
      {.name = "invalid UTF-8", .json = "{\"username\":\"abc\",\"password\":\"password\xff\"}", .result = -1},
      {.name = "trailing input", .json = "{\"username\":\"abc\",\"password\":\"password\"}x", .result = -1},
      {.name = "array", .json = "[]", .result = -1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    auth_request_t request;
    char *input = string_from_cstr_dup(cases[i].json).ptr;
    ASSERT_EQ(cases[i].name, cases[i].result, json_to_auth_request(input, strlen(input), &request));
    free(input);

    if (cases[i].result == 0)
    {
      ASSERT_STR_EQ(cases[i].name, cases[i].username, request.username.ptr);
      ASSERT_STR_EQ(cases[i].name, cases[i].password, request.password.ptr);
    }
    else
    {
      ASSERT_NULL(cases[i].name, request.username.ptr);
      ASSERT_NULL(cases[i].name, request.password.ptr);
    }

    free_auth_request(&request);
    free_auth_request(&request);
    ASSERT_NULL(cases[i].name, request.password.ptr);
    CHECK_TEST(cases[i].name);
  }

  const struct
  {
    size_t username_length;
    size_t password_length;
    bool valid;
  } boundaries[] = {
      {.username_length = 3, .password_length = 8, .valid = true},
      {.username_length = 32, .password_length = 128, .valid = true},
      {.username_length = 33, .password_length = 128},
      {.username_length = 32, .password_length = 129},
  };

  for (size_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); i++)
  {
    ctx->is_canceled = false;
    char username[34] = {0};
    char password[130] = {0};
    char json[256];
    memset(username, 'a', boundaries[i].username_length);
    memset(password, 'b', boundaries[i].password_length);
    int length = snprintf(json, sizeof(json), "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);
    auth_request_t request;
    ASSERT_EQ("length boundary", boundaries[i].valid ? 0 : -1, json_to_auth_request(json, length, &request));
    free_auth_request(&request);
    CHECK_TEST("length boundary");
  }

  const struct
  {
    int malloc_after;
    int calloc_after;
  } failures[] = {
      {.malloc_after = 0, .calloc_after = -1},
      {.malloc_after = 1, .calloc_after = -1},
      {.malloc_after = -1, .calloc_after = 0},
      {.malloc_after = -1, .calloc_after = 1},
  };

  for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++)
  {
    ctx->is_canceled = false;
    auth_request_t request;
    const char json[] = "{\"username\":\"abc\",\"password\":\"password\"}";
    test_set_malloc_failure(failures[i].malloc_after);
    test_set_calloc_failure(failures[i].calloc_after);
    int result = json_to_auth_request(json, sizeof(json) - 1, &request);
    test_set_malloc_failure(-1);
    test_set_calloc_failure(-1);
    ASSERT_EQ("allocation cleanup", -2, result);
    ASSERT_NULL("allocation cleanup", request.username.ptr);
    ASSERT_NULL("allocation cleanup", request.password.ptr);
    CHECK_TEST("allocation cleanup");
  }
}

void
test_app_json_auth(test_ctx_t *ctx)
{
  test_json_string_decode(ctx);
  test_json_to_auth_request(ctx);
}
