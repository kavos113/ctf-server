#ifndef SRC_HTTP_P_H
#define SRC_HTTP_P_H

#include <stddef.h>

#include "http.h"

#define DEFAULT_HEADER_SIZE 4096

typedef enum
{
  STATE_REQ_METHOD,
} parse_state;

struct http_parser_internal_state
{
  char buf[DEFAULT_HEADER_SIZE];
  parse_state state;
};

int parse_chunk(http_request *request, const char *buf, size_t len);



#endif //SRC_HTTP_P_H
