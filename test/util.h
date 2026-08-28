#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRINT_ASSERT(x, name, file, line, exp, act) _Generic((x),                                                                                                     \
    int: fprintf(stderr, "[FAIL] %s: expected %d, actual %d, at %s:%d\n", name, (int)(exp), (int)(act), file, line),                                                  \
    long: fprintf(stderr, "[FAIL] %s: expected %ld, actual %ld, at %s:%d\n", name, (long)(exp), (long)(act), file, line),                                             \
    long long: fprintf(stderr, "[FAIL] %s: expected %lld, actual %lld, at %s:%d\n", name, (long long)(exp), (long long)(act), file, line),                            \
    unsigned int: fprintf(stderr, "[FAIL] %s: expected %u, actual %u, at %s:%d\n", name, (unsigned int)(exp), (unsigned int)(act), file, line),                       \
    unsigned long: fprintf(stderr, "[FAIL] %s: expected %lu, actual %lu, at %s:%d\n", name, (unsigned long)(exp), (unsigned long)(act), file, line),                  \
    unsigned long long: fprintf(stderr, "[FAIL] %s: expected %llu, actual %llu, at %s:%d\n", name, (unsigned long long)(exp), (unsigned long long)(act), file, line), \
    double: fprintf(stderr, "[FAIL] %s: expected %f, actual %f, at %s:%d\n", name, (double)(exp), (double)(act), file, line),                                         \
    float: fprintf(stderr, "[FAIL] %s: expected %f, actual %f, at %s:%d\n", name, (float)(exp), (float)(act), file, line),                                            \
    char: fprintf(stderr, "[FAIL] %s: expected '%c', actual '%c', at %s:%d\n", name, (char)(exp), (char)(act), file, line),                                           \
    char *: fprintf(stderr, "[FAIL] %s: expected \"%s\", actual \"%s\", at %s:%d\n", name, (char *)(exp), (char *)(act), file, line),                                 \
    const char *: fprintf(stderr, "[FAIL] %s: expected \"%s\", actual \"%s\", at %s:%d\n", name, (const char *)(exp), (const char *)(act), file, line),               \
    default: fprintf(stderr, "[FAIL] %s: expected %p, actual %p, at %s:%d\n", name, (void *)(exp), (void *)(act), file, line))

#define ASSERT_EQ(name, expected, actual)                       \
  do                                                            \
  {                                                             \
    __typeof__(expected) _exp = (expected);                     \
    __typeof__(actual) _act = (actual);                         \
    if (!ctx->is_canceled && _exp != _act)                      \
    {                                                           \
      ctx->is_canceled = true;                                  \
      PRINT_ASSERT(_exp, name, __FILE__, __LINE__, _exp, _act); \
    }                                                           \
  } while (0)

#define ASSERT_NE(name, expected, actual)                       \
  do                                                            \
  {                                                             \
    __typeof__(expected) _exp = (expected);                     \
    __typeof__(actual) _act = (actual);                         \
    if (!ctx->is_canceled && _exp == _act)                      \
    {                                                           \
      ctx->is_canceled = true;                                  \
      PRINT_ASSERT(_exp, name, __FILE__, __LINE__, _exp, _act); \
    }                                                           \
  } while (0)

#define ASSERT_TRUE(name, condition)                                                                   \
  do                                                                                                   \
  {                                                                                                    \
    if (!ctx->is_canceled && !(condition))                                                             \
    {                                                                                                  \
      ctx->is_canceled = true;                                                                         \
      fprintf(stderr, "[FAIL] %s: expected true, actual false, at %s:%d\n", name, __FILE__, __LINE__); \
    }                                                                                                  \
  } while (0)

#define ASSERT_FALSE(name, condition)                                                                  \
  do                                                                                                   \
  {                                                                                                    \
    if (!ctx->is_canceled && (condition))                                                              \
    {                                                                                                  \
      ctx->is_canceled = true;                                                                         \
      fprintf(stderr, "[FAIL] %s: expected false, actual true, at %s:%d\n", name, __FILE__, __LINE__); \
    }                                                                                                  \
  } while (0)

#define ASSERT_STR_EQ(name, expected, actual)                                                                                   \
  do                                                                                                                            \
  {                                                                                                                             \
    if (!ctx->is_canceled && strcmp((expected), (actual)) != 0)                                                                 \
    {                                                                                                                           \
      ctx->is_canceled = true;                                                                                                  \
      fprintf(stderr, "[FAIL] %s: expected \"%s\", actual \"%s\", at %s:%d\n", name, (expected), (actual), __FILE__, __LINE__); \
    }                                                                                                                           \
  } while (0)

#define ASSERT_STR_NE(name, expected, actual)                                                                                                \
  do                                                                                                                                         \
  {                                                                                                                                          \
    if (!ctx->is_canceled && strcmp((expected), (actual)) == 0)                                                                              \
    {                                                                                                                                        \
      ctx->is_canceled = true;                                                                                                               \
      fprintf(stderr, "[FAIL] %s: expected not equal to \"%s\", actual \"%s\", at %s:%d\n", name, (expected), (actual), __FILE__, __LINE__); \
    }                                                                                                                                        \
  } while (0)

#define ASSERT_STR_N_EQ(name, expected, actual, n)                                                                                                 \
  do                                                                                                                                  \
  {                                                                                                                                   \
    if (!ctx->is_canceled && strncmp((expected), (actual), (n)) != 0)                                                                \
    {                                                                                                                                 \
      ctx->is_canceled = true;                                                                                                        \
      fprintf(stderr, "[FAIL] %s: expected not equal to \"%.*s\", actual \"%.*s\", at %s:%d\n", name, (int)(n), (expected), (int)(n), (actual), __FILE__, __LINE__); \
    }                                                                                                                                 \
  } while (0)

#define ASSERT_STR_N_NE(name, expected, actual, n)                                                                                                 \
  do                                                                                                                                  \
  {                                                                                                                                   \
    if (!ctx->is_canceled && strncmp((expected), (actual), (n)) == 0)                                                                \
    {                                                                                                                                 \
      ctx->is_canceled = true;                                                                                                        \
      fprintf(stderr, "[FAIL] %s: expected equal to \"%.*s\", actual \"%.*s\", at %s:%d\n", name, (int)(n), (expected), (int)(n), (actual), __FILE__, __LINE__); \
    }                                                                                                                                 \
  } while (0)

#endif // TEST_UTIL_H