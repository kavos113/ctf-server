#include "json.h"

#include <stdlib.h>

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
      switch (*str)
      {
      case '"':
        out_buf[idx++] = '"';
        break;
      case '\\':
        out_buf[idx++] = '\\';
        break;
      case '/':
        out_buf[idx++] = '/';
        break;
      case 'b':
        out_buf[idx++] = '\b';
        break;
      case 'f':
        out_buf[idx++] = '\f';
        break;
      case 'n':
        out_buf[idx++] = '\n';
        break;
      case 'r':
        out_buf[idx++] = '\r';
        break;
      case 't':
        out_buf[idx++] = '\t';
        break;
      default:
        return NULL;
      }
    }
    else
    {
      out_buf[idx++] = *str;
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