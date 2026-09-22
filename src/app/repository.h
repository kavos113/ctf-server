#ifndef APP_REPOSITORY_H
#define APP_REPOSITORY_H

#include "db.h"

#include "model.h"
#include "model_auth.h"

// convert result of "SELECT * FROM challenges"
challenge_t *bind_challenges(const db_result_t *result, size_t *out_count);
// convert result of "SELECT id, creator_id, name, description, genre FROM challenges"
challenge_t *bind_challenges_without_flag(const db_result_t *result, size_t *out_count);

// Columns: id, challenge_id, user_id, answer, is_correct, formatted UTC time, joined username.
bool bind_answers(const db_result_t *result, answer_t **out_answers, size_t *out_count);

// SELECT id, username, password_hash FROM users
bool bind_users(const db_result_t *result, user_t **out_users, size_t *out_count);

// Columns: user id, username, nonnegative score. No authentication secrets.
bool bind_public_users(const db_result_t *result, public_user_t **out_users, size_t *out_count);

// Columns: session id, joined user id, issued_at and expires_at as UTC epoch seconds.
// Use TIMESTAMPDIFF(SECOND, '1970-01-01', column) on UTC DATETIME columns;
// unlike UNIX_TIMESTAMP(DATETIME), this does not depend on the session time zone.
// The SELECT must JOIN users; password hashes are not needed for authentication.
bool bind_auth_sessions(const db_result_t *result, auth_session_t **out_sessions, size_t *out_count);

#endif // APP_REPOSITORY_H
