#ifndef APP_STR_H
#define APP_STR_H

#include <stddef.h>

typedef struct
{
  char *ptr;
  size_t len;
} string_t;

#endif // APP_STR_H