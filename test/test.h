#ifndef TEST_TEST_H
#define TEST_TEST_H

#define PREFACE_INDENT       2
#define TESTCASE_MORE_INDENT 4

typedef struct test_ctx
{
  int detailed;
  int indent;
} test_ctx_t;

void test_http(test_ctx_t *ctx);

#define PRINT_TEST_PREFACE(name)                           \
  if (ctx->detailed)                                       \
  {                                                        \
    fprintf(stderr, "%*s--- %s\n", ctx->indent, "", name); \
  }

#define PRINT_TEST_PASS(name) \
  if (ctx->detailed)                                       \
  {                                                        \
    fprintf(stderr, "%*sPASS: %s\n", ctx->indent + TESTCASE_MORE_INDENT, "", name); \
  }

#define PRINT_TEST_FAIL(name, msg) \
  if (ctx->detailed)                                       \
  {                                                        \
    fprintf(stderr, "%*sFAIL: %s (%s)\n", ctx->indent + TESTCASE_MORE_INDENT, "", name, msg); \
  }

#endif // TEST_TEST_H