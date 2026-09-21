#include "test.h"

#include <app/str.h>

#include "util.h"
#include "util_app.h"

void test_string_equals(test_ctx_t *ctx);
void test_string_from_cstr(test_ctx_t *ctx);
void test_string_from_cstr_n(test_ctx_t *ctx);
void test_string_from_cstr_dup(test_ctx_t *ctx);
void test_string_from_cstr_dup_n(test_ctx_t *ctx);

void
test_app_str(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_app_str");
  ctx->indent += PREFACE_INDENT;

  test_string_equals(ctx);
  test_string_from_cstr(ctx);
  test_string_from_cstr_n(ctx);
  test_string_from_cstr_dup(ctx);
  test_string_from_cstr_dup_n(ctx);

  ctx->indent -= PREFACE_INDENT;
}

void
test_string_equals(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_string_equals");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    string_t a;
    string_t b;
    bool expected_output;
  } test_cases[] = {
      {
          .name = "equal",
          .a = (string_t){"hello", 5},
          .b = (string_t){"hello", 5},
          .expected_output = true,
      },
      {
          .name = "different lengths",
          .a = (string_t){"hello", 5},
          .b = (string_t){"hello!", 6},
          .expected_output = false,
      },
      {
          .name = "different contents",
          .a = (string_t){"hello", 5},
          .b = (string_t){"world", 5},
          .expected_output = false,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    bool result = string_equals(tc->a, tc->b);
    ASSERT_EQ(tc->name, result, tc->expected_output);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_string_from_cstr(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_string_from_cstr");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    string_t expected_output;
  } test_cases[] = {
      {
          .name = "simple string",
          .input = "hello",
          .expected_output = (string_t){"hello", 5},
      },
      {
          .name = "empty string",
          .input = "",
          .expected_output = (string_t){"", 0},
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    string_t result = string_from_cstr(tc->input);
    ASSERT_STRING_EQ(tc->name, tc->expected_output, result);

    // should same memory
    // ASSERT_EQ(tc->name, result.ptr, tc->expected_output.ptr);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_string_from_cstr_n(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_string_from_cstr_n");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t n;
    string_t expected_output;
  } test_cases[] = {
      {
          .name = "simple string",
          .input = "hello",
          .n = 5,
          .expected_output = (string_t){"hello", 5},
      },
      {
          .name = "empty string",
          .input = "",
          .n = 0,
          .expected_output = (string_t){"", 0},
      },
      {
          .name = "partial string",
          .input = "hello world",
          .n = 5,
          .expected_output = (string_t){"hello", 5},
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    string_t result = string_from_cstr_n(tc->input, tc->n);
    ASSERT_STRING_EQ(tc->name, tc->expected_output, result);

    

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}