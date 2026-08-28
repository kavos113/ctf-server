#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ(name, expected, actual)                                                                         \
  if (!ctx->is_canceled && (expected) != (actual))                                                                \
  {                                                                                                               \
    ctx->is_canceled = true;                                                                                      \
    fprintf(stderr, "Assertion failed (%s): %s == %s, at %s:%d\n", name, #expected, #actual, __FILE__, __LINE__); \
  }

#define ASSERT_NE(name, expected, actual)                                                                         \
  if (!ctx->is_canceled && (expected) == (actual))                                                                \
  {                                                                                                               \
    ctx->is_canceled = true;                                                                                      \
    fprintf(stderr, "Assertion failed (%s): %s != %s, at %s:%d\n", name, #expected, #actual, __FILE__, __LINE__); \
  }

#endif // TEST_UTIL_H