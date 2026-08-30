#define _POSIX_C_SOURCE 200809L

#include "repository.h"

#include <string.h>

#include <mysql/mysql.h>

challenge_t * bind_challenges(const db_result_t *result)
{
  MYSQL_RES *res = result->res;

  uint64_t rows = mysql_num_rows(res);
  if (rows == 0)
  {
    return NULL;
  }

  challenge_t *chals = malloc(sizeof(challenge_t) * rows);
  size_t chal_count = 0;

  unsigned int num_fields = mysql_num_fields(res);
  if (num_fields != 6)
  {
    return NULL;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res)))
  {
    chals[chal_count].id = (int)strtol(row[0], NULL, 10); // NOT NULL
    chals[chal_count].creator_id = strdup(row[1]); // NOT NULL
    chals[chal_count].name = strdup(row[2]); // NOT NULL
    chals[chal_count].description = strdup(row[3]); // NOT NULL
    chals[chal_count].flag = strdup(row[4]); // NOT NULL
    chals[chal_count].genre = (ctf_genre) strtol(row[5], NULL, 10); // NOT NULL

    chal_count++;
  }

  return chals;
}

answer_t * bind_answers(db_result_t *result)
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
    answers[answer_count].user_id = strdup(row[2]);
    answers[answer_count].answer = strdup(row[3]);
    answers[answer_count].is_corrected = (row[4][0] == '1');
    answers[answer_count].created_at = strdup(row[5]);

    answer_count++;
  }

  return answers;
}