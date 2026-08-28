#ifndef TEST_TEST_H
#define TEST_TEST_H

#define PRINT_INDENT 2

typedef struct test_ctx
{
  int detailed;
  int indent;
} test_ctx_t;

void test_http(test_ctx_t *ctx);

#define TEST_PREFACE(name)                                \
  if (ctx->detailed)                                      \
  {                                                       \
    fprintf(stderr, "%*s --- %s", ctx->indent, "", name); \
  }

#endif // TEST_TEST_H