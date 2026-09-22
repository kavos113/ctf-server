#define _POSIX_C_SOURCE 200809L

#include "repository.h"

#include <string.h>

#include <mysql/mysql.h>

static challenge_t *
bind_challenges_impl(const db_result_t *result, size_t *out_count, bool include_flag)
{
  MYSQL_RES *res = result->res;

  uint64_t rows = mysql_num_rows(res);
  if (rows == 0)
  {
    return NULL;
  }

  unsigned int num_fields = mysql_num_fields(res);
  if (num_fields != (include_flag ? 6U : 5U))
  {
    return NULL;
  }

  challenge_t *chals = calloc(rows, sizeof(challenge_t));
  if (!chals)
  {
    return NULL;
  }

  size_t chal_count = 0;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res)))
  {
    unsigned long *lengths = mysql_fetch_lengths(res);
    chals[chal_count].id = (int)strtol(row[0], NULL, 10);
    chals[chal_count].creator_id = string_from_cstr_dup_n(row[1], lengths[1]);
    chals[chal_count].name = string_from_cstr_dup_n(row[2], lengths[2]);
    chals[chal_count].description = string_from_cstr_dup_n(row[3], lengths[3]);
    if (include_flag)
    {
      chals[chal_count].flag = string_from_cstr_dup_n(row[4], lengths[4]);
    }
    chals[chal_count].genre = (ctf_genre)strtol(row[include_flag ? 5 : 4], NULL, 10);

    chals[chal_count].is_string_allocated = true;

    chal_count++;
  }

  if (out_count)
  {
    *out_count = chal_count;
  }
  return chals;
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

answer_t *
bind_answers(db_result_t *result, size_t *out_count)
{
  MYSQL_RES *res = result->res;

  uint64_t rows = mysql_num_rows(res);
  if (rows == 0)
  {
    return NULL;
  }

  answer_t *answers = malloc(sizeof(answer_t) * rows);
  size_t answer_count = 0;

  unsigned int num_fields = mysql_num_fields(res);
  if (num_fields != 6)
  {
    return NULL;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res)))
  {
    answers[answer_count].id = (int)strtol(row[0], NULL, 10);
    answers[answer_count].challenge_id = (int)strtol(row[1], NULL, 10);
    answers[answer_count].user_id = string_from_cstr_dup(row[2]);
    answers[answer_count].answer = string_from_cstr_dup(row[3]);
    answers[answer_count].is_corrected = (row[4][0] == '1');
    answers[answer_count].created_at = string_from_cstr_dup(row[5]);

    answers[answer_count].is_string_allocated = true;

    answer_count++;
  }

  if (out_count)
  {
    *out_count = answer_count;
  }
  return answers;
}
