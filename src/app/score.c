#include "score.h"

#include <stdlib.h>
#include <string.h>

static int
compare_user_ids(const void *left, const void *right)
{
  const public_user_t *a = left;
  const public_user_t *b = right;
  return strcmp(a->id, b->id);
}

static int
compare_answers(const void *left, const void *right)
{
  const score_answer_t *a = left;
  const score_answer_t *b = right;
  int order = strcmp(a->user_id, b->user_id);

  if (order)
  {
    return order;
  }

  return (a->challenge_id > b->challenge_id) - (a->challenge_id < b->challenge_id);
}

static int
compare_scores(const void *left, const void *right)
{
  const public_user_t *a = left;
  const public_user_t *b = right;

  if (a->score != b->score)
  {
    return (a->score < b->score) - (a->score > b->score);
  }

  return strcmp(a->id, b->id);
}

void
calculate_user_scores(public_user_t *users, size_t user_count,
                      score_answer_t *answers, size_t answer_count,
                      bool end_enabled, int64_t end_at)
{
  if (user_count > 1)
  {
    qsort(users, user_count, sizeof(*users), compare_user_ids);
  }

  if (answer_count > 1)
  {
    qsort(answers, answer_count, sizeof(*answers), compare_answers);
  }

  size_t answer_index = 0;

  for (size_t i = 0; i < user_count; i++)
  {
    users[i].score = 0;
    int64_t last_challenge = 0;

    while (answer_index < answer_count)
    {
      const score_answer_t *answer = &answers[answer_index];
      int order = strcmp(answer->user_id, users[i].id);

      if (order > 0)
      {
        break;
      }

      answer_index++;

      if (order < 0 || (end_enabled && answer->created_at >= end_at))
      {
        continue;
      }

      if (answer->challenge_id != last_challenge)
      {
        users[i].score += 100;
        last_challenge = answer->challenge_id;
      }
    }
  }

  if (user_count > 1)
  {
    qsort(users, user_count, sizeof(*users), compare_scores);
  }
}
