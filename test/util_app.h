#ifndef TEST_UTIL_APP_H
#define TEST_UTIL_APP_H

#include "util.h"

#define ASSERT_STRING_EQ(name, expected, actual)                   \
  do                                                               \
  {                                                                \
    ASSERT_EQ(name, expected.len, actual.len);                     \
    ASSERT_STR_N_EQ(name, expected.ptr, actual.ptr, expected.len); \
  } while (0)

#define ASSERT_CHALLENGE_EQ(test_name, expected, actual)                   \
  do                                                                  \
  {                                                                   \
    ASSERT_EQ(test_name, expected.id, actual.id);                          \
    ASSERT_STRING_EQ(test_name, expected.creator_id, actual.creator_id);   \
    ASSERT_STRING_EQ(test_name, expected.name, actual.name);               \
    ASSERT_STRING_EQ(test_name, expected.description, actual.description); \
    ASSERT_STRING_EQ(test_name, expected.flag, actual.flag);               \
    ASSERT_EQ(test_name, expected.genre, actual.genre);                    \
  } while (0)

#endif // TEST_UTIL_APP_H