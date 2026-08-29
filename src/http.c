#include "http.h"
#include "http_p.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>

#include "strutil.h"

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
destroy_http_request(http_request_t *req)
{
  free(req->internal);
  free(req);
}

error
parse_http_request(connection_t *conn, http_request_t *out_request)
{
  memset(out_request, 0, sizeof(http_request_t));

  out_request->internal = malloc(sizeof(http_parser_internal_state));
  http_parser_internal_state *s = out_request->internal;

  memset(s, 0, sizeof(http_parser_internal_state));
  s->state = STATE_REQ_METHOD;

  while (1)
  {
    ssize_t bytes_read = recv(
        conn->fd,
        s->buf + s->buf_len,
        MAX_HEADER_BYTES - s->buf_len,
        0);

    if (bytes_read > 0)
    {
      s->buf_len += bytes_read;

      error err = parse_chunk(out_request, bytes_read);
      if (err.code == ERR_MORE_DATA_NEEDED)
      {
        continue;
      }
      else
      {
        return err;
      }
    }
    else if (bytes_read == 0)
    {
      // TODO: 4096超えたときの処理
      error e = {
          .code = ERR_CONNECTION_CLOSED,
          .msg = "connection closed by client"};
      return e;
    }
    else
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
      {
        perror("recv");
        error e = {
            .code = ERR_CONNECTION_CLOSED,
            .msg = "recv failed"};
        return e;
      }
    }
  }
}

