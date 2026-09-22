#include "mysql_result_mock.h"
#include "test.h"
#include "util.h"

#include <app/repository.h>

#define USER_ID    "0123456789abcdef0123456789abcdef"
#define SESSION_ID "fedcba9876543210fedcba9876543210"

static void
test_bind_users(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *id;
    const char *username;
    const char *hash;
    size_t rows;
    unsigned fields;
    bool db_error;
    int calloc_failure;
    int malloc_failure;
    bool success;
  } cases[] = {
      {.name = "empty", .rows = 0, .fields = 3, .calloc_failure = -1, .malloc_failure = -1, .success = true},
      {.name = "owned copies", .id = USER_ID, .username = "Alice", .hash = "$argon2id$test", .rows = 2, .fields = 3, .calloc_failure = -1, .malloc_failure = -1, .success = true},
      {.name = "wrong fields", .fields = 2, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "DB error", .fields = 3, .db_error = true, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "NULL id", .username = "Alice", .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "short id", .id = "abc", .username = "Alice", .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "nonhex id", .id = "z123456789abcdef0123456789abcdef", .username = "Alice", .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "NULL name", .id = USER_ID, .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "short name", .id = USER_ID, .username = "ab", .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "invalid name", .id = USER_ID, .username = "a b", .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "NULL hash", .id = USER_ID, .username = "Alice", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "invalid hash", .id = USER_ID, .username = "Alice", .hash = "$hash\n", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = -1},
      {.name = "array allocation", .id = USER_ID, .username = "Alice", .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = 0, .malloc_failure = -1},
      {.name = "name allocation", .id = USER_ID, .username = "Alice", .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = 0},
      {.name = "hash allocation", .id = USER_ID, .username = "Alice", .hash = "$hash", .rows = 1, .fields = 3, .calloc_failure = -1, .malloc_failure = 1},
      {.name = "second row allocation", .id = USER_ID, .username = "Alice", .hash = "$hash", .rows = 2, .fields = 3, .calloc_failure = -1, .malloc_failure = 2},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const char *values[2][6] = {
        {cases[i].id, cases[i].username, cases[i].hash},
        {cases[i].id, cases[i].username, cases[i].hash},
    };
    db_result_t result = {.success = !cases[i].db_error,
                          .res = test_mysql_result(values, cases[i].rows, cases[i].fields)};
    user_t *users = NULL;
    size_t count = 99;
    test_set_calloc_failure(cases[i].calloc_failure);
    test_set_malloc_failure(cases[i].malloc_failure);
    bool success = bind_users(&result, &users, &count);
    test_set_calloc_failure(-1);
    test_set_malloc_failure(-1);
    mysql_free_result(result.res);
    ASSERT_EQ(cases[i].name, cases[i].success, success);
    ASSERT_EQ(cases[i].name, cases[i].success ? cases[i].rows : 0, count);

    if (success)
    {
      for (size_t j = 0; j < count; j++)
      {
        ASSERT_STR_EQ(cases[i].name, cases[i].id, users[j].id);
        ASSERT_STR_EQ(cases[i].name, cases[i].username, users[j].username.ptr);
        ASSERT_STR_EQ(cases[i].name, cases[i].hash, users[j].password_hash.ptr);
      }
    }
    else
    {
      ASSERT_NULL(cases[i].name, users);
    }

    free_users(users, count);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_bind_auth_sessions(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *id;
    const char *user_id;
    const char *issued_at;
    const char *expires_at;
    size_t rows;
    unsigned fields;
    bool db_error;
    bool allocation_failure;
    bool success;
  } cases[] = {
      {.name = "empty", .fields = 4, .success = true},
      {.name = "owned rows", .id = SESSION_ID, .user_id = USER_ID, .issued_at = "0", .expires_at = "3600", .rows = 2, .fields = 4, .success = true},
      {.name = "NULL id", .user_id = USER_ID, .issued_at = "0", .expires_at = "3600", .rows = 1, .fields = 4},
      {.name = "invalid user id", .id = SESSION_ID, .user_id = "dummy", .issued_at = "0", .expires_at = "3600", .rows = 1, .fields = 4},
      {.name = "NULL time", .id = SESSION_ID, .user_id = USER_ID, .expires_at = "3600", .rows = 1, .fields = 4},
      {.name = "negative", .id = SESSION_ID, .user_id = USER_ID, .issued_at = "-1", .expires_at = "3600", .rows = 1, .fields = 4},
      {.name = "fraction", .id = SESSION_ID, .user_id = USER_ID, .issued_at = "0.0", .expires_at = "3600", .rows = 1, .fields = 4},
      {.name = "overflow", .id = SESSION_ID, .user_id = USER_ID, .issued_at = "0", .expires_at = "99999999999999999999999999999", .rows = 1, .fields = 4},
      {.name = "reversed", .id = SESSION_ID, .user_id = USER_ID, .issued_at = "3600", .expires_at = "0", .rows = 1, .fields = 4},
      {.name = "equal", .id = SESSION_ID, .user_id = USER_ID, .issued_at = "0", .expires_at = "0", .rows = 1, .fields = 4},
      {.name = "allocation", .id = SESSION_ID, .user_id = USER_ID, .issued_at = "0", .expires_at = "3600", .rows = 1, .fields = 4, .allocation_failure = true},
      {.name = "wrong fields", .fields = 3},
      {.name = "DB error", .fields = 4, .db_error = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    const char *values[2][6] = {
        {cases[i].id, cases[i].user_id, cases[i].issued_at, cases[i].expires_at},
        {cases[i].id, cases[i].user_id, cases[i].issued_at, cases[i].expires_at},
    };
    db_result_t result = {.success = !cases[i].db_error,
                          .res = test_mysql_result(values, cases[i].rows, cases[i].fields)};
    auth_session_t *sessions = NULL;
    size_t count = 99;
    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    bool success = bind_auth_sessions(&result, &sessions, &count);
    test_set_calloc_failure(-1);
    mysql_free_result(result.res);
    ASSERT_EQ(cases[i].name, cases[i].success, success);
    ASSERT_EQ(cases[i].name, cases[i].success ? cases[i].rows : 0, count);

    if (success)
    {
      for (size_t j = 0; j < count; j++)
      {
        ASSERT_STR_EQ(cases[i].name, SESSION_ID, sessions[j].id);
        ASSERT_STR_EQ(cases[i].name, USER_ID, sessions[j].user_id);
        ASSERT_EQ(cases[i].name, (int64_t)0, sessions[j].issued_at);
        ASSERT_EQ(cases[i].name, (int64_t)3600, sessions[j].expires_at);
      }
    }
    else
    {
      ASSERT_NULL(cases[i].name, sessions);
    }

    free_auth_sessions(sessions);
    CHECK_TEST(cases[i].name);
  }
}

void
test_app_auth_repository(test_ctx_t *ctx)
{
  test_bind_users(ctx);
  test_bind_auth_sessions(ctx);
}
