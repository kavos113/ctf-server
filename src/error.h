#ifndef ERROR_H
#define ERROR_H

typedef enum error_code
{
  ERR_UNKNOWN,
} error_code;

typedef struct error
{
  error_code code;
  char *msg;
} error;

#endif // ERROR_H