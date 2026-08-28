#include "strutil.h"

int
is_whitespace(char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}