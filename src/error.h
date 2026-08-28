#ifndef ERROR_H
#define ERROR_H

typedef enum error_code
{
  ERR_NONE,
  ERR_CONNECTION_CLOSED,
  ERR_HTTP_PARSE_FAILED,
} error_code;

typedef struct error
{
  error_code code;
  const char *msg;
} error;

#endif // ERROR_H