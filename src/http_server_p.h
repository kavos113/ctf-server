#ifndef HTTP_SERVER_P_H
#define HTTP_SERVER_P_H

#include <unistd.h>

#include "http_request.h"

int normalize_uri(http_request_t *request);

ssize_t url_decode(char *str, size_t len);
ssize_t normalize_path(char *path, size_t len);

#endif // HTTP_SERVER_P_H