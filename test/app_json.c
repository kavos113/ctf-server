#include "test.h"

#include <app/json.h>
#include <app/json_p.h>

#include "util.h"

void test_skip_whitespace(test_ctx_t *ctx);
void test_parse_json_str(test_ctx_t *ctx);
void test_parse_json_int(test_ctx_t *ctx);
void test_skip_json_value(test_ctx_t *ctx);
void test_json_to_challenge(test_ctx_t *ctx);

void
test_app_json(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_app_json");
  ctx->indent += PREFACE_INDENT;

  test_skip_whitespace(ctx);
  test_parse_json_str(ctx);
  test_parse_json_int(ctx);
  test_skip_json_value(ctx);
  test_json_to_challenge(ctx);

  ctx->indent -= PREFACE_INDENT;
}

void
test_skip_whitespace(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_skip_whitespace");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    const char *expected_output;
  } test_cases[] = {
      {
          .name = "success: leading spaces",
          .input = "   hello",
          .input_len = 8,
          .expected_output = "hello",
      },
      {
          .name = "success: leading tabs",
          .input = "\t\thello",
          .input_len = 7,
          .expected_output = "hello",
      },
      {
          .name = "success: leading newlines",
          .input = "\n\nhello",
          .input_len = 7,
          .expected_output = "hello",
      },
      {
          .name = "success: no leading whitespace",
          .input = "hello",
          .input_len = 5,
          .expected_output = "hello",
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    const char *end = tc->input + tc->input_len;
    const char *result = skip_whitespace(tc->input, end);

    ASSERT_NOT_NULL(tc->name, result);
    ASSERT_STR_EQ(tc->name, result, tc->expected_output);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_json_str(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_json_str");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    const char *expected_output;
    bool expect_null;
  } test_cases[] = {
      {
          .name = "success: simple string",
          .input = "\"hello\"",
          .input_len = 7,
          .expected_output = "hello",
          .expect_null = false,
      },
      {
          .name = "success: escaped quote",
          .input = "\"he\\\"llo\"",
          .input_len = 10,
          .expected_output = "he\"llo",
          .expect_null = false,
      },
      {
          .name = "success: escaped backslash",
          .input = "\"he\\\\llo\"",
          .input_len = 10,
          .expected_output = "he\\llo",
          .expect_null = false,
      },
      {
          .name = "success: escaped forward slash",
          .input = "\"he\\/llo\"",
          .input_len = 10,
          .expected_output = "he/llo",
          .expect_null = false,
      },
      {
          .name = "success: escaped backspace",
          .input = "\"he\\bllo\"",
          .input_len = 9,
          .expected_output = "he\bllo",
          .expect_null = false,
      },
      {
          .name = "success: escaped form feed",
          .input = "\"he\\fllo\"",
          .input_len = 9,
          .expected_output = "he\fllo",
          .expect_null = false,
      },
      {
          .name = "success: escaped newline",
          .input = "\"he\\nllo\"",
          .input_len = 9,
          .expected_output = "he\nllo",
          .expect_null = false,
      },
      {
          .name = "success: escaped carriage return",
          .input = "\"he\\rllo\"",
          .input_len = 9,
          .expected_output = "he\rllo",
          .expect_null = false,
      },
      {
          .name = "success: empty string",
          .input = "\"\"",
          .input_len = 2,
          .expected_output = "",
          .expect_null = false,
      },
      {
          .name = "success: complex string with multiple escapes",
          .input = "\"he\\\"llo\\\\\\/\\b\\f\\n\\r\"",
          .input_len = 24,
          .expected_output = "he\"llo\\/\b\f\n\r",
          .expect_null = false,
      },
      {
          .name = "failure: missing closing quote",
          .input = "\"hello",
          .input_len = 6,
          .expected_output = NULL,
          .expect_null = true,
      },
      {
          .name = "failure: invalid escape sequence",
          .input = "\"he\\xllo\"",
          .input_len = 10,
          .expected_output = NULL,
          .expect_null = true,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    const char *end = tc->input + tc->input_len;
    char out_buf[256] = {0};
    string_t out_str = {out_buf, sizeof(out_buf) - 1};
    const char *result = parse_json_str(tc->input, end, &out_str);

    if (tc->expect_null)
    {
      ASSERT_NULL(tc->name, result);
    }
    else
    {
      ASSERT_NOT_NULL(tc->name, result);
      ASSERT_STR_EQ(tc->name, tc->expected_output, out_str.ptr);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_json_int(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_json_int");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    int expected_output;
    bool expect_null;
  } test_cases[] = {
      {
          .name = "success: simple integer",
          .input = "123",
          .input_len = 3,
          .expected_output = 123,
          .expect_null = false,
      },
      {
          .name = "success: negative integer",
          .input = "-456",
          .input_len = 4,
          .expected_output = -456,
          .expect_null = false,
      },
      {
          .name = "success: integer with trailing backquote",
          .input = "123\"",
          .input_len = 4,
          .expected_output = 123,
          .expect_null = false,
      },
      {
          .name = "failure: non-integer input",
          .input = "abc",
          .input_len = 3,
          .expected_output = 0,
          .expect_null = true,
      },
      {
          .name = "failure: empty input",
          .input = "",
          .input_len = 0,
          .expected_output = 0,
          .expect_null = true,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    const char *end = tc->input + tc->input_len;
    int out_value = 0;
    const char *result = parse_json_int(tc->input, end, &out_value);

    if (tc->expect_null)
    {
      ASSERT_NULL(tc->name, result);
    }
    else
    {
      ASSERT_NOT_NULL(tc->name, result);
      ASSERT_EQ(tc->name, out_value, tc->expected_output);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_skip_json_value(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_skip_json_value");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    bool expect_null;
  } test_cases[] = {
      {
          .name = "success: skip string value",
          .input = "\"hello\",",
          .input_len = 8,
          .expect_null = false,
      },
      {
          .name = "success: skip integer value",
          .input = "123,",
          .input_len = 4,
          .expect_null = false,
      },
      {
          .name = "success: skip object value",
          .input = "{\"key\": \"value\"},",
          .input_len = 17,
          .expect_null = false,
      },
      {
          .name = "success: skip array value",
          .input = "[1, 2, 3],",
          .input_len = 10,
          .expect_null = false,
      },
      {
          .name = "success: skip nested object value",
          .input = "{\"outer\": {\"inner\": 42}},",
          .input_len = 25,
          .expect_null = false,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    const char *end = tc->input + tc->input_len;
    const char *result = skip_json_value(tc->input, end);

    if (tc->expect_null)
    {
      ASSERT_NULL(tc->name, result);
    }
    else
    {
      ASSERT_NOT_NULL(tc->name, result);
      ASSERT_STR_EQ(tc->name, result, ",");
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_json_to_challenge(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_json_to_challenge");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    challenge_t expected_output;
    bool expect_failure;
  } test_cases[] = {
      {
          .name = "success: valid challenge JSON",
          .input = "{\"id\": 1, \"creator_id\": \"user123\", \"name\": \"Challenge 1\", \"description\": \"This is a test challenge.\", \"flag\": \"flag{test}\", \"genre\": \"web\"}",
          .input_len = 128,
          .expected_output = {
              .id = 1,
              .creator_id = "user123",
              .name = "Challenge 1",
              .description = "This is a test challenge.",
              .flag = "flag{test}",
              .genre = CTF_GENRE_WEB,
          },
          .expect_failure = false,
      },
      {
          .name = "failure: invalid JSON",
          .input = "{\"id\": 1, \"creator_id\": \"user123\", \"name\": \"Challenge 1\", \"description\": \"This is a test challenge.\", \"flag\": \"flag{test}\", \"genre\": \"web\"",
          .input_len = 127,
          .expected_output = {0},
          .expect_failure = true,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    challenge_t challenge;
    int result = json_to_challenge(tc->input, tc->input_len, &challenge);

    if (tc->expect_failure)
    {
      ASSERT_NEQ("json_to_challenge", result, 0);
    }
    else
    {
      ASSERT_EQ("json_to_challenge", result, 0);
      ASSERT_EQ("challenge.id", challenge.id, tc->expected_output.id);
      ASSERT_STR_EQ("challenge.creator_id", challenge.creator_id, tc->expected_output.creator_id);
      ASSERT_STR_EQ("challenge.name", challenge.name, tc->expected_output.name);
      ASSERT_STR_EQ("challenge.description", challenge.description, tc->expected_output.description);
      ASSERT_STR_EQ("challenge.flag", challenge.flag, tc->expected_output.flag);
      ASSERT_EQ("challenge.genre", challenge.genre, tc->expected_output.genre);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}