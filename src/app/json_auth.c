#include "json_auth.h"
#include "json_p.h"

#include <sodium.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  string_t username;
  string_t password;
  unsigned fields;
} auth_parser;

static bool
read_auth_field(json_parser_t *parser, string_t key, unsigned depth, void *context)
{
  auth_parser *parsed = context;
  const struct
  {
    const char *name;
    unsigned bit;
    string_t *value;
  } fields[] = {
      {.name = "username", .bit = 1, .value = &parsed->username},
      {.name = "password", .bit = 2, .value = &parsed->password},
  };

  for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
  {
    int equal = json_string_equal_decoded(key, string_from_cstr(fields[i].name));

    if (equal < 0)
    {
      return false;
    }

    if (equal)
    {
      if (parsed->fields & fields[i].bit)
      {
        return false;
      }

      parsed->fields |= fields[i].bit;
      return read_string(parser, fields[i].value);
    }
  }

  return false;
}

void
free_auth_request(auth_request_t *request)
{
  if (!request)
  {
    return;
  }

  free(request->username.ptr);

  if (request->password.ptr)
  {
    sodium_memzero(request->password.ptr, request->password.len);
    free(request->password.ptr);
  }

  *request = (auth_request_t){0};
}

int
json_to_auth_request(const char *json, size_t len, auth_request_t *request)
{
  if (!request)
  {
    return -1;
  }

  *request = (auth_request_t){0};

  if (!json || !len || len >= 65536)
  {
    return -1;
  }

  json_parser_t parser = {.cur = json, .end = json + len, .error = -1};
  auth_parser parsed = {0};

  if (!read_object(&parser, 0, read_auth_field, &parsed) || parsed.fields != 3 ||
      skip_whitespace(parser.cur, parser.end) != parser.end)
  {
    return parser.error;
  }

  int result = json_string_decode(parsed.username, &request->username);

  if (result == 0)
  {
    result = json_string_decode(parsed.password, &request->password);
  }

  if (result != 0)
  {
    free_auth_request(request);
    return result;
  }

  if (request->username.len < 3 || request->username.len > 32 ||
      request->password.len < 8 || request->password.len > 128 ||
      memchr(request->password.ptr, '\0', request->password.len))
  {
    goto invalid;
  }

  for (size_t i = 0; i < request->username.len; i++)
  {
    char ch = request->username.ptr[i];

    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
    {
      goto invalid;
    }
  }

  return 0;

invalid:
  free_auth_request(request);
  return -1;
}
