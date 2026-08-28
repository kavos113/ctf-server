#ifndef SRC_HTTP_P_H
#define SRC_HTTP_P_H

#include <stddef.h>

#include "http.h"

#define MAX_HEADER_BYTES 4096

typedef enum
{
  STATE_REQ_METHOD,
  STATE_REQ_URI,
  STATE_REQ_VERSION,
  STATE_REQ_LF,
  STATE_HEADER_KEY,
  STATE_HEADER_VALUE,
  STATE_HEADER_LF,
  STATE_HEADER_END,
  STATE_ERROR,
} parse_state;

struct http_parser_internal_state
{
  char buf[MAX_HEADER_BYTES];
  size_t buf_len;

  parse_state state;

  const char *method;
  size_t method_len;
  const char *version;
  size_t version_len;
};

error parse_chunk(http_request *req, size_t read_bytes);

int parse_method(http_request *req, const char *cur);
int parse_version(http_request *req, const char *cur, size_t len);

#endif //SRC_HTTP_P_H
