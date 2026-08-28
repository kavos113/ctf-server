#include "http.h"
#include "http_p.h"

#include <stdint.h>
#include <string.h>

#define METHOD_VALUE_GET     0x0000000020544547ULL
#define METHOD_VALUE_HEAD    0x0000002044414548ULL
#define METHOD_VALUE_OPTIONS 0x20534e4f4954504fULL
#define METHOD_VALUE_TRACE   0x0000204543415254ULL
#define METHOD_VALUE_PUT     0x0000000020545550ULL
#define METHOD_VALUE_DELETE  0x00204554454c4544ULL
#define METHOD_VALUE_POST    0x0000002054534f50ULL
#define METHOD_VALUE_PATCH   0x0000204843544150ULL
#define METHOD_VALUE_CONNECT 0x205443454e4e4f43ULL

error
parse_http_request(connection_t *conn, http_request *out_request)
{
}

int
parse_chunk(http_request *req, const char *buf, size_t len)
{
  struct http_parser_internal_state *s = req->internal;

  for (size_t i = 0; i < len;)
  {
    if (s->buf_len < MAX_HEADER_SIZE)
    {
      s->state = STATE_ERROR;
      return -1;
    }

    s->buf[s->buf_len] = buf[i];
    char *cur = &s->buf[s->buf_len];

    switch (s->state)
    {
    case STATE_REQ_METHOD:
      if (!s->method)
      {
        s->method = cur;
      }

      // not all arrived yet
      if ((cur - s->method) + len < 8)
      {
        i++;
        break;
      }

      if (read_method(req, s->method) < 0)
      {
        s->state = STATE_ERROR;
        return -1;
      }
      i = (s->method - buf) + s->method_len;
      break;

    default:
      return -1;
    }
  }

  return 0;
}

int
read_method(http_request *req, const char *cur)
{
  uint64_t v;
  memcpy(&v, cur, sizeof(uint64_t));

  struct http_parser_internal_state *s = req->internal;

  switch (v & 0x00000000ffffffffULL)
  {
  case METHOD_VALUE_GET:
    req->method = HTTP_METHOD_GET;
    s->method_len = 3;
    return 0;
  case METHOD_VALUE_PUT:
    req->method = HTTP_METHOD_PUT;
    s->method_len = 3;
    return 0;
  }

  switch (v & 0x000000ffffffffffULL)
  {
  case METHOD_VALUE_POST:
    req->method = HTTP_METHOD_POST;
    s->method_len = 4;
    return 0;
  case METHOD_VALUE_HEAD:
    req->method = HTTP_METHOD_HEAD;
    s->method_len = 4;
    return 0;
  }

  switch (v & 0x0000ffffffffffffULL)
  {
  case METHOD_VALUE_PATCH:
    req->method = HTTP_METHOD_PATCH;
    s->method_len = 5;
    return 0;
  case METHOD_VALUE_TRACE:
    req->method = HTTP_METHOD_TRACE;
    s->method_len = 5;
    return 0;
  }

  if ((v & 0x00ffffffffffffffULL) == METHOD_VALUE_DELETE)
  {
    req->method = HTTP_METHOD_DELETE;
    s->method_len = 6;
    return 0;
  }

  if (v == METHOD_VALUE_OPTIONS)
  {
    req->method = HTTP_METHOD_OPTIONS;
    s->method_len = 7;
    return 0;
  }

  if (v == METHOD_VALUE_CONNECT)
  {
    req->method = HTTP_METHOD_CONNECT;
    s->method_len = 7;
    return 0;
  }

  return -1;
}