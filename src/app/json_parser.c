#include "json_p.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX_JSON_DEPTH 64

typedef struct json_key
{
  string_t slice;
  struct json_key *next;
} json_key;

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

  const char *start = ++str;

  while (str < end)
  {
    unsigned char c = *str++;

    if (c == '"')
    {
      if (out_str)
      {
        out_str->ptr = (char *)start;
        out_str->len = str - start - 1;
      }

      return str;
    }

    if (c < 0x20)
    {
      return NULL;
    }

    if (c == '\\')
    {
      if (str == end)
      {
        return NULL;
      }

      c = *str++;

      if (c == 'u')
      {
        if (end - str < 4)
        {
          return NULL;
        }

        for (size_t i = 0; i < 4; i++)
        {
          if (!isxdigit((unsigned char)str[i]))
          {
            return NULL;
          }
        }

        str += 4;
      }
      else if (c != '"' && c != '\\' && c != '/' && c != 'b' && c != 'f' && c != 'n' && c != 'r' &&
               c != 't')
      {
        return NULL;
      }

      continue;
    }

    if (c >= 0x80)
    {
      size_t continuation;
      unsigned int codepoint;
      unsigned int minimum;

      if (c >= 0xc2 && c <= 0xdf)
      {
        continuation = 1;
        codepoint = c & 0x1f;
        minimum = 0x80;
      }
      else if (c >= 0xe0 && c <= 0xef)
      {
        continuation = 2;
        codepoint = c & 0x0f;
        minimum = 0x800;
      }
      else if (c >= 0xf0 && c <= 0xf4)
      {
        continuation = 3;
        codepoint = c & 0x07;
        minimum = 0x10000;
      }
      else
      {
        return NULL;
      }

      if ((size_t)(end - str) < continuation)
      {
        return NULL;
      }

      for (size_t i = 0; i < continuation; i++)
      {
        unsigned char next = *str++;

        if ((next & 0xc0) != 0x80)
        {
          return NULL;
        }

        codepoint = (codepoint << 6) | (next & 0x3f);
      }

      if (codepoint < minimum || codepoint > 0x10ffff ||
          (codepoint >= 0xd800 && codepoint <= 0xdfff))
      {
        return NULL;
      }
    }
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

static bool
consume(json_parser_t *parser, char c)
{
  parser->cur = skip_whitespace(parser->cur, parser->end);

  if (parser->cur == parser->end || *parser->cur != c)
  {
    return false;
  }

  parser->cur++;
  return true;
}

bool
read_string(json_parser_t *parser, string_t *slice)
{
  const char *start = skip_whitespace(parser->cur, parser->end);
  const char *next = parse_json_str(start, parser->end, slice);

  if (!next)
  {
    return false;
  }

  parser->cur = next;
  return true;
}

bool
read_object(json_parser_t *parser, unsigned depth, json_field_reader read_field, void *context)
{
  if (depth > MAX_JSON_DEPTH || !consume(parser, '{'))
  {
    return false;
  }

  json_key *keys = NULL;
  bool valid = false;

  if (consume(parser, '}'))
  {
    return true;
  }

  do
  {
    string_t key;

    if (!read_string(parser, &key))
    {
      goto done;
    }

    for (json_key *old = keys; old; old = old->next)
    {
      if (string_equals(old->slice, key))
      {
        goto done;
      }
    }

    // Only bookkeeping is allocated; key and value bytes remain in the input.
    json_key *entry = calloc(1, sizeof(*entry));

    if (!entry)
    {
      parser->error = -2;
      goto done;
    }

    entry->slice = key;
    entry->next = keys;
    keys = entry;

    if (!consume(parser, ':'))
    {
      goto done;
    }

    if (read_field)
    {
      if (!read_field(parser, key, depth + 1, context))
      {
        goto done;
      }
    }
    else if (!read_value(parser, depth + 1))
    {
      goto done;
    }

    if (consume(parser, '}'))
    {
      valid = true;
      break;
    }
  } while (consume(parser, ','));

done:
  while (keys)
  {
    json_key *next = keys->next;
    free(keys);
    keys = next;
  }

  return valid;
}

bool
read_value(json_parser_t *parser, unsigned depth)
{
  parser->cur = skip_whitespace(parser->cur, parser->end);

  if (depth > MAX_JSON_DEPTH || parser->cur == parser->end)
  {
    return false;
  }

  if (*parser->cur == '"')
  {
    return read_string(parser, NULL);
  }

  if (*parser->cur == '{')
  {
    return read_object(parser, depth, NULL, NULL);
  }

  if (*parser->cur == '[')
  {
    parser->cur++;

    if (consume(parser, ']'))
    {
      return true;
    }

    do
    {
      if (!read_value(parser, depth + 1))
      {
        return false;
      }

      if (consume(parser, ']'))
      {
        return true;
      }
    } while (consume(parser, ','));

    return false;
  }

  const char *literals[] = {"true", "false", "null"};

  for (size_t i = 0; i < sizeof(literals) / sizeof(literals[0]); i++)
  {
    size_t len = strlen(literals[i]);

    if ((size_t)(parser->end - parser->cur) >= len && memcmp(parser->cur, literals[i], len) == 0)
    {
      parser->cur += len;
      return true;
    }
  }

  if (*parser->cur == '-')
  {
    parser->cur++;
  }

  if (parser->cur == parser->end)
  {
    return false;
  }

  if (*parser->cur == '0')
  {
    parser->cur++;
  }
  else
  {
    if (*parser->cur < '1' || *parser->cur > '9')
    {
      return false;
    }

    do
    {
      parser->cur++;
    } while (parser->cur < parser->end && *parser->cur >= '0' && *parser->cur <= '9');
  }

  if (parser->cur < parser->end && *parser->cur == '.')
  {
    parser->cur++;
    const char *start = parser->cur;

    while (parser->cur < parser->end && *parser->cur >= '0' && *parser->cur <= '9')
    {
      parser->cur++;
    }

    if (start == parser->cur)
    {
      return false;
    }
  }

  if (parser->cur < parser->end && (*parser->cur == 'e' || *parser->cur == 'E'))
  {
    parser->cur++;

    if (parser->cur < parser->end && (*parser->cur == '+' || *parser->cur == '-'))
    {
      parser->cur++;
    }

    const char *start = parser->cur;

    while (parser->cur < parser->end && *parser->cur >= '0' && *parser->cur <= '9')
    {
      parser->cur++;
    }

    if (start == parser->cur)
    {
      return false;
    }
  }

  return true;
}
