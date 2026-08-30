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
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/hello", handle_hello_async, handle_hello);

  serve(server);

  destroy_server(server);

  return 0;
}