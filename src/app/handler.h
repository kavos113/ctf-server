#ifndef APP_HANDLER_H
#define APP_HANDLER_H

#include "http_server.h"

http_response_t handle_root(const http_request_t *req);
http_response_t handle_hello(const http_request_t *req);

#endif // APP_HANDLER_H