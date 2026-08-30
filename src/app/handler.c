#include "handler.h"

http_response_t
handle_root(const http_request_t *req, void *data)
{
  return (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = "Welcome to the CTF server!",
      .body_len = 26};
}

http_response_t
handle_hello(const http_request_t *req, void *data)
{
  return (http_response_t){
      .status = HTTP_STATUS_OK,
      .body = "Hello, World!",
      .body_len = 13};
}

http_response_t
handle_get_challenges(const http_request_t *req)
{
}

http_response_t
handle_post_challenge(const http_request_t *req)
{
}

http_response_t
handle_put_challenge(const http_request_t *req)
{
}

http_response_t
handle_delete_challenge(const http_request_t *req)
{
}

http_response_t
handle_get_challenges_me(const http_request_t *req)
{
}

http_response_t
handle_get_answers(const http_request_t *req)
{
}

http_response_t
handle_post_answers(const http_request_t *req)
{
}

http_response_t
handle_get_answers_me(const http_request_t *req)
{
}
