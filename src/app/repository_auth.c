#include "repository.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool
read_auth_id(const char *value, size_t length, char *out)
{
  if (!value || length != AUTH_ID_LENGTH)
  {
    return false;
  }

  for (size_t i = 0; i < length; i++)
  {
    if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f')))
    {
      return false;
    }
  }

  memcpy(out, value, length);
  out[length] = '\0';
  return true;
}

static bool
valid_username(const char *value, size_t length)
{
  if (!value || length < 3 || length > 32)
  {
    return false;
  }

  for (size_t i = 0; i < length; i++)
  {
    char ch = value[i];

    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
    {
      return false;
    }
  }

  return true;
}

static bool
read_auth_time(const char *value, size_t length, int64_t *out)
{
  // Upper bound: 9999-12-31 23:59:59 UTC, the DATETIME range.
  const int64_t maximum = INT64_C(253402300799);
  int64_t result = 0;

  if (!value || !length)
  {
    return false;
  }

  for (size_t i = 0; i < length; i++)
  {
    if (value[i] < '0' || value[i] > '9' || result > (maximum - (value[i] - '0')) / 10)
    {
      return false;
    }

    result = result * 10 + (value[i] - '0');
  }

  *out = result;
  return true;
}

bool
bind_users(const db_result_t *result, user_t **out_users, size_t *out_count)
{
  if (!out_users || !out_count)
  {
    return false;
  }

  *out_users = NULL;
  *out_count = 0;

  if (!result || !result->success || !result->res || mysql_num_fields(result->res) != 3)
  {
    return false;
  }

  uint64_t count = mysql_num_rows(result->res);

  if (count > SIZE_MAX / sizeof(user_t))
  {
    return false;
  }

  if (!count)
  {
    return true;
  }

  user_t *users = calloc((size_t)count, sizeof(*users));

  if (!users)
  {
    return false;
  }

  for (size_t i = 0; i < count; i++)
  {
    MYSQL_ROW row = mysql_fetch_row(result->res);
    unsigned long *lengths = row ? mysql_fetch_lengths(result->res) : NULL;

    if (!row || !lengths || !read_auth_id(row[0], lengths[0], users[i].id) ||
        !valid_username(row[1], lengths[1]) || !row[2] || !lengths[2] || lengths[2] > 255)
    {
      goto error;
    }

    for (size_t j = 0; j < lengths[2]; j++)
    {
      if (row[2][j] < '!' || row[2][j] > '~')
      {
        goto error;
      }
    }

    users[i].username = string_from_cstr_dup_n(row[1], lengths[1]);
    users[i].password_hash = string_from_cstr_dup_n(row[2], lengths[2]);

    if (!users[i].username.ptr || !users[i].password_hash.ptr)
    {
      goto error;
    }
  }

  *out_users = users;
  *out_count = (size_t)count;
  return true;

error:
  free_users(users, (size_t)count);
  return false;
}

bool
bind_auth_sessions(const db_result_t *result, auth_session_t **out_sessions, size_t *out_count)
{
  if (!out_sessions || !out_count)
  {
    return false;
  }

  *out_sessions = NULL;
  *out_count = 0;

  if (!result || !result->success || !result->res || mysql_num_fields(result->res) != 4)
  {
    return false;
  }

  uint64_t count = mysql_num_rows(result->res);

  if (count > SIZE_MAX / sizeof(auth_session_t))
  {
    return false;
  }

  if (!count)
  {
    return true;
  }

  auth_session_t *sessions = calloc((size_t)count, sizeof(*sessions));

  if (!sessions)
  {
    return false;
  }

  for (size_t i = 0; i < count; i++)
  {
    MYSQL_ROW row = mysql_fetch_row(result->res);
    unsigned long *lengths = row ? mysql_fetch_lengths(result->res) : NULL;

    if (!row || !lengths || !read_auth_id(row[0], lengths[0], sessions[i].id) ||
        !read_auth_id(row[1], lengths[1], sessions[i].user_id) ||
        !read_auth_time(row[2], lengths[2], &sessions[i].issued_at) ||
        !read_auth_time(row[3], lengths[3], &sessions[i].expires_at) ||
        sessions[i].expires_at <= sessions[i].issued_at)
    {
      free_auth_sessions(sessions);
      return false;
    }
  }

  *out_sessions = sessions;
  *out_count = (size_t)count;
  return true;
}

static bool
read_nonnegative_integer(const char *value, size_t length, int64_t *out)
{
  int64_t score = 0;

  if (!value || !length)
  {
    return false;
  }

  for (size_t i = 0; i < length; i++)
  {
    if (value[i] < '0' || value[i] > '9' || score > (INT64_MAX - (value[i] - '0')) / 10)
    {
      return false;
    }

    score = score * 10 + (value[i] - '0');
  }

  *out = score;
  return true;
}

bool
bind_public_users(const db_result_t *result, public_user_t **out_users, size_t *out_count)
{
  if (!out_users || !out_count)
  {
    return false;
  }

  *out_users = NULL;
  *out_count = 0;

  if (!result || !result->success || !result->res || mysql_num_fields(result->res) != 2)
  {
    return false;
  }

  uint64_t count = mysql_num_rows(result->res);

  if (count > SIZE_MAX / sizeof(public_user_t))
  {
    return false;
  }

  if (!count)
  {
    return true;
  }

  public_user_t *users = calloc((size_t)count, sizeof(*users));

  if (!users)
  {
    return false;
  }

  for (size_t i = 0; i < count; i++)
  {
    MYSQL_ROW row = mysql_fetch_row(result->res);
    unsigned long *lengths = row ? mysql_fetch_lengths(result->res) : NULL;

    if (!row || !lengths || !read_auth_id(row[0], lengths[0], users[i].id) ||
        !valid_username(row[1], lengths[1]))
    {
      goto error;
    }

    users[i].username = string_from_cstr_dup_n(row[1], lengths[1]);

    if (!users[i].username.ptr)
    {
      goto error;
    }
  }

  *out_users = users;
  *out_count = (size_t)count;
  return true;

error:
  free_public_users(users, (size_t)count);
  return false;
}

bool
bind_score_answers(const db_result_t *result, score_answer_t **out_answers, size_t *out_count)
{
  *out_answers = NULL;
  *out_count = 0;

  if (!result || !result->success || !result->res || mysql_num_fields(result->res) != 3)
  {
    return false;
  }

  uint64_t count = mysql_num_rows(result->res);

  if (count > SIZE_MAX / sizeof(score_answer_t))
  {
    return false;
  }

  if (!count)
  {
    return true;
  }

  score_answer_t *answers = calloc((size_t)count, sizeof(*answers));

  if (!answers)
  {
    return false;
  }

  for (size_t i = 0; i < count; i++)
  {
    MYSQL_ROW row = mysql_fetch_row(result->res);
    unsigned long *lengths = row ? mysql_fetch_lengths(result->res) : NULL;

    if (!row || !lengths || !read_auth_id(row[0], lengths[0], answers[i].user_id) ||
        !read_nonnegative_integer(row[1], lengths[1], &answers[i].challenge_id) ||
        answers[i].challenge_id < 1 || answers[i].challenge_id > INT_MAX ||
        !read_auth_time(row[2], lengths[2], &answers[i].created_at))
    {
      free(answers);
      return false;
    }
  }

  *out_answers = answers;
  *out_count = (size_t)count;
  return true;
}
