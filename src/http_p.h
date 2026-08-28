#ifndef SRC_HTTP_P_H
#define SRC_HTTP_P_H

#include <stddef.h>

#include "http.h"

#define MAX_HEADER_SIZE 4096

typedef enum
{
  STATE_REQ_METHOD,
  STATE_ERROR,
} parse_state;

struct http_parser_internal_state
{
  char buf[MAX_HEADER_SIZE];
  size_t buf_len;

  parse_state state;
  const char *method;
  size_t method_len;
};

/**
 * parse request body
 * @param req
 * @param buf
 * @param len
 * @return 1: continue, 0: parsed successfully, -1: parse error
 */
int parse_chunk(http_request *req, const char *buf, size_t len);

int read_method(http_request *req, const char *cur);

#endif //SRC_HTTP_P_H