error
parse_chunk(http_request_t *req, size_t read_bytes)
{
  http_parser_internal_state *s = req->internal;

  size_t start_idx = s->buf_len - read_bytes;
  size_t end_idx = s->buf_len;

  if (s->buf_len >= MAX_HEADER_BYTES)
  {
    s->state = STATE_ERROR;
    error e = {
        .code = ERR_HTTP_PARSE_FAILED,
        .msg = "header too large"};
    return e;
  }

  for (size_t i = start_idx; i < end_idx; i++)
  {
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
        error e = {
            .code = ERR_MORE_DATA_NEEDED,
            .msg = "more data needed"};
        return e;
      }

      if (parse_method(req, s->method) < 0)
      {
        s->state = STATE_ERROR;
        error e = {
            .code = ERR_HTTP_PARSE_FAILED,
            .msg = "invalid request line: unknown method"};
        return e;
      }

      // consider i++
      i = method_idx + s->method_len;
      s->state = STATE_REQ_URI;
      break;
    }

    case STATE_REQ_URI:
      if (!req->uri)
      {
        req->uri = cur;
      }

      if (*cur == ' ')
      {
        req->uri_len = cur - req->uri;
        s->state = STATE_REQ_VERSION;

        if (req->uri_len == 0)
        {
          s->state = STATE_ERROR;
          error e = {
              .code = ERR_HTTP_PARSE_FAILED,
              .msg = "invalid request line: empty URI"};
          return e;
        }
      }
      else if (*cur == '\r' || *cur == '\n')
      {
        s->state = STATE_ERROR;
        error e = {
            .code = ERR_HTTP_PARSE_FAILED,
            .msg = "invalid request line: unexpected end of line while parsing URI"};
        return e;
      }
      break;

    case STATE_REQ_VERSION:
      if (!s->version)
      {
        s->version = cur;
      }

      if (*cur == '\r')
      {
        s->version_len = cur - s->version;
        s->state = STATE_REQ_LF;
        int result = parse_version(req, s->version, s->version_len);
        if (result < 0)
        {
          s->state = STATE_ERROR;
          error e = {
              .code = ERR_HTTP_PARSE_FAILED,
              .msg = "invalid request line: invalid HTTP version"};
          return e;
        }
      }
      else if (*cur == '\n')
      {
        s->version_len = cur - s->version;
        s->state = STATE_HEADER_KEY;
        int result = parse_version(req, s->version, s->version_len);
        if (result < 0)
        {
          s->state = STATE_ERROR;
          error e = {
              .code = ERR_HTTP_PARSE_FAILED,
              .msg = "invalid request line: invalid HTTP version"};
          return e;
        }
      }
      break;

    case STATE_REQ_LF:
      if (*cur == '\n')
      {
        s->state = STATE_HEADER_KEY;
      }
      else
      {
        s->state = STATE_ERROR;
        error e = {
            .code = ERR_HTTP_PARSE_FAILED,
            .msg = "invalid request line: expected LF"};
        return e;
      }
      break;

    case STATE_HEADER_KEY:
      if (*cur == '\r')
      {
        s->state = STATE_HEADER_END;
      }
      else if (*cur == '\n')
      {
        s->state = STATE_HEADER_END;
        return (error){.code = ERR_NONE};
      }
      else if (*cur == ':')
      {
        s->state = STATE_ERROR;
        error e = {
            .code = ERR_HTTP_PARSE_FAILED,
            .msg = "empty field name"};
        return e;
      }
      else
      {
        if (req->header_count >= MAX_HEADERS)
        {
          s->state = STATE_ERROR;
          error e = {
              .code = ERR_HTTP_PARSE_FAILED,
              .msg = "too many headers"};
          return e;
        }

        http_header_t *h = &req->headers[req->header_count];
        h->name = cur;
        s->state = STATE_HEADER_NAME;
      }
      break;

    case STATE_HEADER_NAME:
    {
      http_header_t *h = &req->headers[req->header_count];

      // white space in header name is not allowed
      if (is_whitespace(*cur))
      {
        s->state = STATE_ERROR;
        error e = {
            .code = ERR_HTTP_PARSE_FAILED,
            .msg = "whitespace is included in field-name"};
        return e;
      }
      else if (*cur == '\r')
      {
        s->state = STATE_ERROR;
        error e = {
            .code = ERR_HTTP_PARSE_FAILED,
            .msg = "unexpected CR in header"};
        return e;
      }
      else if (*cur == '\n')
      {
        s->state = STATE_ERROR;
        error e = {
            .code = ERR_HTTP_PARSE_FAILED,
            .msg = "unexpected LF in header"};
        return e;
      }
      else if (*cur == ':')
      {
        h->name_len = cur - h->name;
        s->state = STATE_HEADER_VALUE;
      }

      break;
    }

    case STATE_HEADER_VALUE:
    {
      http_header_t *h = &req->headers[req->header_count];

      if (!h->value && !is_whitespace(*cur))
      {
        h->value = cur;
        h->value_len = 0;
      }

      if (*cur == '\r')
      {
        s->state = STATE_HEADER_LF;
      }
      else if (*cur == '\n')
      {
        req->header_count++;
        s->state = STATE_HEADER_KEY;
      }

      if (!is_whitespace(*cur))
      {
        h->value_len = cur - h->value + 1;
      }

      break;
    }

    case STATE_HEADER_LF:
      if (*cur == '\n')
      {
        req->header_count++;
        s->state = STATE_HEADER_KEY;
      }
      else
      {
        s->state = STATE_ERROR;
        error e = {
            .code = ERR_HTTP_PARSE_FAILED,
            .msg = "unexpected lf"};
        return e;
      }

      break;

    case STATE_HEADER_END:
      if (*cur == '\n')
      {
        return (error){.code = ERR_NONE};
      }
      break;

    case STATE_ERROR:
      return (error){
          .code = ERR_HTTP_PARSE_FAILED,
          .msg = "parse error"};
    }
  }

  error e = {
      .code = ERR_MORE_DATA_NEEDED,
      .msg = "more data needed"};
  return e;
}

int
parse_method(http_request_t *req, const char *cur)
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

int
parse_version(http_request_t *req, const char *cur, size_t len)
{
  if (len != 8)
  {
    return -1;
  }

  if (cur[0] != 'H' || cur[1] != 'T' || cur[2] != 'T' || cur[3] != 'P' || cur[4] != '/' || cur[6] != '.')
  {
    return -1;
  }

  if (cur[5] < '0' || cur[5] > '9')
  {
    return -1;
  }
  if (cur[7] < '0' || cur[7] > '9')
  {
    return -1;
  }

  int major = cur[5] - '0';
  int minor = cur[7] - '0';

  if (major == 1)
  {
    if (minor == 0)
    {
      req->version = HTTP_VERSION_1_0;
      return 0;
    }
    else if (minor == 1)
    {
      req->version = HTTP_VERSION_1_1;
      return 0;
    }
  }

  return -1;
}