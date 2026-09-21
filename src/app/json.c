#include "json.h"
#include "json_p.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t
int_string_length(int value)
{
  int length = 0;
  if (value == 0)
  {
    return 1;
  }
  if (value < 0)
  {
    length++; // for the negative sign
    value = -value;
  }
  while (value > 0)
  {
    length++;
    value /= 10;
  }
  return length;
}

void
challenge_to_json(const challenge_t *challenge, string_t *json_str, bool only_size)
{
  if (!challenge || !json_str)
  {
    return;
  }

  string_t genre_str = ctf_genre_to_string(challenge->genre);

  size_t buffer_suze = 1                                          // "{"
                       + 5 + int_string_length(challenge->id) + 1 // "id":<id>,
                       + 13 + challenge->creator_id.len + 3       // "creator_id":"<creator_id>",
                       + 7 + challenge->name.len + 3              // "name":"<name>",
                       + 14 + challenge->description.len + 3      // "description":"<description>",
                       + 7 + challenge->flag.len + 3              // "flag":"<flag>",
                       + 8 + genre_str.len + 4;                   // "genre":"<genre>"}\0

  printf("length: id: %zu, creator_id: %zu, name: %zu, description: %zu, flag: %zu, genre: %zu\n",
         5 + int_string_length(challenge->id) + 1,
         13 + challenge->creator_id.len + 3,
         7 + challenge->name.len + 3,
         14 + challenge->description.len + 3,
         7 + challenge->flag.len + 3,
         8 + genre_str.len + 4);

  if (only_size)
  {
    json_str->ptr = NULL;
    json_str->len = buffer_suze - 1; // Exclude null terminator
    return;
  }

  char *buffer = malloc(buffer_suze);
  if (!buffer)
  {
    return;
  }

  snprintf(buffer, buffer_suze,
           "{\"id\":%d,\"creator_id\":\"%.*s\",\"name\":\"%.*s\",\"description\":\"%.*s\",\"flag\":\"%.*s\",\"genre\":\"%.*s\"}",
           challenge->id,
           (int)challenge->creator_id.len, challenge->creator_id.ptr,
           (int)challenge->name.len, challenge->name.ptr,
           (int)challenge->description.len, challenge->description.ptr,
           (int)challenge->flag.len, challenge->flag.ptr,
           (int)genre_str.len, genre_str.ptr);

  json_str->ptr = buffer;
  json_str->len = buffer_suze - 1; // Exclude null terminator
}

void
challenges_to_json(const challenge_t *challenges, size_t count, string_t *json_str, bool only_size)
{
  if (!challenges || !json_str)
  {
    return;
  }

  if (only_size)
  {
    json_str->ptr = NULL;
    json_str->len = 0;
    return;
  }

  size_t total_size = 1; // [
  for (size_t i = 0; i < count; i++)
  {
    string_t challenge_json;
    challenge_to_json(&challenges[i], &challenge_json, true);
    total_size += challenge_json.len + 1; // , or ]
  }
  total_size += 1; // null terminator

  char *buffer = malloc(total_size);
  if (!buffer)
  {
    return;
  }

  snprintf(buffer, total_size, "[");
  size_t offset = 1;

  for (size_t i = 0; i < count; i++)
  {
    string_t challenge_json;
    challenge_to_json(&challenges[i], &challenge_json, false);
    offset += snprintf(buffer + offset, total_size - offset, "%s", challenge_json.ptr);
    if (i < count - 1)
    {
      buffer[offset++] = ',';
    }
  }

  buffer[offset] = ']';
  buffer[offset + 1] = '\0';

  json_str->ptr = buffer;
  json_str->len = total_size - 1; // Exclude null terminator
}

