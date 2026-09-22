#ifndef APP_MODEL_AUTH_H
#define APP_MODEL_AUTH_H

#include <stdint.h>

#include "str.h"

#define AUTH_ID_LENGTH 32

typedef struct
{
  char id[AUTH_ID_LENGTH + 1];
  string_t username;
  string_t password_hash;
} user_t;

typedef struct
{
  char id[AUTH_ID_LENGTH + 1];
  string_t username;
  int64_t score;
} public_user_t;

typedef struct
{
  char id[AUTH_ID_LENGTH + 1];
  char user_id[AUTH_ID_LENGTH + 1];
  int64_t issued_at; // UTC seconds since 1970-01-01.
  int64_t expires_at;
} auth_session_t;

// Arrays returned by the bind functions own all their strings.
void free_users(user_t *users, size_t count);
void free_public_users(public_user_t *users, size_t count);
void free_auth_sessions(auth_session_t *sessions);

#endif // APP_MODEL_AUTH_H
