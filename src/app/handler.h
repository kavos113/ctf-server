#ifndef APP_HANDLER_H
#define APP_HANDLER_H

#include "http_request.h"
#include "http_response.h"

http_response_t handle_root(const http_request_t *req, void *data);

void handle_hello_async(const http_request_t *req);
http_response_t handle_hello(const http_request_t *req, void *data);


#endif // APP_HANDLER_H