int
json_to_challenge(const char *json_str, size_t json_len, challenge_t *challenge)
{
  if (!json_str || !challenge)
  {
    return -1;
  }

  memset(challenge, 0, sizeof(challenge_t));

  const char *ptr = json_str;
  const char *end = json_str + json_len;

  ptr = skip_whitespace(ptr, end);
  if (ptr >= end || *ptr != '{')
  {
    return -1;
  }
  ptr++; // Skip '{'

  while (ptr < end)
  {
    ptr = skip_whitespace(ptr, end);
    if (ptr >= end)
    {
      return -1;
    }

    if (*ptr == '}')
    {
      ptr++; // Skip '}'
      return 0;
    }

    if (*ptr == ',')
    {
      ptr++; // Skip ','
      continue;
    }

    // parse key
    string_t key;
    ptr = parse_json_str(ptr, end, &key);
    if (!ptr)
    {
      return -1;
    }

    ptr = skip_whitespace(ptr, end);
    if (ptr >= end || *ptr != ':')
    {
      return -1;
    }
    ptr++; // Skip ':'
    ptr = skip_whitespace(ptr, end);

    // parse value
    if (string_equals_cstr(key, "id"))
    {
      int value;
      ptr = parse_json_int(ptr, end, &value);
      if (!ptr)
      {
        return -1;
      }
      challenge->id = value;
    }
    else if (string_equals_cstr(key, "creator_id"))
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->creator_id = value_str;
    }
    else if (string_equals_cstr(key, "name"))
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->name = value_str;
    }
    else if (string_equals_cstr(key, "description"))
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->description = value_str;
    }
    else if (string_equals_cstr(key, "flag"))
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->flag = value_str;
    }
    else if (string_equals_cstr(key, "genre"))
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->genre = ctf_genre_from_string(value_str);
    }
    else
    {
      // Skip unknown key-value pair
      ptr = skip_json_value(ptr, end);
      if (!ptr)
      {
        return -1;
      }
    }
  }

  // ここではrequestのポインタのみを使用しているため
  challenge->is_string_allocated = false;

  return 0;
}

void
json_to_challenges(const char *json_str, size_t json_len, challenge_t **challenges, size_t *count)
{
  if (!json_str || !challenges || !count)
  {
    return;
  }

  const char *ptr = json_str;
  const char *end = json_str + json_len;

  ptr = skip_whitespace(ptr, end);
  if (ptr >= end || *ptr != '[')
  {
    return;
  }
  ptr++; // Skip '['

  size_t capacity = 4; // default
  size_t num_challenges = 0;
  challenge_t *challenge_array = malloc(capacity * sizeof(challenge_t));
  if (!challenge_array)
  {
    return;
  }

  while (ptr < end)
  {
    ptr = skip_whitespace(ptr, end);
    if (ptr >= end)
    {
      break;
    }

    if (*ptr == ']')
    {
      ptr++; // Skip ']'
      break;
    }

    if (*ptr == ',')
    {
      ptr++; // Skip ','
      continue;
    }

    if (num_challenges >= capacity)
    {
      capacity *= 2;
      challenge_t *new_array = realloc(challenge_array, capacity * sizeof(challenge_t));
      if (!new_array)
      {
        free(challenge_array);
        return;
      }
      challenge_array = new_array;
    }

    challenge_t challenge;
    int result = json_to_challenge(ptr, end - ptr, &challenge);
    if (result != 0)
    {
      free(challenge_array);
      return;
    }

    challenge_array[num_challenges++] = challenge;

    // Move the pointer to the next value
    ptr = skip_json_value(ptr, end);
    if (!ptr)
    {
      free(challenge_array);
      return;
    }
  }

  *challenges = challenge_array;
  *count = num_challenges;
}

const char *
skip_whitespace(const char *str, const char *end)
{
  while (str < end && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r'))
  {
    str++;
  }
  return str;
}

const char *
parse_json_str(const char *str, const char *end, string_t *out_str)
{
  if (str >= end || *str != '"')
  {
    return NULL;
  }
  str++; // 開始"をskip
  if (out_str)
  {
    out_str->ptr = str;
    out_str->len = 0;
  }

  size_t idx = 0;
  while (str < end)
  {
    if (*str == '"')
    {
      if (out_str)
      {
        out_str->len = idx;
      }
      return str + 1;
    }

    str++;
    idx++;
  }

  return NULL;
}

const char *
parse_json_int(const char *str, const char *end, int *out_value)
{
  char *endptr;
  long value = strtol(str, &endptr, 10);
  if (endptr == str || endptr > end)
  {
    return NULL;
  }

  if (out_value)
  {
    *out_value = (int)value;
  }
  return endptr;
}

const char *
skip_json_value(const char *str, const char *end)
{
  str = skip_whitespace(str, end);
  if (str >= end)
  {
    return NULL;
  }

  if (*str == '"')
  {
    return parse_json_str(str, end, NULL);
  }

  if (*str == '{' || *str == '[')
  {
    char open = *str;
    char close = (open == '{') ? '}' : ']';
    int depth = 1;
    str++;

    while (str < end && depth > 0)
    {
      if (*str == open)
      {
        depth++;
      }
      else if (*str == close)
      {
        depth--;
      }
      str++;
    }
    return (depth == 0) ? str : NULL;
  }

  while (str < end && *str != ',' && *str != '}' && !isspace((unsigned char)*str))
  {
    str++;
  }

  return str;
}