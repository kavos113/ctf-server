#include "json_p.h"

static bool
hex_quad(const char **cur, const char *end, unsigned *value)
{
  if (end - *cur < 4)
  {
    return false;
  }

  *value = 0;

  for (size_t i = 0; i < 4; i++)
  {
    unsigned char c = *(*cur)++;
    unsigned digit;

    if (c >= '0' && c <= '9')
    {
      digit = c - '0';
    }
    else if (c >= 'a' && c <= 'f')
    {
      digit = c - 'a' + 10;
    }
    else if (c >= 'A' && c <= 'F')
    {
      digit = c - 'A' + 10;
    }
    else
    {
      return false;
    }

    *value = *value * 16 + digit;
  }

  return true;
}

static bool
read_codepoint(const char **cur, const char *end, unsigned *value)
{
  unsigned char c = *(*cur)++;

  if (c == '\\')
  {
    if (*cur == end)
    {
      return false;
    }

    c = *(*cur)++;

    switch (c)
    {
    case '"':
    case '\\':
    case '/':
      *value = c;
      return true;
    case 'b':
      *value = '\b';
      return true;
    case 'f':
      *value = '\f';
      return true;
    case 'n':
      *value = '\n';
      return true;
    case 'r':
      *value = '\r';
      return true;
    case 't':
      *value = '\t';
      return true;
    case 'u':
      if (!hex_quad(cur, end, value))
      {
        return false;
      }

      if (*value >= 0xd800 && *value <= 0xdbff)
      {
        if (end - *cur < 6 || (*cur)[0] != '\\' || (*cur)[1] != 'u')
        {
          return false;
        }

        *cur += 2;
        unsigned low;

        if (!hex_quad(cur, end, &low) || low < 0xdc00 || low > 0xdfff)
        {
          return false;
        }

        *value = 0x10000 + ((*value - 0xd800) << 10) + low - 0xdc00;
      }

      return *value < 0xdc00 || *value > 0xdfff;
    default:
      return false;
    }
  }

  if (c < 0x20 || c == '"')
  {
    return false;
  }

  if (c < 0x80)
  {
    *value = c;
    return true;
  }

  unsigned count;
  unsigned minimum;

  if (c >= 0xc2 && c <= 0xdf)
  {
    count = 1;
    minimum = 0x80;
    *value = c & 0x1f;
  }
  else if (c >= 0xe0 && c <= 0xef)
  {
    count = 2;
    minimum = 0x800;
    *value = c & 0x0f;
  }
  else if (c >= 0xf0 && c <= 0xf4)
  {
    count = 3;
    minimum = 0x10000;
    *value = c & 7;
  }
  else
  {
    return false;
  }

  if ((size_t)(end - *cur) < count)
  {
    return false;
  }

  for (unsigned i = 0; i < count; i++)
  {
    c = *(*cur)++;

    if ((c & 0xc0) != 0x80)
    {
      return false;
    }

    *value = (*value << 6) | (c & 0x3f);
  }

  return *value >= minimum && *value <= 0x10ffff && (*value < 0xd800 || *value > 0xdfff);
}

int
json_string_equal_decoded(string_t left, string_t right)
{
  if (!left.ptr || !right.ptr)
  {
    return -1;
  }

  const char *a = left.ptr;
  const char *b = right.ptr;
  const char *a_end = a + left.len;
  const char *b_end = b + right.len;
  bool equal = true;

  while (a < a_end || b < b_end)
  {
    bool has_a = a < a_end;
    bool has_b = b < b_end;
    unsigned x = 0;
    unsigned y = 0;

    if ((has_a && !read_codepoint(&a, a_end, &x)) || (has_b && !read_codepoint(&b, b_end, &y)))
    {
      return -1;
    }

    equal = equal && has_a == has_b && x == y;
  }

  return equal;
}
