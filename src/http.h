#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#include "error.h"
#include "server.h"

typedef enum
{
  HTTP_1_1,
  HTTP_2,
} http_version;

typedef enum
{
  HTTP_METHOD_GET,
  HTTP_METHOD_HEAD,
  HTTP_METHOD_OPTIONS,
  HTTP_METHOD_TRACE,
  HTTP_METHOD_PUT,
  HTTP_METHOD_DELETE,
  HTTP_METHOD_POST,
  HTTP_METHOD_PATCH,
  HTTP_METHOD_CONNECT,
} http_method;

// if request method is larger than this, discard
#define HTTP_METHOD_MAX_LEN 7

struct http_parser_internal_state;

typedef struct http_request
{
  connection_t *conn;

  size_t content_length;
  size_t body_bytes_read;
  int tmp_file_fd;

  http_version version;
  http_method method;
  const char *url;
  size_t url_len;

  struct http_parser_internal_state *internal;
} http_request;

error parse_http_request(connection_t *conn, http_request *out_request);
void destroy_http_request(http_request *req);

#endif // HTTP_H