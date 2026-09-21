#ifndef APP_STR_H
#define APP_STR_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
  char *ptr;
  size_t len;
} string_t;

bool string_equals(string_t a, string_t b);
bool string_equals_cstr(string_t a, const char *cstr);

string_t string_from_cstr(const char *cstr);
string_t string_from_cstr_n(const char *cstr, size_t n);
string_t string_from_cstr_dup(const char *cstr);
string_t string_from_cstr_dup_n(const char *cstr, size_t n);

#endif // APP_STR_H