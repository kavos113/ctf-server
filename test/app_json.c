#include "test.h"

#include <app/json.h>
#include <app/json_p.h>

#include "util.h"

void test_skip_whitespace(test_ctx_t *ctx);
void test_parse_json_str(test_ctx_t *ctx);

void
test_app_json(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_app_json");
  ctx->indent += PREFACE_INDENT;

  test_skip_whitespace(ctx);
  test_parse_json_str(ctx);

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