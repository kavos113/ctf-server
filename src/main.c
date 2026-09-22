#include "app/handler.h"
#include "app/auth.h"

#include <stdio.h>
#include "http_server.h"
#include "server.h"

#define PORT            8080
#define MAX_CONNECTIONS 10

int
main()
{
  auth_config_t auth;

  if (auth_init_from_env(&auth) != AUTH_OK)
  {
    fprintf(stderr, "Authentication initialization failed; check JWT configuration.\n");
    return 1;
  }

  server_t *server = create_server(PORT, MAX_CONNECTIONS);
  if (!server)
  {
    auth_config_dispose(&auth);
    return 1;
  }

  http_handler_t root_handler = {handle_root, NULL};
  http_handler_t hello_2_handler = {handle_hello_2, NULL};
  http_handler_t hello_1_handler = {handle_hello_1, &hello_2_handler};
  http_handler_t get_challenges_2_handler = {handle_get_challenges_2, NULL};
  http_handler_t get_challenges_1_handler = {handle_get_challenges_1, &get_challenges_2_handler};

  http_handler_t post_challenges_2_handler = {handle_post_challenges_2, NULL};
  http_handler_t post_challenges_1_handler = {handle_post_challenges_1, &post_challenges_2_handler};

  http_handler_t put_challenges_4_handler = {handle_put_challenges_4, NULL};
  http_handler_t put_challenges_3_handler = {handle_put_challenges_3, &put_challenges_4_handler};
  http_handler_t put_challenges_2_handler = {handle_put_challenges_2, &put_challenges_3_handler};
  http_handler_t put_challenges_1_handler = {handle_put_challenges_1, &put_challenges_2_handler};

  http_handler_t delete_challenges_4_handler = {handle_delete_challenges_4, NULL};
  http_handler_t delete_challenges_3_handler = {handle_delete_challenges_3, &delete_challenges_4_handler};
  http_handler_t delete_challenges_2_handler = {handle_delete_challenges_2, &delete_challenges_3_handler};
  http_handler_t delete_challenges_1_handler = {handle_delete_challenges_1, &delete_challenges_2_handler};

  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/", &root_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/hello", &hello_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/challenges", &get_challenges_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_POST, "/challenges", &post_challenges_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_PUT, "/challenges", &put_challenges_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_DELETE, "/challenges", &delete_challenges_1_handler);

  http_handler_t post_answers_5_handler = {
      .func = handle_post_answers_5,
      .next = NULL,
  };
  http_handler_t post_answers_4_handler = {
      .func = handle_post_answers_4,
      .next = &post_answers_5_handler,
  };
  http_handler_t post_answers_3_handler = {
      .func = handle_post_answers_3,
      .next = &post_answers_4_handler,
  };
  http_handler_t post_answers_2_handler = {
      .func = handle_post_answers_2,
      .next = &post_answers_3_handler,
  };
  http_handler_t post_answers_1_handler = {
      .func = handle_post_answers_1,
      .next = &post_answers_2_handler,
  };
  http_server_add_route(server->http_server, HTTP_METHOD_POST, "/answers", &post_answers_1_handler);

  http_handler_t get_answers_3_handler = {
      .func = handle_get_answers_3,
      .next = NULL,
  };
  http_handler_t get_answers_2_handler = {
      .func = handle_get_answers_2,
      .next = &get_answers_3_handler,
  };
  http_handler_t get_answers_1_handler = {
      .func = handle_get_answers_1,
      .next = &get_answers_2_handler,
  };
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/answers", &get_answers_1_handler);

  http_handler_t get_own_answers_3_handler = {
      .func = handle_get_own_answers_3,
      .next = NULL,
  };
  http_handler_t get_own_answers_2_handler = {
      .func = handle_get_own_answers_2,
      .next = &get_own_answers_3_handler,
  };
  http_handler_t get_own_answers_1_handler = {
      .func = handle_get_own_answers_1,
      .next = &get_own_answers_2_handler,
  };
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/answers/me", &get_own_answers_1_handler);

  serve(server);

  destroy_server(server);
  auth_config_dispose(&auth);

  return 0;
}