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

void handle_hello_async(const http_request_t *req)
{

}
