#include "http.h"

const char *
http_method_to_string(http_method method)
{
  switch (method)
  {
  case HTTP_METHOD_GET:
    return "GET";
  case HTTP_METHOD_HEAD:
    return "HEAD";
  case HTTP_METHOD_OPTIONS:
    return "OPTIONS";
  case HTTP_METHOD_TRACE:
    return "TRACE";
  case HTTP_METHOD_PUT:
    return "PUT";
  case HTTP_METHOD_DELETE:
    return "DELETE";
  case HTTP_METHOD_POST:
    return "POST";
  case HTTP_METHOD_PATCH:
    return "PATCH";
  case HTTP_METHOD_CONNECT:
    return "CONNECT";
  }
  return "UNKNOWN";
}

const char *
http_version_to_string(http_version version)
{
  switch (version)
  {
  case HTTP_VERSION_1_0:
    return "HTTP/1.0";
  case HTTP_VERSION_1_1:
    return "HTTP/1.1";
  }
  return "UNKNOWN";
}