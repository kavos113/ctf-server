#ifndef APP_REPOSITORY_H
#define APP_REPOSITORY_H

#include "db.h"

#include "model.h"

// convert result of "SELECT * FROM challenges"
challenge_t *bind_challenges(const db_result_t *result);

// convert result of "SELECT * FROM answers"
answer_t *bind_answers(db_result_t *result);

#endif // APP_REPOSITORY_H