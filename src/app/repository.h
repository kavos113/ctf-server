#ifndef APP_REPOSITORY_H
#define APP_REPOSITORY_H

#include "db.h"

#include "model.h"

// convert result of "SELECT * FROM challenges"
challenge_t *bind_challenges(const db_result_t *result, size_t *out_count);
// convert result of "SELECT id, creator_id, name, description, genre FROM challenges"
challenge_t *bind_challenges_without_flag(const db_result_t *result, size_t *out_count);

// Columns: id, challenge_id, user_id, answer, is_correct, formatted UTC time.
// Returns false on error; an empty result succeeds with NULL and count 0.
// Release the owned array and strings with free_answers.
bool bind_answers(const db_result_t *result, answer_t **out_answers, size_t *out_count);

#endif // APP_REPOSITORY_H
