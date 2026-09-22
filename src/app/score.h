#ifndef APP_SCORE_H
#define APP_SCORE_H

#include "model_auth.h"

#include <stdbool.h>

// Reset scores, count distinct eligible correct answers, then sort users by score/ID.
// Both arrays may be reordered. The answers must contain only correct submissions.
void calculate_user_scores(public_user_t *users, size_t user_count,
                           score_answer_t *answers, size_t answer_count,
                           bool end_enabled, int64_t end_at);

#endif // APP_SCORE_H
