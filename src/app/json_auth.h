#ifndef APP_JSON_AUTH_H
#define APP_JSON_AUTH_H

#include "str.h"

typedef struct
{
  string_t username;
  string_t password;
} auth_request_t;

int json_to_auth_request(const char *json, size_t len, auth_request_t *request);
void free_auth_request(auth_request_t *request);

#endif // APP_JSON_AUTH_H
