#include "http.h"
#include "http_p.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>

#define METHOD_VALUE_GET     0x0000000020544547ULL
#define METHOD_VALUE_HEAD    0x0000002044414548ULL
#define METHOD_VALUE_OPTIONS 0x20534e4f4954504fULL
#define METHOD_VALUE_TRACE   0x0000204543415254ULL
#define METHOD_VALUE_PUT     0x0000000020545550ULL
#define METHOD_VALUE_DELETE  0x00204554454c4544ULL
#define METHOD_VALUE_POST    0x0000002054534f50ULL
#define METHOD_VALUE_PATCH   0x0000204843544150ULL
#define METHOD_VALUE_CONNECT 0x205443454e4e4f43ULL

typedef struct http_parser_internal_state http_parser_internal_state;

void
destroy_http_request(http_request *req)
{
  free(req->internal);
  free(req);
}

error
parse_http_request(connection_t *conn, http_request *out_request)
{
  memset(out_request, 0, sizeof(http_request));

  out_request->internal = malloc(sizeof(http_parser_internal_state));
  http_parser_internal_state *s = out_request->internal;

  memset(s, 0, sizeof(http_parser_internal_state));
  s->state = STATE_REQ_METHOD;

  while (1)
  {
    ssize_t bytes_read =  recv(
      conn->fd,
      s->buf + s->buf_len,
      MAX_HEADER_BYTES - s->buf_len,
      0
    );

    if (bytes_read > 0)
    {
      s->buf_len += bytes_read;

      int result = parse_chunk(out_request, bytes_read);
      if (result < 0)
      {
        error e = {
          .code = ERR_HTTP_PARSE_FAILED,
          .msg = "http parse failed"
        };
        return e;
      }
      if (result > 0)
      {
        continue;
      }
      else
      {
        error e = {
          .code = ERR_NONE
        };
        return e;
      }
    }
    else if (bytes_read == 0)
    {
      // TODO: 4096超えたときの処理
      error e = {
        .code = ERR_CONNECTION_CLOSED,
        .msg = "connection closed by client"
      };
      return e;
    }
    else
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
      {
        perror("recv");
        error e = {
          .code = ERR_CONNECTION_CLOSED,
          .msg = "recv failed"
        };
        return e;
      }
    }
  }
}

int
parse_chunk(http_request *req, size_t read_bytes)
{
  http_parser_internal_state *s = req->internal;

  size_t start_idx = s->buf_len - read_bytes;
  size_t end_idx = s->buf_len;

  for (size_t i = start_idx; i < end_idx;)
  {
    if (s->buf_len < MAX_HEADER_BYTES)
    {
      s->state = STATE_ERROR;
      return -1;
    }

    char *cur = &s->buf[i];

    switch (s->state)
    {
    case STATE_REQ_METHOD:
    {
      if (!s->method)
      {
        s->method = cur;
      }

      size_t method_idx = s->method - s->buf;

      // not all arrived yet
      if (end_idx - method_idx < 8)
      {
        return 1;
      }

      if (read_method(req, s->method) < 0)
      {
        s->state = STATE_ERROR;
        return -1;
      }

      i = method_idx + s->method_len + 1;
      s->state = STATE_REQ_URI;
      break;
    }

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

  http_parser_internal_state *s = req->internal;

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