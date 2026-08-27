#include "http.h"
#include "http_p.h"

#include <string.h>

error
parse_http_request(connection_t *conn, http_request *out_request)
{
}

int
parse_chunk(http_request *request, const char *buf, size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
  }
}