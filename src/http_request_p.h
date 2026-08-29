#ifndef HTTP_P_H
#define HTTP_P_H

#include <stddef.h>
#include <unistd.h>

#include "http_request.h"

#define MAX_HEADER_BYTES 4096

typedef enum
{
  STATE_REQ_METHOD,
  STATE_REQ_URI,
  STATE_REQ_VERSION,
  STATE_REQ_LF,
  STATE_HEADER_KEY,
  STATE_HEADER_NAME,
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

error parse_chunk(http_request_t *req, size_t read_bytes, http_response_t *out_response);

int parse_method(http_request_t *req, const char *cur);
int parse_version(http_request_t *req, const char *cur, size_t len);

// if field name is common such as "Content-Type", store to req
void parse_header(http_request_t *req, http_header_t *header);

int normalize_uri(http_request_t *request);
ssize_t url_decode(char *str, size_t len);
ssize_t normalize_path(char *path, size_t len);
void parse_query_params(http_request_t *req);

#endif // HTTP_P_H
