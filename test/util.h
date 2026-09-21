#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void
print_fail_int(const char *name, const char *file, int line, long long expected, long long actual)
{
  fprintf(stderr, "[FAIL] %s: expected %lld, actual %lld, at %s:%d\n", name, expected, actual, file, line);
}

static inline void
print_fail_uint(const char *name, const char *file, int line, unsigned long long expected, unsigned long long actual)
{
  fprintf(stderr, "[FAIL] %s: expected %llu, actual %llu, at %s:%d\n", name, expected, actual, file, line);
}

static inline void
print_fail_double(const char *name, const char *file, int line, double expected, double actual)
{
  fprintf(stderr, "[FAIL] %s: expected %f, actual %f, at %s:%d\n", name, expected, actual, file, line);
}

static inline void
print_fail_str(const char *name, const char *file, int line, const char *expected, const char *actual)
{
  fprintf(stderr, "[FAIL] %s: expected \"%s\", actual \"%s\", at %s:%d\n", name, expected, actual, file, line);
}

static inline void
print_fail_ptr(const char *name, const char *file, int line, const void *expected, const void *actual)
{
  fprintf(stderr, "[FAIL] %s: expected %p, actual %p, at %s:%d\n", name, expected, actual, file, line);
}

static inline void
print_fail_bool(const char *name, const char *file, int line, bool expected, bool actual)
{
  fprintf(stderr, "[FAIL] %s: expected %s, actual %s, at %s:%d\n", name, expected ? "true" : "false", actual ? "true" : "false", file, line);
}

#define PRINT_ASSERT(x, name, file, line, exp, act) _Generic((x), \
    int: print_fail_int,                                          \
    long: print_fail_int,                                         \
    long long: print_fail_int,                                    \
    unsigned int: print_fail_uint,                                \
    unsigned long: print_fail_uint,                               \
    unsigned long long: print_fail_uint,                          \
    float: print_fail_double,                                     \
    double: print_fail_double,                                    \
    const char *: print_fail_str,                                 \
    char *: print_fail_str,                                       \
    bool: print_fail_bool,                                        \
    void *: print_fail_ptr)(name, file, line, exp, act)

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

#define ASSERT_NULL(name, ptr)                                                                                     \
  do                                                                                                               \
  {                                                                                                                \
    if (!ctx->is_canceled && (ptr) != NULL)                                                                        \
    {                                                                                                              \
      ctx->is_canceled = true;                                                                                     \
      fprintf(stderr, "[FAIL] %s: expected NULL, actual %p, at %s:%d\n", name, (void *)(ptr), __FILE__, __LINE__); \
    }                                                                                                              \
  } while (0)

#define ASSERT_NOT_NULL(name, ptr)                                                                        \
  do                                                                                                      \
  {                                                                                                       \
    if (!ctx->is_canceled && (ptr) == NULL)                                                               \
    {                                                                                                     \
      ctx->is_canceled = true;                                                                            \
      fprintf(stderr, "[FAIL] %s: expected not NULL, actual NULL, at %s:%d\n", name, __FILE__, __LINE__); \
    }                                                                                                     \
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

#define ASSERT_STR_N_EQ(name, expected, actual, n)                                                                                                               \
  do                                                                                                                                                             \
  {                                                                                                                                                              \
    if (!ctx->is_canceled && strncmp((expected), (actual), (n)) != 0)                                                                                            \
    {                                                                                                                                                            \
      ctx->is_canceled = true;                                                                                                                                   \
      fprintf(stderr, "[FAIL] %s: expected equal to \"%.*s\", actual \"%.*s\", at %s:%d\n", name, (int)(n), (expected), (int)(n), (actual), __FILE__, __LINE__); \
    }                                                                                                                                                            \
  } while (0)

#define ASSERT_STR_N_NE(name, expected, actual, n)                                                                                                                   \
  do                                                                                                                                                                 \
  {                                                                                                                                                                  \
    if (!ctx->is_canceled && strncmp((expected), (actual), (n)) == 0)                                                                                                \
    {                                                                                                                                                                \
      ctx->is_canceled = true;                                                                                                                                       \
      fprintf(stderr, "[FAIL] %s: expected not equal to \"%.*s\", actual \"%.*s\", at %s:%d\n", name, (int)(n), (expected), (int)(n), (actual), __FILE__, __LINE__); \
    }                                                                                                                                                                \
  } while (0)

#endif // TEST_UTIL_H