#ifndef TEST_TEST_H
#define TEST_TEST_H

#define PREFACE_INDENT 2
#define TESTCASE_MORE_INDENT 2

typedef struct test_ctx
{
  int detailed;
  int indent;
} test_ctx_t;

void test_http(test_ctx_t *ctx);

#define TEST_PREFACE(name)                                \
  if (ctx->detailed)                                      \
  {                                                       \
    fprintf(stderr, "%*s--- %s\n", ctx->indent, "", name); \
  }

#endif // TEST_TEST_H