#ifndef TEST_TEST_H
#define TEST_TEST_H

#define PREFACE_INDENT       2
#define TESTCASE_MORE_INDENT 4

#include <stdbool.h>
#include <stdio.h>

typedef struct test_ctx
{
  bool detailed;
  int indent;

  bool is_canceled;
  int failed_count;
  int passed_count;
} test_ctx_t;

void test_password_worker(test_ctx_t *ctx);
void test_app_auth_api(test_ctx_t *ctx);
void test_app_contest(test_ctx_t *ctx);
void test_app_user(test_ctx_t *ctx);
void test_app_auth(test_ctx_t *ctx);
void test_app_json_auth(test_ctx_t *ctx);
void test_auth_libraries(test_ctx_t *ctx);
void test_app_auth_repository(test_ctx_t *ctx);
void test_server_lifetime(test_ctx_t *ctx);

void test_set_calloc_failure(int after);
void test_set_malloc_failure(int after);
void test_app_answer(test_ctx_t *ctx);
void test_app_create_challenge(test_ctx_t *ctx);
void test_db(test_ctx_t *ctx);
void test_app_challenge_write(test_ctx_t *ctx);
void test_http_request(test_ctx_t *ctx);
void test_http_response(test_ctx_t *ctx);
void test_http_request_path(test_ctx_t *ctx);
void test_app_json(test_ctx_t *ctx);
void test_app_str(test_ctx_t *ctx);

#define PRINT_TEST_PREFACE(name)                             \
  do                                                         \
  {                                                          \
    if (ctx->detailed)                                       \
    {                                                        \
      fprintf(stderr, "%*s--- %s\n", ctx->indent, "", name); \
    }                                                        \
  } while (0)

#define PRINT_TEST_PASS(name)                                                         \
  do                                                                                  \
  {                                                                                   \
    if (ctx->detailed)                                                                \
    {                                                                                 \
      fprintf(stderr, "%*sPASS: %s\n", ctx->indent + TESTCASE_MORE_INDENT, "", name); \
    }                                                                                 \
  } while (0)

#define PRINT_TEST_FAIL(name)                                                         \
  do                                                                                  \
  {                                                                                   \
    if (ctx->detailed)                                                                \
    {                                                                                 \
      fprintf(stderr, "%*sFAIL: %s\n", ctx->indent + TESTCASE_MORE_INDENT, "", name); \
    }                                                                                 \
  } while (0)

#define CHECK_TEST(name)     \
  do                         \
  {                          \
    if (ctx->is_canceled)    \
    {                        \
      ctx->failed_count++;   \
      PRINT_TEST_FAIL(name); \
    }                        \
    else                     \
    {                        \
      ctx->passed_count++;   \
      PRINT_TEST_PASS(name); \
    }                        \
  } while (0)

#endif // TEST_TEST_H