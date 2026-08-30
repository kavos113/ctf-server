#ifndef APP_HANDLER_H
#define APP_HANDLER_H

#include "http_request.h"
#include "http_response.h"

http_response_t handle_root(const http_request_t *req, void *data);
http_response_t handle_hello(const http_request_t *req, void *data);

http_response_t handle_get_challenges(const http_request_t *req);
http_response_t handle_post_challenge(const http_request_t *req);
http_response_t handle_put_challenge(const http_request_t *req);
http_response_t handle_delete_challenge(const http_request_t *req);
http_response_t handle_get_challenges_me(const http_request_t *req);
http_response_t handle_get_answers(const http_request_t *req);
http_response_t handle_post_answers(const http_request_t *req);
http_response_t handle_get_answers_me(const http_request_t *req);

#endif // APP_HANDLER_H