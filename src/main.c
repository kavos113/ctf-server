#include "app/handler.h"
#include "http_server.h"
#include "server.h"

#define PORT            8080
#define MAX_CONNECTIONS 10

int
main()
{
  server_t *server = create_server(PORT, MAX_CONNECTIONS);
  if (!server)
  {
    return 1;
  }

  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/", NULL, handle_root);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/hello", NULL, handle_hello);

  // http_server_add_route(server->http_server, HTTP_METHOD_GET, "/challenges", handle_get_challenges);
  // http_server_add_route(server->http_server, HTTP_METHOD_POST, "/challenges", handle_post_challenge);
  // http_server_add_route(server->http_server, HTTP_METHOD_PUT, "/challenges", handle_put_challenge);
  // http_server_add_route(server->http_server, HTTP_METHOD_DELETE, "/challenges", handle_delete_challenge);
  // http_server_add_route(server->http_server, HTTP_METHOD_GET, "/challenges/me", handle_get_challenges_me);
  // http_server_add_route(server->http_server, HTTP_METHOD_GET, "/answers", handle_get_answers);
  // http_server_add_route(server->http_server, HTTP_METHOD_POST, "/answers", handle_post_answers);
  // http_server_add_route(server->http_server, HTTP_METHOD_GET, "/answers/me", handle_get_answers_me);

  serve(server);

  destroy_server(server);

  return 0;
}