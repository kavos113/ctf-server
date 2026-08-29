#include "http.h"

int
is_error_status(http_status status)
{
  return status >= HTTP_STATUS_BAD_REQUEST;
}
