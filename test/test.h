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

void test_http_request(test_ctx_t *ctx);
void test_http_request_path(test_ctx_t *ctx);

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