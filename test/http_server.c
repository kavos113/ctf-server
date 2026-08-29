#include "test.h"

#include <http_server.h>
#include <http_server_p.h>

#include "util.h"

void test_url_decode(test_ctx_t *ctx);
void test_normalize_path(test_ctx_t *ctx);

void
test_http_server(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_http_server");
  ctx->indent += PREFACE_INDENT;

  test_url_decode(ctx);
  test_normalize_path(ctx);

  ctx->indent -= PREFACE_INDENT;
}

void
test_url_decode(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_url_decode");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;
    ssize_t expected_result;
    const char *expected_decoded;
  } test_cases[] = {
      {
          .name = "success: simple",
          .buf = "hello%20world",
          .buf_len = 13,
          .expected_result = 11,
          .expected_decoded = "hello world",
      },
      {
          .name = "success: multiple percent encodings",
          .buf = "multiple%20percent%20encodings",
          .buf_len = 30,
          .expected_result = 26,
          .expected_decoded = "multiple percent encodings",
      },
      {
          .name = "success: no percent encoding",
          .buf = "no_percent_encoding",
          .buf_len = 19,
          .expected_result = 19,
          .expected_decoded = "no_percent_encoding",
      },
      {
          .name = "error: invalid hex",
          .buf = "invalid%2Ghex",
          .buf_len = 14,
          .expected_result = -1,
          .expected_decoded = NULL,
      },
      {
          .name = "error: incomplete percent encoding",
          .buf = "incomplete%",
          .buf_len = 11,
          .expected_result = -1,
          .expected_decoded = NULL,
      },
      {
          .name = "error: null byte in decoded string",
          .buf = "null%00byte",
          .buf_len = 11,
          .expected_result = -1,
          .expected_decoded = NULL,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    char buf[64];
    memcpy(buf, tc->buf, tc->buf_len);

    ssize_t result = url_decode(buf, tc->buf_len);
    ASSERT_EQ(tc->name, tc->expected_result, result);

    if (result >= 0)
    {
      ASSERT_STR_N_EQ(tc->name, tc->expected_decoded, buf, (size_t)result);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_normalize_path(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_normalize_path");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;
    ssize_t expected_result;
    const char *expected_normalized;
  } test_cases[] = {
      {
          .name = "success: do not change",
          .buf = "/simple/path",
          .buf_len = 12,
          .expected_result = 12,
          .expected_normalized = "/simple/path",
      },
      {
          .name = "success: with dot segments",
          .buf = "/path/./to/./file",
          .buf_len = 18,
          .expected_result = 14,
          .expected_normalized = "/path/to/file",
      },
      {
          .name = "success: with double dot segments",
          .buf = "/path/to/../file",
          .buf_len = 17,
          .expected_result = 11,
          .expected_normalized = "/path/file",
      },
      {
          .name = "success: remove multiple slashes",
          .buf = "/path//to///file",
          .buf_len = 17,
          .expected_result = 14,
          .expected_normalized = "/path/to/file",
      },
      {
          .name = "success: path with trailing slash",
          .buf = "/path/to/directory/",
          .buf_len = 19,
          .expected_result = 19,
          .expected_normalized = "/path/to/directory/",
      },
      {
          .name = "error: directory traversal",
          .buf = "/path/to/../../../../etc/passwd",
          .buf_len = 27,
          .expected_result = -1,
          .expected_normalized = NULL,
      },
      {
          .name = "error: empty path",
          .buf = "",
          .buf_len = 0,
          .expected_result = -1,
          .expected_normalized = NULL,
      },
      {
          .name = "error: path not starting with slash",
          .buf = "no/leading/slash",
          .buf_len = 16,
          .expected_result = -1,
          .expected_normalized = NULL,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    char buf[64];
    memcpy(buf, tc->buf, tc->buf_len);

    ssize_t result = normalize_path(buf, tc->buf_len);
    ASSERT_EQ(tc->name, tc->expected_result, result);

    if (result >= 0)
    {
      ASSERT_STR_N_EQ(tc->name, tc->expected_normalized, buf, (size_t)result);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}