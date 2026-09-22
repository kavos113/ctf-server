#include "test.h"
#include "util.h"

#include <http_response.h>
#include <stdlib.h>
#include <string.h>

static const struct response_case
{
  const char *name;
  const char *content_type;
  const char *body;
  bool bearer_challenge;
  bool no_store;
  const char *expected_header;
} cases[] = {
    {
        .name = "default content type",
        .content_type = NULL,
        .body = "hello",
        .expected_header = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n"
                           "Content-Type: text/plain\r\nConnection: close\r\n\r\n",
    },
    {
        .name = "JSON content type",
        .content_type = "application/json",
        .body = "{}",
        .expected_header = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n"
                           "Content-Type: application/json\r\nConnection: close\r\n\r\n",
    },
    {
        .name = "empty body",
        .content_type = NULL,
        .body = "",
        .expected_header = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"
                           "Content-Type: text/plain\r\nConnection: close\r\n\r\n",
    },
    {
        .name = "Bearer challenge",
        .body = "",
        .bearer_challenge = true,
        .no_store = false,
        .expected_header = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"
                           "Content-Type: text/plain\r\nConnection: close\r\n"
                           "WWW-Authenticate: Bearer\r\n\r\n",
    },
    {
        .name = "uncacheable token",
        .body = "",
        .bearer_challenge = false,
        .no_store = true,
        .expected_header = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"
                           "Content-Type: text/plain\r\nConnection: close\r\n"
                           "Cache-Control: no-store\r\n\r\n",
    },
    {
        .name = "uncacheable challenge",
        .body = "",
        .bearer_challenge = true,
        .no_store = true,
        .expected_header = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"
                           "Content-Type: text/plain\r\nConnection: close\r\n"
                           "WWW-Authenticate: Bearer\r\nCache-Control: no-store\r\n\r\n",
    },
};

static void
test_http_response_build(test_ctx_t *ctx)
{
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const struct response_case *tc = &cases[i];
    http_response_t response = {
        .status = HTTP_STATUS_OK,
        .body = tc->body,
        .body_len = strlen(tc->body),
        .content_type = tc->content_type,
        .bearer_challenge = tc->bearer_challenge,
        .no_store = tc->no_store,
    };
    char *buffer = NULL;
    size_t len = 0;
    size_t header_len = strlen(tc->expected_header);

    ASSERT_EQ(tc->name, (error_code)ERR_NONE, http_response_build(&response, &buffer, &len).code);

    if (buffer)
    {
      ASSERT_EQ(tc->name, header_len + response.body_len, len);
      ASSERT_TRUE(tc->name, len >= header_len);

      if (len >= header_len)
      {
        ASSERT_EQ(tc->name, 0, memcmp(buffer, tc->expected_header, header_len));

        if (len == header_len + response.body_len)
        {
          ASSERT_EQ(tc->name, 0, memcmp(buffer + header_len, tc->body, response.body_len));
        }
      }
    }

    free(buffer);
    CHECK_TEST(tc->name);
  }
}

static void
test_http_response_build_header(test_ctx_t *ctx)
{
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const struct response_case *tc = &cases[i];
    http_response_t response = {
        .status = HTTP_STATUS_OK,
        .body = tc->body,
        .body_len = strlen(tc->body),
        .content_type = tc->content_type,
        .bearer_challenge = tc->bearer_challenge,
        .no_store = tc->no_store,
    };
    char *buffer = NULL;
    size_t len = 0;
    size_t header_len = strlen(tc->expected_header);

    ASSERT_EQ(tc->name, (error_code)ERR_NONE, http_response_build_header(&response, &buffer, &len).code);

    if (buffer)
    {
      ASSERT_EQ(tc->name, header_len, len);
      ASSERT_TRUE(tc->name, len >= header_len);

      if (len >= header_len)
      {
        ASSERT_EQ(tc->name, 0, memcmp(buffer, tc->expected_header, header_len));
      }
    }

    free(buffer);
    CHECK_TEST(tc->name);
  }
}

void
test_http_response(test_ctx_t *ctx)
{
  test_http_response_build(ctx);
  test_http_response_build_header(ctx);
}
