#define _POSIX_C_SOURCE 200809L

#include "repository.h"
#include "json_p.h"

#include <stdint.h>

#include <string.h>

#include <mysql/mysql.h>

static challenge_t *
bind_challenges_impl(const db_result_t *result, size_t *out_count, bool include_flag)
{
  if (out_count)
  {
    *out_count = 0;
  }

  if (!result || !result->success || !result->res)
  {
    return NULL;
  }

  MYSQL_RES *res = result->res;
  uint64_t rows = mysql_num_rows(res);

  if (rows == 0 || rows > SIZE_MAX / sizeof(challenge_t))
  {
    return NULL;
  }

  unsigned int num_fields = mysql_num_fields(res);

  if (num_fields != (include_flag ? 6U : 5U))
  {
    return NULL;
  }

  challenge_t *chals = calloc((size_t)rows, sizeof(*chals));

  if (!chals)
  {
    return NULL;
  }

  for (size_t i = 0; i < rows; i++)
  {
    MYSQL_ROW row = mysql_fetch_row(res);
    unsigned long *lengths = row ? mysql_fetch_lengths(res) : NULL;

    if (!row || !lengths)
    {
      goto error;
    }

    for (size_t j = 0; j < num_fields; j++)
    {
      if (!row[j])
      {
        goto error;
      }
    }

    chals[i].is_string_allocated = true;
    chals[i].id = (int)strtol(row[0], NULL, 10);
    chals[i].creator_id = string_from_cstr_dup_n(row[1], lengths[1]);
    chals[i].name = string_from_cstr_dup_n(row[2], lengths[2]);
    chals[i].description = string_from_cstr_dup_n(row[3], lengths[3]);

    if (include_flag)
    {
      chals[i].flag = string_from_cstr_dup_n(row[4], lengths[4]);
    }

    chals[i].genre = (ctf_genre)strtol(row[include_flag ? 5 : 4], NULL, 10);

    if (!chals[i].creator_id.ptr || !chals[i].name.ptr || !chals[i].description.ptr ||
        (include_flag && !chals[i].flag.ptr))
    {
      goto error;
    }
  }

  if (out_count)
  {
    *out_count = (size_t)rows;
  }

  return chals;

error:
  free_challenges(chals, (size_t)rows);
  return NULL;
}

challenge_t *
bind_challenges(const db_result_t *result, size_t *out_count)
{
  return bind_challenges_impl(result, out_count, true);
}

challenge_t *
bind_challenges_without_flag(const db_result_t *result, size_t *out_count)
{
  return bind_challenges_impl(result, out_count, false);
}

static bool
bind_answer_id(const char *value, size_t len, int *id)
{
  if (!value)
  {
    return false;
  }

  json_parser_t parser = {.cur = value, .end = value + len, .error = -1};
  return read_positive_integer(&parser, id) && parser.cur == parser.end;
}

bool
bind_answers(const db_result_t *result, answer_t **out_answers, size_t *out_count)
{
  *out_answers = NULL;
  *out_count = 0;

  if (!result || !result->success || !result->res || mysql_num_fields(result->res) != 6)
  {
    return false;
  }

  uint64_t count = mysql_num_rows(result->res);

  if (count > SIZE_MAX / sizeof(answer_t))
  {
    return false;
  }

  if (!count)
  {
    return true;
  }

  answer_t *answers = calloc((size_t)count, sizeof(*answers));

  if (!answers)
  {
    return false;
  }

  for (size_t i = 0; i < count; i++)
  {
    MYSQL_ROW row = mysql_fetch_row(result->res);
    unsigned long *lengths = row ? mysql_fetch_lengths(result->res) : NULL;

    if (!row || !lengths)
    {
      goto error;
    }

    for (size_t j = 0; j < 6; j++)
    {
      if (!row[j])
      {
        goto error;
      }
    }

    if (!bind_answer_id(row[0], lengths[0], &answers[i].id) ||
        !bind_answer_id(row[1], lengths[1], &answers[i].challenge_id) ||
        lengths[4] != 1 ||
        (row[4][0] != '0' && row[4][0] != '1'))
    {
      goto error;
    }

    answers[i].is_string_allocated = true;
    answers[i].user_id = string_from_cstr_dup_n(row[2], lengths[2]);
    answers[i].answer = string_from_cstr_dup_n(row[3], lengths[3]);
    answers[i].created_at = string_from_cstr_dup_n(row[5], lengths[5]);
    answers[i].is_corrected = row[4][0] == '1';

    if (!answers[i].user_id.ptr || !answers[i].answer.ptr || !answers[i].created_at.ptr)
    {
      goto error;
    }
  }

  *out_answers = answers;
  *out_count = (size_t)count;
  return true;

error:
  free_answers(answers, (size_t)count);
  return false;
}
