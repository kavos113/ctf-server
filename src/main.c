#include "app/auth.h"
#include "app/contest.h"
#include "app/handler.h"
#include "app/handler_auth.h"
#include "app/password_worker.h"

#include "http_server.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>

#define PORT            8080
#define MAX_CONNECTIONS 10

int
main()
{
  int64_t contest_start_at;

  if (!contest_start_parse(getenv("CONTEST_START_AT"), &contest_start_at))
  {
    fprintf(stderr, "Invalid CONTEST_START_AT; use an ISO 8601 timestamp with timezone.\n");
    return 1;
  }

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

  server->password_worker = password_worker_new(&auth, server->epoll_fd);

  if (!server->password_worker)
  {
    fprintf(stderr, "Password worker initialization failed.\n");
    destroy_server(server);
    auth_config_dispose(&auth);
    return 1;
  }

  auth_runtime_t runtime = {.config = &auth, .password_worker = server->password_worker,
                            .contest_start_at = contest_start_at};
  server->http_server->app_context = &runtime;

  http_handler_t get_users_2_handler = {handle_get_users_2, NULL};
  http_handler_t get_users_1_handler = {handle_get_users_1, &get_users_2_handler};

  http_handler_t root_handler = {handle_root, NULL};
  http_handler_t hello_2_handler = {handle_hello_2, NULL};
  http_handler_t hello_1_handler = {handle_hello_1, &hello_2_handler};
  http_handler_t get_challenges_2_handler = {handle_get_challenges_2, NULL};
  http_handler_t get_challenges_1_handler = {handle_get_challenges_1, &get_challenges_2_handler};

  http_handler_t get_own_challenges_2_handler = {handle_get_own_challenges_2, NULL};
  http_handler_t get_own_challenges_1_handler = {handle_get_own_challenges_1, &get_own_challenges_2_handler};
  http_handler_t get_own_challenges_auth_2 = {handle_auth_2, &get_own_challenges_1_handler};
  http_handler_t get_own_challenges_auth_1 = {handle_auth_1, &get_own_challenges_auth_2};

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

  http_handler_t post_answers_5_handler = {handle_post_answers_5, NULL};
  http_handler_t post_answers_4_handler = {handle_post_answers_4, &post_answers_5_handler};
  http_handler_t post_answers_3_handler = {handle_post_answers_3, &post_answers_4_handler};
  http_handler_t post_answers_2_handler = {handle_post_answers_2, &post_answers_3_handler};
  http_handler_t post_answers_1_handler = {handle_post_answers_1, &post_answers_2_handler};

  http_handler_t get_answers_3_handler = {handle_get_answers_3, NULL};
  http_handler_t get_answers_2_handler = {handle_get_answers_2, &get_answers_3_handler};
  http_handler_t get_answers_1_handler = {handle_get_answers_1, &get_answers_2_handler};

  http_handler_t get_own_answers_3_handler = {handle_get_own_answers_3, NULL};
  http_handler_t get_own_answers_2_handler = {handle_get_own_answers_2, &get_own_answers_3_handler};
  http_handler_t get_own_answers_1_handler = {handle_get_own_answers_1, &get_own_answers_2_handler};

  http_handler_t signup_5_handler = {handle_signup_5, NULL};
  http_handler_t signup_4_handler = {handle_signup_4, &signup_5_handler};
  http_handler_t signup_3_handler = {handle_signup_3, &signup_4_handler};
  http_handler_t signup_2_handler = {handle_signup_2, &signup_3_handler};
  http_handler_t signup_1_handler = {handle_signup_1, &signup_2_handler};

  http_handler_t login_5_handler = {handle_login_5, NULL};
  http_handler_t login_4_handler = {handle_login_4, &login_5_handler};
  http_handler_t login_3_handler = {handle_login_3, &login_4_handler};
  http_handler_t login_2_handler = {handle_login_2, &login_3_handler};
  http_handler_t login_1_handler = {handle_login_1, &login_2_handler};

  http_handler_t logout_2_handler = {handle_logout_2, NULL};
  http_handler_t logout_1_handler = {handle_logout_1, &logout_2_handler};

  http_handler_t post_challenges_auth_2 = {handle_auth_2, &post_challenges_1_handler};
  http_handler_t post_challenges_auth_1 = {handle_auth_1, &post_challenges_auth_2};
  http_handler_t put_challenges_auth_2 = {handle_auth_2, &put_challenges_1_handler};
  http_handler_t put_challenges_auth_1 = {handle_auth_1, &put_challenges_auth_2};
  http_handler_t delete_challenges_auth_2 = {handle_auth_2, &delete_challenges_1_handler};
  http_handler_t delete_challenges_auth_1 = {handle_auth_1, &delete_challenges_auth_2};
  http_handler_t post_answers_auth_2 = {handle_auth_2, &post_answers_1_handler};
  http_handler_t post_answers_auth_1 = {handle_auth_1, &post_answers_auth_2};
  http_handler_t get_own_answers_auth_2 = {handle_auth_2, &get_own_answers_1_handler};
  http_handler_t get_own_answers_auth_1 = {handle_auth_1, &get_own_answers_auth_2};
  http_handler_t logout_auth_2 = {handle_auth_2, &logout_1_handler};
  http_handler_t logout_auth_1 = {handle_auth_1, &logout_auth_2};

  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/", &root_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/hello", &hello_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/challenges", &get_challenges_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/challenges/me", &get_own_challenges_auth_1);
  http_server_add_route(server->http_server, HTTP_METHOD_POST, "/challenges", &post_challenges_auth_1);
  http_server_add_route(server->http_server, HTTP_METHOD_PUT, "/challenges", &put_challenges_auth_1);
  http_server_add_route(server->http_server, HTTP_METHOD_DELETE, "/challenges", &delete_challenges_auth_1);
  http_server_add_route(server->http_server, HTTP_METHOD_POST, "/answers", &post_answers_auth_1);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/answers", &get_answers_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/answers/me", &get_own_answers_auth_1);

  http_server_add_route(server->http_server, HTTP_METHOD_POST, "/signup", &signup_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_POST, "/login", &login_1_handler);
  http_server_add_route(server->http_server, HTTP_METHOD_POST, "/logout", &logout_auth_1);

  http_server_add_route(server->http_server, HTTP_METHOD_GET, "/users", &get_users_1_handler);

  serve(server);

  destroy_server(server);
  auth_config_dispose(&auth);

  return 0;
}
