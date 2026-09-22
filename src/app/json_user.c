#include "json.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static int
format_public_user(char *buffer, size_t size, const public_user_t *user)
{
  // IDs and usernames are validated ASCII by bind_public_users.
  return snprintf(buffer, size, "{\"id\":\"%s\",\"username\":\"%.*s\",\"score\":%" PRId64 "}",
                  user->id, (int)user->username.len, user->username.ptr, user->score);
}

void
public_users_to_json(const public_user_t *users, size_t count, string_t *json)
{
  *json = (string_t){0};

  if (!users && count)
  {
    return;
  }

  size_t size = 3; // Brackets and terminator.

  for (size_t i = 0; i < count; i++)
  {
    int length = format_public_user(NULL, 0, &users[i]);

    if (length < 0 || (size_t)length + (i > 0) > SIZE_MAX - size)
    {
      return;
    }

    size += (size_t)length + (i > 0);
  }

  char *buffer = malloc(size);

  if (!buffer)
  {
    return;
  }

  size_t offset = 0;
  buffer[offset++] = '[';

  for (size_t i = 0; i < count; i++)
  {
    if (i)
    {
      buffer[offset++] = ',';
    }

    int length = format_public_user(buffer + offset, size - offset, &users[i]);

    if (length < 0 || (size_t)length >= size - offset)
    {
      free(buffer);
      return;
    }

    offset += (size_t)length;
  }

  buffer[offset++] = ']';
  buffer[offset] = '\0';
  *json = (string_t){.ptr = buffer, .len = offset};
}
