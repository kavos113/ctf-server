#include "test.h"

#include <stdio.h>
#include <string.h>

#include <http_request.h>
#include <http_request_p.h>

#include "util.h"

typedef struct http_parser_internal_state http_parser_internal_state;

void test_parse_method(test_ctx_t *ctx);
void test_parse_version(test_ctx_t *ctx);
void test_parse_header(test_ctx_t *ctx);

void test_parse_chunk(test_ctx_t *ctx);
void test_parse_chunk_state_transition(test_ctx_t *ctx);
void test_parse_chunk_parse_method(test_ctx_t *ctx);
void test_parse_chunk_parse_uri(test_ctx_t *ctx);
void test_parse_chunk_parse_version(test_ctx_t *ctx);
void test_parse_chunk_parse_headers(test_ctx_t *ctx);

void
test_http_request(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_http_request");
  ctx->indent += PREFACE_INDENT;

  test_parse_method(ctx);
  test_parse_version(ctx);
  test_parse_header(ctx);
  test_parse_chunk(ctx);

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_method(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_method");
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
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

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
  PRINT_TEST_PREFACE("test_parse_version");
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
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

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

void
test_parse_header(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_header");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    http_header_t header;
    size_t expected_content_length;
    const char *expected_content_type;
    const char *expected_host;
  } test_cases[] = {
      {
          .name = "success: content-length",
          .header = {.name = "Content-Length", .name_len = 14, .value = "123", .value_len = 3},
          .expected_content_length = 123,
          .expected_content_type = NULL,
          .expected_host = NULL,
      },
      {
          .name = "success: content-length (case-insensitive)",
          .header = {.name = "content-length", .name_len = 14, .value = "456", .value_len = 3},
          .expected_content_length = 456,
          .expected_content_type = NULL,
          .expected_host = NULL,
      },
      {
          .name = "success: content-type",
          .header = {.name = "Content-Type", .name_len = 12, .value = "text/html", .value_len = 9},
          .expected_content_length = 0,
          .expected_content_type = "text/html",
          .expected_host = NULL,
      },
      {
          .name = "success: content-type (case-insensitive)",
          .header = {.name = "content-type", .name_len = 12, .value = "application/json", .value_len = 16},
          .expected_content_length = 0,
          .expected_content_type = "application/json",
          .expected_host = NULL,
      },
      {
          .name = "success: host",
          .header = {.name = "Host", .name_len = 4, .value = "example.com", .value_len = 11},
          .expected_content_length = 0,
          .expected_content_type = NULL,
          .expected_host = "example.com",
      },
      {
          .name = "success: host (case-insensitive)",
          .header = {.name = "host", .name_len = 4, .value = "test.com", .value_len = 8},
          .expected_content_length = 0,
          .expected_content_type = NULL,
          .expected_host = "test.com",
      },
      {
          .name = "success: other header",
          .header = {.name = "X-Custom-Header", .name_len = 15, .value = "value", .value_len = 5},
          .expected_content_length = 0,
          .expected_content_type = NULL,
          .expected_host = NULL,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

    parse_header(&req, &tc->header);

    if (tc->expected_content_length > 0)
    {
      ASSERT_EQ(tc->name, tc->expected_content_length, req.content_length);
    }

    if (tc->expected_content_type)
    {
      ASSERT_STR_EQ(tc->name, tc->expected_content_type, req.content_type);
    }

    if (tc->expected_host)
    {
      ASSERT_STR_EQ(tc->name, tc->expected_host, req.host);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_chunk(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_chunk");
  ctx->indent += PREFACE_INDENT;

  test_parse_chunk_state_transition(ctx);
  test_parse_chunk_parse_method(ctx);
  test_parse_chunk_parse_uri(ctx);
  test_parse_chunk_parse_version(ctx);
  test_parse_chunk_parse_headers(ctx);

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_chunk_state_transition(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_chunk_state_transition");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;
    size_t bytes_read;
    parse_state current_state;

    error_code expected_error;
    parse_state expected_state;
  } test_cases[] = {
      {
          .name = "with 1 chunk: request line and headers",
          .buf = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
          .buf_len = 44,
          .bytes_read = 44,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_NONE,
          .expected_state = STATE_HEADER_END,
      },
      {
          .name = "with 1 chunk: method -> uri",
          .buf = "GET /aaaaaaa",
          .buf_len = 12,
          .bytes_read = 12,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_MORE_DATA_NEEDED,
          .expected_state = STATE_REQ_URI,
      },
      {
          .name = "with 1 chunk: uri -> version",
          .buf = "GET /www HTTP/1.",
          .buf_len = 16,
          .bytes_read = 10,
          .current_state = STATE_REQ_URI,
          .expected_error = ERR_MORE_DATA_NEEDED,
          .expected_state = STATE_REQ_VERSION,
      },
      {
          .name = "with 1 chunk: version -> header key",
          .buf = "GET / HTTP/1.1\r\n",
          .buf_len = 16,
          .bytes_read = 16,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_MORE_DATA_NEEDED,
          .expected_state = STATE_HEADER_KEY,
      },
      {
          .name = "with 1 chunk, error: unknown method",
          .buf = "UNKNOWN / HTTP/1.1\r\n",
          .buf_len = 20,
          .bytes_read = 20,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_state = STATE_ERROR,
      },
      {
          .name = "with 1 chunk, error: invalid version",
          .buf = "GET / HTTP/2.0\r\n",
          .buf_len = 16,
          .bytes_read = 16,
          .current_state = STATE_REQ_VERSION,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_state = STATE_ERROR,
      },
      {
          .name = "with 1 chunk, error: single CR",
          .buf = "GET / HTTP/1.1\r",
          .buf_len = 15,
          .bytes_read = 15,
          .current_state = STATE_REQ_VERSION,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_state = STATE_ERROR,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

    http_parser_internal_state s;
    memset(&s, 0, sizeof(http_parser_internal_state));
    s.state = tc->current_state;
    req.internal = &s;

    memcpy(s.buf, tc->buf, tc->buf_len);
    s.buf_len = tc->buf_len;

    error e = parse_chunk(&req, tc->bytes_read);
    ASSERT_EQ(tc->name, tc->expected_error, e.code);
    ASSERT_EQ(tc->name, tc->expected_state, s.state);

    CHECK_TEST(tc->name);
  }

  struct test_case_by_byte
  {
    const char *name;

    const char *buf;
    size_t buf_len;
    parse_state current_state;

    error_code expected_error;
    parse_state expected_state;
  } test_cases_by_byte[] = {
      {
          .name = "with 1 byte at a time: request line and headers",
          .buf = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
          .buf_len = 44,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_NONE,
          .expected_state = STATE_HEADER_END,
      },
      {
          .name = "with 1 byte at a time: method -> uri",
          .buf = "GET /aaaaaaa",
          .buf_len = 12,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_MORE_DATA_NEEDED,
          .expected_state = STATE_REQ_URI,
      },
      {
          .name = "with 1 byte at a time: uri -> version",
          .buf = "GET /www HTTP/1.",
          .buf_len = 16,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_MORE_DATA_NEEDED,
          .expected_state = STATE_REQ_VERSION,
      },
      {
          .name = "with 1 byte at a time: version -> header key",
          .buf = "GET / HTTP/1.1\r\n",
          .buf_len = 16,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_MORE_DATA_NEEDED,
          .expected_state = STATE_HEADER_KEY,
      },
      {
          .name = "with 1 byte at a time, error: unknown method",
          .buf = "UNKNOWN / HTTP/1.1\r\n",
          .buf_len = 20,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_state = STATE_ERROR,
      },
      {
          .name = "with 1 byte at a time, error: invalid version",
          .buf = "GET / HTTP/2.0\r\n",
          .buf_len = 16,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_state = STATE_ERROR,
      },
      {
          .name = "with 1 byte at a time, error: single CR",
          .buf = "GET / HTTP/1.1\raaa",
          .buf_len = 18,
          .current_state = STATE_REQ_METHOD,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_state = STATE_ERROR,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases_by_byte) / sizeof(test_cases_by_byte[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case_by_byte *tc = &test_cases_by_byte[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

    http_parser_internal_state s;
    memset(&s, 0, sizeof(http_parser_internal_state));
    s.state = tc->current_state;
    req.internal = &s;

    memcpy(s.buf, tc->buf, tc->buf_len);
    s.buf_len = 0;

    error e;
    for (size_t j = 0; j < tc->buf_len; j++)
    {
      s.buf_len++;
      e = parse_chunk(&req, 1);
      if (e.code != ERR_MORE_DATA_NEEDED)
      {
        break;
      }
    }

    ASSERT_EQ(tc->name, tc->expected_error, e.code);
    ASSERT_EQ(tc->name, tc->expected_state, s.state);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_chunk_parse_method(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_chunk_parse_method");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;

    error_code expected_error;
    http_method expected_method;
    size_t expected_method_len;
  } test_cases[] = {
      {
          .name = "success: GET method",
          .buf = "GET /aaaaaaa HTTP/1.1\r\n\r\n",
          .buf_len = 25,
          .expected_error = ERR_NONE,
          .expected_method = HTTP_METHOD_GET,
          .expected_method_len = 3,
      },
      {
          .name = "success: POST method",
          .buf = "POST /aaaaaaa HTTP/1.1\r\n\r\n",
          .buf_len = 26,
          .expected_error = ERR_NONE,
          .expected_method = HTTP_METHOD_POST,
          .expected_method_len = 4,
      },
      {
          .name = "error: unknown method",
          .buf = "UNKNOWN /aaaaaaa HTTP/1.1\r\n\r\n",
          .buf_len = 29,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_method = HTTP_METHOD_GET, // dummy value
          .expected_method_len = 0,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

    http_parser_internal_state s;
    memset(&s, 0, sizeof(http_parser_internal_state));
    s.state = STATE_REQ_METHOD;
    req.internal = &s;

    memcpy(s.buf, tc->buf, tc->buf_len);
    s.buf_len = tc->buf_len;

    error e = parse_chunk(&req, tc->buf_len);
    ASSERT_EQ(tc->name, tc->expected_error, e.code);
    ASSERT_EQ(tc->name, tc->expected_method, req.method);
    ASSERT_EQ(tc->name, tc->expected_method_len, s.method_len);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_chunk_parse_uri(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_chunk_parse_uri");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;

    error_code expected_error;
    const char *expected_uri;
    size_t expected_uri_len;
  } test_cases[] = {
      {
          .name = "success: simple uri",
          .buf = "GET /aaaaaaa HTTP/1.1\r\n\r\n",
          .buf_len = 26,
          .expected_error = ERR_NONE,
          .expected_uri = "/aaaaaaa",
          .expected_uri_len = 8,
      },
      {
          .name = "success: complex uri",
          .buf = "GET /path/to/resource?query=param HTTP/1.1\r\n\r\n",
          .buf_len = 46,
          .expected_error = ERR_NONE,
          .expected_uri = "/path/to/resource?query=param",
          .expected_uri_len = 29,
      },
      {
          .name = "error: no uri",
          .buf = "GET  HTTP/1.1\r\n\r\n",
          .buf_len = 17,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_uri = NULL,
          .expected_uri_len = 0,
      },
      {
          .name = "error: too many spaces before uri",
          .buf = "GET    / HTTP/1.1\r\n\r\n",
          .buf_len = 21,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_uri = NULL,
          .expected_uri_len = 0,
      },
      {
          .name = "error: unexpected CR",
          .buf = "GET \raa HTTP/1.1\r\r\n",
          .buf_len = 19,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_uri = NULL,
          .expected_uri_len = 0,
      },
      {
          .name = "error: unexpected LF",
          .buf = "GET \naa HTTP/1.1\n\r\n",
          .buf_len = 19,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_uri = NULL,
          .expected_uri_len = 0,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

    http_parser_internal_state s;
    memset(&s, 0, sizeof(http_parser_internal_state));
    req.internal = &s;

    memcpy(s.buf, tc->buf, tc->buf_len);
    s.buf_len = tc->buf_len;

    error e = parse_chunk(&req, tc->buf_len);
    ASSERT_EQ(tc->name, tc->expected_error, e.code);
    if (e.code == ERR_NONE)
    {
      ASSERT_STR_N_EQ(tc->name, tc->expected_uri, req.uri, tc->expected_uri_len);
      ASSERT_EQ(tc->name, tc->expected_uri_len, req.uri_len);
    }
    else
    {
      ASSERT_EQ(tc->name, 0, req.uri_len);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_chunk_parse_version(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_chunk_parse_version");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;

    error_code expected_error;
    http_version expected_version;
  } test_cases[] = {
      {
          .name = "success: HTTP/1.0",
          .buf = "GET / HTTP/1.0\r\n\r\n",
          .buf_len = 18,
          .expected_error = ERR_NONE,
          .expected_version = HTTP_VERSION_1_0,
      },
      {
          .name = "success: HTTP/1.1",
          .buf = "GET / HTTP/1.1\r\n\r\n",
          .buf_len = 18,
          .expected_error = ERR_NONE,
          .expected_version = HTTP_VERSION_1_1,
      },
      {
          .name = "error: invalid version",
          .buf = "GET / HTTP/2.0\r\n\r\n",
          .buf_len = 18,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_version = HTTP_VERSION_1_0, // dummy value
      },
      {
          .name = "error: not http version",
          .buf = "GET / SOMETEXT\r\n\r\n",
          .buf_len = 18,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_version = HTTP_VERSION_1_0, // dummy value
      }};

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

    http_parser_internal_state s;
    memset(&s, 0, sizeof(http_parser_internal_state));
    req.internal = &s;

    memcpy(s.buf, tc->buf, tc->buf_len);
    s.buf_len = tc->buf_len;

    error e = parse_chunk(&req, tc->buf_len);
    ASSERT_EQ(tc->name, tc->expected_error, e.code);
    if (e.code == ERR_NONE)
    {
      ASSERT_EQ(tc->name, tc->expected_version, req.version);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_chunk_parse_headers(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_chunk_parse_headers");
  ctx->indent += PREFACE_INDENT;

#define TEST_MAX_HEADER_COUNT 10

  struct test_case
  {
    const char *name;

    const char *buf;
    size_t buf_len;

    error_code expected_error;
    http_header_t expected_headers[TEST_MAX_HEADER_COUNT];
    size_t expected_header_count;
  } test_cases[] = {
      {
          .name = "success: single header",
          .buf = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
          .buf_len = 37,
          .expected_error = ERR_NONE,
          .expected_headers = {
              {
                  .name = "Host",
                  .name_len = 4,
                  .value = "example.com",
                  .value_len = 11,
              },
          },
          .expected_header_count = 1,
      },
      {
          .name = "success: multiple headers",
          .buf = "GET / HTTP/1.1\r\nHost: example.com\r\nUser-Agent: TestAgent\r\n\r\n",
          .buf_len = 60,
          .expected_error = ERR_NONE,
          .expected_headers = {
              {
                  .name = "Host",
                  .name_len = 4,
                  .value = "example.com",
                  .value_len = 11,
              },
              {
                  .name = "User-Agent",
                  .name_len = 10,
                  .value = "TestAgent",
                  .value_len = 9,
              },
          },
          .expected_header_count = 2,
      },
      {
          .name = "success: header with no whitespace after colon",
          .buf = "GET / HTTP/1.1\r\nHost:example.com\r\n\r\n",
          .buf_len = 36,
          .expected_error = ERR_NONE,
          .expected_headers = {
              {
                  .name = "Host",
                  .name_len = 4,
                  .value = "example.com",
                  .value_len = 11,
              },
          },
          .expected_header_count = 1,
      },
      {
          .name = "success: header with whitespace after colon",
          .buf = "GET / HTTP/1.1\r\nHost:   example.com\r\n\r\n",
          .buf_len = 40,
          .expected_error = ERR_NONE,
          .expected_headers = {
              {
                  .name = "Host",
                  .name_len = 4,
                  .value = "example.com",
                  .value_len = 11,
              },
          },
          .expected_header_count = 1,
      },
      {
          .name = "success: whitespace after header value: stripping whitespace",
          .buf = "GET / HTTP/1.1\r\nHost: example.com   \n\r\n\r\n",
          .buf_len = 40,
          .expected_error = ERR_NONE,
          .expected_headers = {
              {
                  .name = "Host",
                  .name_len = 4,
                  .value = "example.com",
                  .value_len = 11,
              },
          },
          .expected_header_count = 1,
      },
      {
          .name = "success: whitespace in header value",
          .buf = "GET / HTTP/1.1\r\nAuthorization: Bearer token with spaces\r\n\r\n",
          .buf_len = 59,
          .expected_error = ERR_NONE,
          .expected_headers = {
              {
                  .name = "Authorization",
                  .name_len = 13,
                  .value = "Bearer token with spaces",
                  .value_len = 24,
              },
          },
          .expected_header_count = 1,
      },
      {
          .name = "error: malformed header",
          .buf = "GET / HTTP/1.1\r\nHost example.com\r\n\r\n",
          .buf_len = 46,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_header_count = 0,
      },
      {
          .name = "error: whitespace in header name",
          .buf = "GET / HTTP/1.1\r\nHo st: example.com\r\n\r\n",
          .buf_len = 38,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_header_count = 0,
      },
      {
          .name = "error: unexpected CR in header",
          .buf = "GET / HTTP/1.1\r\nHost: example.com\r\r\n\r\n",
          .buf_len = 38,
          .expected_error = ERR_HTTP_PARSE_FAILED,
          .expected_header_count = 0,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    http_request_t req;
    memset(&req, 0, sizeof(http_request_t));

    http_parser_internal_state s;
    memset(&s, 0, sizeof(http_parser_internal_state));
    req.internal = &s;

    memcpy(s.buf, tc->buf, tc->buf_len);
    s.buf_len = tc->buf_len;

    error e = parse_chunk(&req, tc->buf_len);
    ASSERT_EQ(tc->name, tc->expected_error, e.code);
    if (e.code == ERR_NONE)
    {
      ASSERT_EQ(tc->name, tc->expected_header_count, req.header_count);
      for (size_t j = 0; j < tc->expected_header_count; j++)
      {
        http_header_t *expected = &tc->expected_headers[j];
        http_header_t *actual = &req.headers[j];

        ASSERT_STR_N_EQ(tc->name, expected->name, actual->name, expected->name_len);
        ASSERT_EQ(tc->name, expected->name_len, actual->name_len);

        ASSERT_STR_N_EQ(tc->name, expected->value, actual->value, expected->value_len);
        ASSERT_EQ(tc->name, expected->value_len, actual->value_len);
      }
    }
    else
    {
      ASSERT_EQ(tc->name, 0, req.header_count);
    }

    CHECK_TEST(tc->name);
  }

#undef TEST_MAX_HEADER_COUNT

  ctx->indent -= PREFACE_INDENT;
}