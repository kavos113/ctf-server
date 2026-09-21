#include "str.h"

#include <stdlib.h>
#include <string.h>

bool
string_equals(string_t a, string_t b)
{
  if (a.len != b.len)
  {
    return false;
  }
  return (memcmp(a.ptr, b.ptr, a.len) == 0);
}

bool
string_equals_cstr(string_t a, const char *cstr)
{
  size_t cstr_len = strlen(cstr);
  if (a.len != cstr_len)
  {
    return false;
  }
  return (memcmp(a.ptr, cstr, a.len) == 0);
}

string_t
string_from_cstr(const char *cstr)
{
  return string_from_cstr_n(cstr, strlen(cstr));
}

string_t
string_from_cstr_n(const char *cstr, size_t n)
{
  return (string_t){(char *)cstr, n};
}

string_t
string_from_cstr_dup(const char *cstr)
{
  return string_from_cstr_dup_n(cstr, strlen(cstr));
}

string_t
string_from_cstr_dup_n(const char *cstr, size_t n)
{
  string_t s;
  s.ptr = malloc(n + 1);
  if (!s.ptr)
  {
    s.len = 0;
    return s;
  }
  memcpy(s.ptr, cstr, n);
  s.ptr[n] = '\0';
  s.len = n;
  return s;
}