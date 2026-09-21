#include "json.h"
#include "json_p.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

  char key_buf[64];

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

    // parse key
    ptr = parse_json_str(ptr, end, key_buf);
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
    if (strcmp(key_buf, "id") == 0)
    {
      int value;
      ptr = parse_json_int(ptr, end, &value);
      if (!ptr)
      {
        return -1;
      }
      challenge->id = value;
    }
    else if (strcmp(key_buf, "creator_id") == 0)
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->creator_id = value_str;
    }
    else if (strcmp(key_buf, "name") == 0)
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->name = value_str;
    }
    else if (strcmp(key_buf, "description") == 0)
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->description = value_str;
    }
    else if (strcmp(key_buf, "flag") == 0)
    {
      string_t value_str;
      ptr = parse_json_str(ptr, end, &value_str);
      if (!ptr)
      {
        return -1;
      }
      challenge->flag = value_str;
    }
    else if (strcmp(key_buf, "genre") == 0)
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
  out_str->ptr = str;

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

    if (*str == '\\')
    {
      str++;
      if (str >= end)
      {
        return NULL;
      }

      char c = *str;
      switch (*str)
      {
      case '"':
        c = '"';
        break;
      case '\\':
        c = '\\';
        break;
      case '/':
        c = '/';
        break;
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      default:
        return NULL;
      }

      if (out_str)
      {
        out_str->ptr[idx++] = c;
      }
    }
    else
    {
      if (out_str)
      {
        out_str->ptr[idx++] = *str;
      }
    }

    str++;
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