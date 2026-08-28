#include "test.h"

#include <stdio.h>
#include <string.h>

#include <http.h>
#include <http_p.h>

#include "util.h"

typedef struct http_parser_internal_state http_parser_internal_state;

void test_parse_method(test_ctx_t *ctx);
void test_parse_version(test_ctx_t *ctx);

void
test_http(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_http")
  ctx->indent += PREFACE_INDENT;

  test_parse_method(ctx);
  test_parse_version(ctx);

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_method(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_method")
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;
    int expected_result;
    http_method expected_method;
    size_t expected_method_len;
  } test_cases[] = {
      {
          .name = "GET",
          .buf = "GET / HTTP/1.1\r\n",
          .buf_len = 16,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_GET,
          .expected_method_len = 3,
      },
      {
          .name = "PUT",
          .buf = "PUT / HTTP/1.1\r\n",
          .buf_len = 16,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_PUT,
          .expected_method_len = 3,
      },
      {
          .name = "POST",
          .buf = "POST / HTTP/1.1\r\n",
          .buf_len = 17,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_POST,
          .expected_method_len = 4,
      },
      {
          .name = "HEAD",
          .buf = "HEAD / HTTP/1.1\r\n",
          .buf_len = 17,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_HEAD,
          .expected_method_len = 4,
      },
      {
          .name = "PATCH",
          .buf = "PATCH / HTTP/1.1\r\n",
          .buf_len = 18,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_PATCH,
          .expected_method_len = 5,
      },
      {
          .name = "TRACE",
          .buf = "TRACE / HTTP/1.1\r\n",
          .buf_len = 18,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_TRACE,
          .expected_method_len = 5,
      },
      {
          .name = "DELETE",
          .buf = "DELETE / HTTP/1.1\r\n",
          .buf_len = 19,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_DELETE,
          .expected_method_len = 6,
      },
      {
          .name = "OPTIONS",
          .buf = "OPTIONS / HTTP/1.1\r\n",
          .buf_len = 20,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_OPTIONS,
          .expected_method_len = 7,
      },
      {
          .name = "CONNECT",
          .buf = "CONNECT / HTTP/1.1\r\n",
          .buf_len = 20,
          .expected_result = 0,
          .expected_method = HTTP_METHOD_CONNECT,
          .expected_method_len = 7,
      },
      {
          .name = "too long method",
          .buf = "TOOLONGMETHOD / HTTP/1.1\r\n",
          .buf_len = 27,
          .expected_result = -1,
          .expected_method = HTTP_METHOD_GET, // dummy value
          .expected_method_len = 0,
      },
      {
          .name = "invalid method",
          .buf = "INVALID / HTTP/1.1\r\n",
          .buf_len = 21,
          .expected_result = -1,
          .expected_method = HTTP_METHOD_GET, // dummy value
          .expected_method_len = 0,
      },
      {
          .name = "empty method",
          .buf = " / HTTP/1.1\r\n",
          .buf_len = 14,
          .expected_result = -1,
          .expected_method = HTTP_METHOD_GET, // dummy value
          .expected_method_len = 0,
      },
      {
          .name = "no method",
          .buf = "",
          .buf_len = 0,
          .expected_result = -1,
          .expected_method = HTTP_METHOD_GET, // dummy value
          .expected_method_len = 0,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    struct test_case *tc = &test_cases[i];

    http_request req;
    memset(&req, 0, sizeof(http_request));

    http_parser_internal_state s;
    memset(&s, 0, sizeof(http_parser_internal_state));
    s.state = STATE_REQ_METHOD;
    req.internal = &s;

    memcpy(s.buf, tc->buf, tc->buf_len);
    s.buf_len = tc->buf_len;

    int result = parse_method(&req, s.buf);
    if (result < 0)
    {
      ASSERT_EQ(tc->name, tc->expected_result, result);
      CHECK_TEST(tc->name);
      continue;
    }

    ASSERT_EQ(tc->name, tc->expected_result, result);
    ASSERT_EQ(tc->name, tc->expected_method, req.method);
    ASSERT_EQ(tc->name, tc->expected_method_len, s.method_len);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_version(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_version")
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;
    int expected_result;
    http_version expected_version;
  } test_cases[] = {
      {
          .name = "HTTP/1.0",
          .buf = "HTTP/1.0",
          .buf_len = 8,
          .expected_result = 0,
          .expected_version = HTTP_VERSION_1_0,
      },
      {
          .name = "HTTP/1.1",
          .buf = "HTTP/1.1",
          .buf_len = 8,
          .expected_result = 0,
          .expected_version = HTTP_VERSION_1_1,
      },
      {
          .name = "invalid version",
          .buf = "HTTP/2.0",
          .buf_len = 8,
          .expected_result = -1,
          .expected_version = HTTP_VERSION_1_0, // dummy value
      },
      {
          .name = "too short version",
          .buf = "HTTP/1.",
          .buf_len = 7,
          .expected_result = -1,
          .expected_version = HTTP_VERSION_1_0, // dummy value
      },
      {
          .name = "too long version",
          .buf = "HTTP/1.10",
          .buf_len = 9,
          .expected_result = -1,
          .expected_version = HTTP_VERSION_1_0, // dummy value
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    struct test_case *tc = &test_cases[i];

    http_request req;
    memset(&req, 0, sizeof(http_request));

    int result = parse_version(&req, tc->buf, tc->buf_len);
    if (result < 0)
    {
      ASSERT_EQ(tc->name, tc->expected_result, result);
      CHECK_TEST(tc->name);
      continue;
    }

    ASSERT_EQ(tc->name, tc->expected_result, result);
    ASSERT_EQ(tc->name, tc->expected_version, req.version);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}