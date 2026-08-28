#ifndef SRC_HTTP_P_H
#define SRC_HTTP_P_H

#include <stddef.h>

#include "http.h"

#define MAX_HEADER_BYTES 4096

typedef enum
{
  STATE_REQ_METHOD,
  STATE_REQ_URI,
  STATE_ERROR,
} parse_state;

struct http_parser_internal_state
{
  char buf[MAX_HEADER_BYTES];
  size_t buf_len;

  parse_state state;
  const char *method;
  size_t method_len;
};

/**
 * parse request body
 * @param req
 * @param buf
 * @param read_bytes
 * @return 1: continue, 0: parsed successfully, -1: parse error
 */
int parse_chunk(http_request *req, size_t read_bytes);

int read_method(http_request *req, const char *cur);

#endif //SRC_HTTP_P_H
