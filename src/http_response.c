#include "http_response.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>

static size_t
resolve_content_length(size_t body_len)
{
  int digits = 1;
  size_t tmp = body_len / 10;
  while (tmp > 0)
  {
    tmp /= 10;
    digits++;
  }
  return digits;
}

static const char *
resolve_content_type(const http_response_t *res)
{
  return res->content_type ? res->content_type : "text/plain";
}

error
http_response_build(http_response_t *res, char **out_buf, size_t *out_buf_len)
{
  const char *status_str = http_status_to_string(res->status);
  const char *version_str = http_version_to_string(HTTP_VERSION_1_1);
  const char *content_type = resolve_content_type(res);

  size_t response_len = strlen(version_str) + 5                 // space + status code + space
                        + strlen(status_str) + 2                // \r\n
                        + 16                                    // "Content-Length: "
                        + resolve_content_length(res->body_len) // Content-Length value
                        + 2                                     // \r\n
                        + 14 + strlen(content_type) + 2           // Content-Type header
                        + 19                                    // "Connection: close\r\n"
                        + 2                                     // \r\n
                        + res->body_len;

  char *buf = malloc(response_len + 1);
  if (!buf)
  {
    error e = {
        .code = ERR_OUT_OF_MEMORY,
        .msg = "failed to allocate memory for HTTP response"};
    return e;
  }

  size_t offset = snprintf(buf, response_len, "%s %d %s\r\n", version_str, res->status, status_str);
  offset += snprintf(buf + offset, response_len - offset, "Content-Length: %zu\r\n", res->body_len);
  offset += snprintf(buf + offset, response_len - offset, "Content-Type: %s\r\n", content_type);
  offset += snprintf(buf + offset, response_len - offset, "Connection: close\r\n");
  offset += snprintf(buf + offset, response_len - offset, "\r\n");
  memcpy(buf + offset, res->body, res->body_len);

  *out_buf = buf;
  *out_buf_len = response_len;

  error e = {
      .code = ERR_NONE,
      .msg = "HTTP response built successfully"};
  return e;
}

error
http_response_build_header(http_response_t *res, char **out_buf, size_t *out_buf_len)
{
  const char *status_str = http_status_to_string(res->status);
  const char *version_str = http_version_to_string(HTTP_VERSION_1_1);
  const char *content_type = resolve_content_type(res);

  size_t response_len = strlen(version_str) + 5                 // space + status code + space
                        + strlen(status_str) + 2                // \r\n
                        + 16                                    // "Content-Length: "
                        + resolve_content_length(res->body_len) // Content-Length value
                        + 2                                     // \r\n
                        + 14 + strlen(content_type) + 2           // Content-Type header
                        + 19                                    // "Connection: close\r\n"
                        + 2;                                    // \r\n

  char *buf = malloc(response_len + 1);
  if (!buf)
  {
    error e = {
        .code = ERR_OUT_OF_MEMORY,
        .msg = "failed to allocate memory for HTTP response header"};
    return e;
  }

  snprintf(buf, response_len + 1, // +1 for null terminator
           "%s %d %s\r\n"
           "Content-Length: %zu\r\n"
           "Content-Type: %s\r\n"
           "Connection: close\r\n"
           "\r\n",
           version_str, res->status, status_str, res->body_len, content_type);

  *out_buf = buf;
  *out_buf_len = response_len;

  error e = {
      .code = ERR_NONE,
      .msg = "HTTP response header built successfully"};
  return e;
}

void
http_response_internal_server_error(char **out_buf, size_t *out_buf_len)
{
  http_response_t res = {
      .status = HTTP_STATUS_INTERNAL_SERVER_ERROR,
      .body = "Internal Server Error",
      .body_len = strlen("Internal Server Error")};

  http_response_build(&res, out_buf, out_buf_len);
}
