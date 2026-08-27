#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#include "error.h"
#include "server.h"

#define DEFAULT_HEADER_SIZE 4096

typedef enum http_parse_state
{
  HTTP_PARSE_STATE_READING_HEADERS,
  HTTP_PARSE_STATE_READING_BODY,
  HTTP_PARSE_STATE_COMPLETE,
} http_parse_state;

typedef enum http_version
{
  HTTP_1_1,
  HTTP_2,
} http_version;

typedef struct http_request
{
  connection_t *conn;
  http_parse_state state;

  char header_buf[DEFAULT_HEADER_SIZE];

  http_version version;
  size_t content_length;
  size_t body_bytes_read;
  int tmp_file_fd;
} http_request;

error parse_http_request(connection_t *conn, http_request *out_request);

#endif // HTTP_H