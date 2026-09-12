#include "json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
parse_json_str(const char *str, const char *end, char *out_buf)
{
  if (str >= end || *str != '"')
  {
    return NULL;
  }
  str++; // 開始"

  size_t idx = 0;
  while (str < end)
  {
    if (*str == '"')
    {
      if (out_buf)
      {
        out_buf[idx] = '\0';
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

      if (out_buf)
      {
        out_buf[idx++] = c;
      }
    }
    else
    {
      if (out_buf)
      {
        out_buf[idx++] = *str;
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