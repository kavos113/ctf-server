#ifndef TEST_TEST_H
#define TEST_TEST_H

#define PREFACE_INDENT       2
#define TESTCASE_MORE_INDENT 4

#include <stdbool.h>

typedef struct test_ctx
{
  bool detailed;
  int indent;

  bool is_canceled;
  int failed_count;
  int passed_count;
} test_ctx_t;

void test_http(test_ctx_t *ctx);

#define PRINT_TEST_PREFACE(name)                           \
  if (ctx->detailed)                                       \
  {                                                        \
    fprintf(stderr, "%*s--- %s\n", ctx->indent, "", name); \
  }

#define PRINT_TEST_PASS(name)                                                       \
  if (ctx->detailed)                                                                \
  {                                                                                 \
    fprintf(stderr, "%*sPASS: %s\n", ctx->indent + TESTCASE_MORE_INDENT, "", name); \
  }

#define PRINT_TEST_FAIL(name)                                                       \
  if (ctx->detailed)                                                                \
  {                                                                                 \
    fprintf(stderr, "%*sFAIL: %s\n", ctx->indent + TESTCASE_MORE_INDENT, "", name); \
  }

#define CHECK_TEST(name)   \
  if (ctx->is_canceled)    \
  {                        \
    ctx->failed_count++;   \
    PRINT_TEST_FAIL(name); \
  }                        \
  else                     \
  {                        \
    ctx->passed_count++;   \
    PRINT_TEST_PASS(name); \
  }

#endif // TEST_TEST_H