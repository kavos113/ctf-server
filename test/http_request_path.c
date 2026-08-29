#include "test.h"

#include <http_request.h>
#include <http_request_p.h>

#include "util.h"

void test_url_decode(test_ctx_t *ctx);
void test_normalize_path(test_ctx_t *ctx);
void test_parse_query_params(test_ctx_t *ctx);
void test_normalize_uri(test_ctx_t *ctx);

void
test_http_request_path(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_http_request_path");
  ctx->indent += PREFACE_INDENT;

  test_url_decode(ctx);
  test_normalize_path(ctx);
  test_parse_query_params(ctx);
  test_normalize_uri(ctx);

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
          .buf_len = 17,
          .expected_result = 13,
          .expected_normalized = "/path/to/file",
      },
      {
          .name = "success: with double dot segments",
          .buf = "/path/to/../file",
          .buf_len = 16,
          .expected_result = 10,
          .expected_normalized = "/path/file",
      },
      {
          .name = "success: remove multiple slashes",
          .buf = "/path//to///file",
          .buf_len = 16,
          .expected_result = 13,
          .expected_normalized = "/path/to/file",
      },
      {
          .name = "success: path with trailing slash",
          .buf = "/path/to/directory/",
          .buf_len = 19,
          .expected_result = 18,
          .expected_normalized = "/path/to/directory",
      },
      {
          .name = "error: directory traversal",
          .buf = "/path/to/../../../../etc/passwd",
          .buf_len = 31,
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

void
test_parse_query_params(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_query_params");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *uri;
    size_t uri_len;
    size_t expected_param_count;
    http_param_t expected_params[4];
  } test_cases[] = {
      {
          .name = "success: single query param",
          .uri = "/path?param1=value1",
          .uri_len = 19,
          .expected_param_count = 1,
          .expected_params = {
              {.name = "param1", .name_len = 6, .value = "value1", .value_len = 6},
          },
      },
      {
          .name = "success: multiple query params",
          .uri = "/path?param1=value1&param2=value2",
          .uri_len = 33,
          .expected_param_count = 2,
          .expected_params = {
              {.name = "param1", .name_len = 6, .value = "value1", .value_len = 6},
              {.name = "param2", .name_len = 6, .value = "value2", .value_len = 6},
          },
      },
      {
          .name = "success: query param without value",
          .uri = "/path?param1",
          .uri_len = 12,
          .expected_param_count = 0,
      },
      {
          .name = "success: no query params",
          .uri = "/path",
          .uri_len = 5,
          .expected_param_count = 0,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(req));
    req.uri = tc->uri;
    req.uri_len = tc->uri_len;

    parse_query_params(&req);

    ASSERT_EQ(tc->name, tc->expected_param_count, req.query_param_count);
    for (size_t j = 0; j < tc->expected_param_count; j++)
    {
      http_param_t *expected = &tc->expected_params[j];
      http_param_t *actual = &req.query_params[j];

      ASSERT_STR_N_EQ(tc->name, expected->name, actual->name, expected->name_len);
      ASSERT_EQ(tc->name, expected->name_len, actual->name_len);
      ASSERT_STR_N_EQ(tc->name, expected->value, actual->value, expected->value_len);
      ASSERT_EQ(tc->name, expected->value_len, actual->value_len);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_normalize_uri(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_normalize_uri");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    char *uri;
    size_t uri_len;
    int expected_result;
    char *expected_normalized;
    size_t expected_normalized_len;
    size_t expected_query_param_count;
  } test_cases[] = {
      {
          .name = "success: simple uri",
          .uri = "/simple/path",
          .uri_len = 12,
          .expected_result = 0,
          .expected_normalized = "/simple/path",
          .expected_normalized_len = 12,
          .expected_query_param_count = 0,
      },
      {
          .name = "success: uri with query params",
          .uri = "/path/to/resource?param1=value1&param2=value2",
          .uri_len = 45,
          .expected_result = 0,
          .expected_normalized = "/path/to/resource",
          .expected_normalized_len = 17,
          .expected_query_param_count = 2,
      },
      {
          .name = "success: uri with dot segments",
          .uri = "/path/./to/./resource",
          .uri_len = 21,
          .expected_result = 0,
          .expected_normalized = "/path/to/resource",
          .expected_normalized_len = 17,
          .expected_query_param_count = 0,
      },
      {
          .name = "success: uri with dot and query params",
          .uri = "/path/./to/./resource?param=value",
          .uri_len = 33,
          .expected_result = 0,
          .expected_normalized = "/path/to/resource",
          .expected_normalized_len = 17,
          .expected_query_param_count = 1,
      },
      {
          .name = "success: uri with percent encoding",
          .uri = "/path/to/%72esource?param=value",
          .uri_len = 31,
          .expected_result = 0,
          .expected_normalized = "/path/to/resource",
          .expected_normalized_len = 17,
          .expected_query_param_count = 1,
      },
      {
          .name = "success: absolute form",
          .uri = "http://example.com/path/to/resource?param=value",
          .uri_len = 47,
          .expected_result = 0,
          .expected_normalized = "/path/to/resource",
          .expected_normalized_len = 17,
          .expected_query_param_count = 1,
      },
      {
          .name = "success: https absolute form",
          .uri = "https://example.com/path/to/resource?param=value",
          .uri_len = 48,
          .expected_result = 0,
          .expected_normalized = "/path/to/resource",
          .expected_normalized_len = 17,
          .expected_query_param_count = 1,
      },
      {
          .name = "success: absolute form with no path",
          .uri = "http://example.com?param=value",
          .uri_len = 30,
          .expected_result = 0,
          .expected_normalized = "/",
          .expected_normalized_len = 1,
          .expected_query_param_count = 1,
      },
      {
          .name = "success: hash fragment in uri",
          .uri = "/path/to/resource?param=value#fragment",
          .uri_len = 38,
          .expected_result = 0,
          .expected_normalized = "/path/to/resource",
          .expected_normalized_len = 17,
          .expected_query_param_count = 1,
      },
      {
          .name = "success: hash is in front of query params",
          .uri = "/path/to/resource#fragment?param=value",
          .uri_len = 38,
          .expected_result = 0,
          .expected_normalized = "/path/to/resource",
          .expected_normalized_len = 17,
          .expected_query_param_count = 1,
      },
      {
          .name = "error: directory traversal",
          .uri = "/path/to/../../../../etc/passwd",
          .uri_len = 31,
          .expected_result = -1,
          .expected_normalized = NULL,
          .expected_normalized_len = 0,
          .expected_query_param_count = 0,
      },
      {
          .name = "error: empty uri",
          .uri = "",
          .uri_len = 0,
          .expected_result = -1,
          .expected_normalized = NULL,
          .expected_normalized_len = 0,
          .expected_query_param_count = 0,
      },
      {
          .name = "error: uri not starting with slash",
          .uri = "no/leading/slash",
          .uri_len = 16,
          .expected_result = -1,
          .expected_normalized = NULL,
          .expected_normalized_len = 0,
          .expected_query_param_count = 0,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    char buf[64];
    sprintf(buf, "GET %.*s HTTP/1.1\r\n", (int)tc->uri_len, tc->uri);

    http_request_t req;
    memset(&req, 0, sizeof(req));
    req.uri = buf + 4; // skip "GET "
    req.uri_len = tc->uri_len;

    int result = normalize_uri(&req);
    ASSERT_EQ(tc->name, tc->expected_result, result);
    if (result < 0)
    {
      CHECK_TEST(tc->name);
      continue;
    }

    ASSERT_EQ(tc->name, tc->expected_normalized_len, req.uri_len);
    ASSERT_STR_N_EQ(tc->name, tc->expected_normalized, req.uri, tc->expected_normalized_len);
    ASSERT_EQ(tc->name, tc->expected_query_param_count, req.query_param_count);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}