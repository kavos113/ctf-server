#include "server.h"

#define PORT            8080
#define MAX_CONNECTIONS 10

int
main()
{
  server_t *server = create_server(PORT, MAX_CONNECTIONS);
  serve(server);

  destroy_server(server);

  return 0;
}