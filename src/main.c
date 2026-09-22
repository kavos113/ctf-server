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

  http_handler_t root_handler = {handle_root, NULL};
  http_handler_t hello_2_handler = {handle_hello_2, NULL};
  http_handler_t hello_1_handler = {handle_hello_1, &hello_2_handler};
  http_handler_t get_challenges_2_handler = {handle_get_challenges_2, NULL};
  http_handler_t get_challenges_1_handler = {handle_get_challenges_1, &get_challenges_2_handler};

  http_handler_t post_challenges_2_handler = {
      .func = handle_post_challenges_2,
      .next = NULL,
  };
  http_handler_t post_challenges_1_handler = {
      .func = handle_post_challenges_1,
      .next = &post_challenges_2_handler,
  };

  http_server_add_route(server->http_server, HTTP_METHOD_POST, "/challenges", &post_challenges_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/", &root_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/hello", &hello_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/challenges", &get_challenges_1_handler);

  serve(server);

  destroy_server(server);

  return 0;
}