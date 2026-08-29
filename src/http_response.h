#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <stddef.h>

#include "error.h"
#include "http.h"

typedef struct
{
  http_status status;
  const char *body;
  size_t body_len;
} http_response_t;

// とりあえずbodyを一括で返すことにする
// out_buf is expected not allocated (allocate inside the function)
error http_response_build(http_response_t *res, char **out_buf, size_t *out_buf_len);
error http_response_build_header(http_response_t *res, char **out_buf, size_t *out_buf_len);
void http_response_internal_server_error(char **out_buf, size_t *out_buf_len);

void http_response_send(http_response_t *res, int fd);

#endif // HTTP_RESPONSE_H