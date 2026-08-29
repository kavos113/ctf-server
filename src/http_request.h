#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <stddef.h>

#include "error.h"
#include "http.h"
#include "server.h"

// if request method is larger than this, discard
#define HTTP_METHOD_MAX_LEN 7
#define MAX_HEADERS         32

typedef struct
{
  const char *name;
  size_t name_len;
  const char *value;
  size_t value_len;
} http_header_t;

typedef struct
{
  enum
  {
    HTTP_PARAM_TYPE_QUERY,
    HTTP_PARAM_TYPE_FORM,
  } type;

  const char *name;
  size_t name_len;
  const char *value;
  size_t value_len;
} http_param_t;

struct http_parser_internal_state;

typedef struct http_request
{
  connection_t *conn;

  size_t body_bytes_read;
  int tmp_file_fd;

  http_version version;
  http_method method;
  const char *uri;
  size_t uri_len;

  http_header_t headers[MAX_HEADERS];
  size_t header_count;

  // commonly used header value
  size_t content_length;
  const char *content_type;
  const char *host;

  struct http_parser_internal_state *internal;
} http_request_t;

error parse_http_request(connection_t *conn, http_request_t *out_request);
void destroy_http_request(http_request_t *req);

/**
 * search request header by fied name (CASE-INSENSITIVE)
 * @param req
 * @param name header field name
 * @return NULL if not found
 */
const http_header_t *http_request_get_header(const http_request_t *req, const char *name);

#endif // HTTP_REQUEST_H