#include "model_auth.h"

#include <sodium.h>
#include <stdlib.h>

void
free_users(user_t *users, size_t count)
{
  if (!users)
  {
    return;
  }

  for (size_t i = 0; i < count; i++)
  {
    free(users[i].username.ptr);

    if (users[i].password_hash.ptr)
    {
      sodium_memzero(users[i].password_hash.ptr, users[i].password_hash.len);
      free(users[i].password_hash.ptr);
    }
  }

  free(users);
}

void
free_auth_sessions(auth_session_t *sessions)
{
  free(sessions);
}

void
free_public_users(public_user_t *users, size_t count)
{
  if (!users)
  {
    return;
  }

  for (size_t i = 0; i < count; i++)
  {
    free(users[i].username.ptr);
  }

  free(users);
}
