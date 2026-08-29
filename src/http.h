#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#include "error.h"
#include "server.h"

typedef enum
{
  HTTP_VERSION_1_0,
  HTTP_VERSION_1_1,
} http_version;

typedef enum
{
  HTTP_METHOD_GET,
  HTTP_METHOD_HEAD,
  HTTP_METHOD_OPTIONS,
  HTTP_METHOD_TRACE,
  HTTP_METHOD_PUT,
  HTTP_METHOD_DELETE,
  HTTP_METHOD_POST,
  HTTP_METHOD_PATCH,
  HTTP_METHOD_CONNECT,
} http_method;

typedef enum
{
  HTTP_STATUS_CONTINUE = 100,
  HTTP_STATUS_SWITCHING_PROTOCOLS = 101,
  HTTP_STATUS_OK = 200,
  HTTP_STATUS_CREATED = 201,
  HTTP_STATUS_ACCEPTED = 202,
  HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
  HTTP_STATUS_NO_CONTENT = 204,
  HTTP_STATUS_RESET_CONTENT = 205,
  HTTP_STATUS_PARTIAL_CONTENT = 206,
  HTTP_STATUS_MULTIPLE_CHOICES = 300,
  HTTP_STATUS_MOVED_PERMANENTLY = 301,
  HTTP_STATUS_FOUND = 302,
  HTTP_STATUS_SEE_OTHER = 303,
  HTTP_STATUS_NOT_MODIFIED = 304,
  HTTP_STATUS_USE_PROXY = 305,
  HTTP_STATUS_TEMPORARY_REDIRECT = 307,
  HTTP_STATUS_PERMANENT_REDIRECT = 308,
  HTTP_STATUS_BAD_REQUEST = 400,
  HTTP_STATUS_UNAUTHORIZED = 401,
  HTTP_STATUS_PAYMENT_REQUIRED = 402,
  HTTP_STATUS_FORBIDDEN = 403,
  HTTP_STATUS_NOT_FOUND = 404,
  HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
  HTTP_STATUS_NOT_ACCEPTABLE = 406,
  HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
  HTTP_STATUS_REQUEST_TIMEOUT = 408,
  HTTP_STATUS_CONFLICT = 409,
  HTTP_STATUS_GONE = 410,
  HTTP_STATUS_LENGTH_REQUIRED = 411,
  HTTP_STATUS_PRECONDITION_FAILED = 412,
  HTTP_STATUS_CONTENT_TOO_LARGE = 413,
  HTTP_STATUS_URI_TOO_LONG = 414,
  HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE = 415,
  HTTP_STATUS_RANGE_NOT_SATISFIABLE = 416,
  HTTP_STATUS_EXPECTATION_FAILED = 417,
  HTTP_STATUS_MISDIRECTED_REQUEST = 421,
  HTTP_STATUS_UNPROCESSABLE_CONTENT = 422,
  HTTP_STATUS_TOO_EARLY = 425,
  HTTP_STATUS_UPGRADE_REQUIRED = 426,
  HTTP_STATUS_PRECONDITION_REQUIRED = 428,
  HTTP_STATUS_TOO_MANY_REQUESTS = 429,
  HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
  HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS = 451,
  HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
  HTTP_STATUS_NOT_IMPLEMENTED = 501,
  HTTP_STATUS_BAD_GATEWAY = 502,
  HTTP_STATUS_SERVICE_UNAVAILABLE = 503,
  HTTP_STATUS_GATEWAY_TIMEOUT = 504,
  HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED = 505,
} http_status;

const char *http_method_to_string(http_method method);
const char *http_version_to_string(http_version version);
const char *http_status_to_string(http_status status);

// if request method is larger than this, discard
#define HTTP_METHOD_MAX_LEN 7
#define MAX_HEADERS         32

typedef struct
{
  const char *name;
  size_t name_len;
  const char *value;
  size_t value_len;
} http_header_t;

struct http_parser_internal_state;

typedef struct http_request
{
  connection_t *conn;

  size_t body_bytes_read;
  int tmp_file_fd;

  http_version version;
  http_method method;
  const char *uri;
  size_t uri_len;

  http_header_t headers[MAX_HEADERS];
  size_t header_count;

  // commonly used header value
  size_t content_length;
  const char *content_type;
  const char *host;

  struct http_parser_internal_state *internal;
} http_request_t;

error parse_http_request(connection_t *conn, http_request_t *out_request);
void destroy_http_request(http_request_t *req);

/**
 * search request header by fied name (CASE-INSENSITIVE)
 * @param req
 * @param name header field name
 * @return NULL if not found
 */
const http_header_t *http_request_get_header(const http_request_t *req, const char *name);

#endif // HTTP_H