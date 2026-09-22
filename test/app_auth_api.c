#include "mysql_result_mock.h"
#include "test.h"
#include "util.h"

#include <app/handler.h>
#include <app/handler_auth.h>
#include <app/json_p.h>
#include <jansson.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define USER_ID     "0123456789abcdef0123456789abcdef"
#define SESSION_ID  "fedcba9876543210fedcba9876543210"
#define OTHER_ID    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define CREDENTIALS "{\"username\":\"Alice\",\"password\":\"p\\u0061ssword\"}"

static bool intercept_worker;
static bool pending_worker;
static bool submitted_unknown;
static bool submitted_password_valid;
static password_submit_result submission;
password_submit_result __real_password_worker_submit(password_worker_t *, password_operation, string_t *, const char *, void *);

password_submit_result
__wrap_password_worker_submit(password_worker_t *worker, password_operation operation,
                              string_t *password, const char *hash, void *data)
{
  submitted_unknown = hash == NULL;
  submitted_password_valid = password->len == 8 && memcmp(password->ptr, "password", 8) == 0;

  if (!intercept_worker)
  {
    return __real_password_worker_submit(worker, operation, password, hash, data);
  }

  sodium_memzero(password->ptr, password->len);
  free(password->ptr);
  *password = (string_t){0};
  pending_worker = submission == PASSWORD_QUEUED;
  return submission;
}

static int64_t
fixed_now(void *data)
{
  return *(int64_t *)data;
}

static int
fixed_id(void *data, unsigned char *out, size_t len)
{
  if (data)
  {
    return -1;
  }

  memset(out, 0xab, len);
  return 0;
}

static void
make_chain(http_handler_t *nodes, const http_handler_func_t *functions, size_t count)
{
  for (size_t i = 0; i < count; i++)
  {
    nodes[i] = (http_handler_t){.func = functions[i], .next = i + 1 < count ? &nodes[i + 1] : NULL};
  }
}

static const http_handler_func_t signup_functions[] = {
    handle_signup_1, handle_signup_2, handle_signup_3, handle_signup_4, handle_signup_5};
static const http_handler_func_t login_functions[] = {
    handle_login_1, handle_login_2, handle_login_3, handle_login_4, handle_login_5};

static MYSQL_RES *
users_result(const char *hash, bool present)
{
  const char *rows[][6] = {{USER_ID, "Alice", hash}};
  return test_mysql_result(rows, present ? 1 : 0, 3);
}

static password_job_t *
wait_password(password_worker_t *worker)
{
  for (int i = 0; i < 32; i++)
  {
    password_job_t *job = password_worker_pop(worker);

    if (job)
    {
      return job;
    }

    struct pollfd fd = {.fd = password_worker_notify_fd(worker), .events = POLLIN};

    if (poll(&fd, 1, 3000) <= 0)
    {
      return NULL;
    }

    eventfd_t count;
    eventfd_read(fd.fd, &count);
  }

  return NULL;
}

static void
test_signup_login(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool login;
    const char *body;
    bool existing;
    bool real_worker;
    bool insert_failure;
    bool race_duplicate;
    bool select_failure;
    bool cleanup_failure;
    bool random_failure;
    bool allocation_failure;
    bool enqueue_failure;
    auth_result calculation;
    password_submit_result submit;
    http_status expected;
  } cases[] = {
      {.name = "signup real worker", .body = CREDENTIALS, .real_worker = true, .expected = HTTP_STATUS_CREATED},
      {.name = "login real worker", .login = true, .body = CREDENTIALS, .existing = true, .real_worker = true, .expected = HTTP_STATUS_OK},
      {.name = "invalid signup", .body = "{}", .expected = HTTP_STATUS_BAD_REQUEST},
      {.name = "invalid login", .login = true, .body = "{}", .expected = HTTP_STATUS_BAD_REQUEST},
      {.name = "signup duplicate", .body = CREDENTIALS, .existing = true, .expected = HTTP_STATUS_CONFLICT},
      {.name = "signup concurrent duplicate", .body = CREDENTIALS, .insert_failure = true, .race_duplicate = true, .expected = HTTP_STATUS_CONFLICT},
      {.name = "signup insert error", .body = CREDENTIALS, .insert_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "signup select error", .body = CREDENTIALS, .select_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "login select error", .login = true, .body = CREDENTIALS, .select_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "unknown user", .login = true, .body = CREDENTIALS, .calculation = AUTH_INVALID, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "unknown cannot succeed", .login = true, .body = CREDENTIALS, .calculation = AUTH_OK, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "wrong password", .login = true, .body = CREDENTIALS, .existing = true, .calculation = AUTH_INVALID, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "login verify error", .login = true, .body = CREDENTIALS, .existing = true, .calculation = AUTH_ERROR, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "signup hash error", .body = CREDENTIALS, .calculation = AUTH_ERROR, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "cleanup error", .login = true, .body = CREDENTIALS, .existing = true, .cleanup_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "session save error", .login = true, .body = CREDENTIALS, .existing = true, .insert_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "signup ID error", .body = CREDENTIALS, .random_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "session ID error", .login = true, .body = CREDENTIALS, .existing = true, .random_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "signup queue full", .body = CREDENTIALS, .submit = PASSWORD_QUEUE_FULL, .expected = HTTP_STATUS_SERVICE_UNAVAILABLE},
      {.name = "login queue full", .login = true, .body = CREDENTIALS, .existing = true, .submit = PASSWORD_QUEUE_FULL, .expected = HTTP_STATUS_SERVICE_UNAVAILABLE},
      {.name = "worker stopped", .body = CREDENTIALS, .submit = PASSWORD_WORKER_STOPPED, .expected = HTTP_STATUS_SERVICE_UNAVAILABLE},
      {.name = "worker allocation", .body = CREDENTIALS, .submit = PASSWORD_SUBMIT_ERROR, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "state allocation", .body = CREDENTIALS, .allocation_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "DB enqueue", .body = CREDENTIALS, .enqueue_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  auth_config_t config;
  auth_config_load(&config, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "ctf", "client", NULL);
  ASSERT_EQ("dummy initialization", AUTH_OK, auth_password_hash(string_from_cstr("password"), config.dummy_hash));
  int epoll_fd = epoll_create1(0);
  password_worker_t *worker = password_worker_new(&config, epoll_fd);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    intercept_worker = !cases[i].real_worker;
    pending_worker = false;
    submitted_password_valid = false;
    submission = cases[i].submit;
    int64_t now = 1000;
    auth_runtime_t runtime = {.config = &config, .password_worker = worker, .now = fixed_now, .clock_data = &now, .random = fixed_id, .random_data = cases[i].random_failure ? ctx : NULL};
    http_request_t *request = calloc(1, sizeof(*request));
    request->body = cases[i].body;
    request->content_length = strlen(cases[i].body);
    http_handler_t nodes[5];
    make_chain(nodes, cases[i].login ? login_functions : signup_functions, 5);
    http_request_context_t context = {.request = request, .app_context = &runtime, .current_handler = nodes};
    db_pool_t db = {.task_queue = task_queue_new()};
    db.task_queue->stop = cases[i].enqueue_failure;
    http_response_t response;
    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    bool complete = nodes[0].func(&context, &db, NULL, &response);
    test_set_calloc_failure(-1);
    unsigned steps = 0;
    unsigned selects = 0;
    bool inserted = false;

    while (!complete && steps++ < 8)
    {
      db_task_t *task = NULL;
      password_job_t *job = NULL;
      password_result_t computed = {.status = cases[i].calculation};
      snprintf(computed.hash, sizeof(computed.hash), "%s", config.dummy_hash);

      if (context.current_handler == &nodes[2])
      {
        if (cases[i].real_worker)
        {
          job = wait_password(worker);
          ASSERT_NOT_NULL(cases[i].name, job);

          if (!job)
          {
            break;
          }

          context.worker_result = &job->result;
        }
        else
        {
          ASSERT_TRUE(cases[i].name, pending_worker);
          pending_worker = false;
          context.worker_result = &computed;
        }

        ASSERT_TRUE(cases[i].name, submitted_password_valid);

        if (cases[i].login)
        {
          ASSERT_EQ(cases[i].name, !cases[i].existing, submitted_unknown);
        }
      }
      else
      {
        ASSERT_NOT_NULL(cases[i].name, db.task_queue->head);

        if (!db.task_queue->head)
        {
          break;
        }

        task = task_queue_pop(db.task_queue);
        task->result->success = 1;
        task->result->affected = 1;

        if (!task->is_param_query)
        {
          selects++;
          task->result->success = !cases[i].select_failure;
          task->result->res = users_result(config.dummy_hash, selects == 1 ? cases[i].existing : cases[i].race_duplicate);
        }
        else if (strncmp(task->query, "DELETE", 6) == 0)
        {
          ASSERT_TRUE(cases[i].name, cases[i].login);
          task->result->success = !cases[i].cleanup_failure;
        }
        else
        {
          inserted = true;
          task->result->success = !cases[i].insert_failure;
          ASSERT_EQ(cases[i].name, (size_t)(cases[i].login ? 4 : 3), task->param_count);
          ASSERT_EQ(cases[i].name, (size_t)AUTH_ID_LENGTH, task->params[0].value.string.len);

          if (cases[i].login)
          {
            ASSERT_STR_EQ(cases[i].name, USER_ID, task->params[1].value.string.ptr);
            ASSERT_EQ(cases[i].name, now, task->params[2].value.integer);
            ASSERT_EQ(cases[i].name, now + config.ttl, task->params[3].value.integer);
          }
          else
          {
            ASSERT_STR_EQ(cases[i].name, "Alice", task->params[1].value.string.ptr);
            ASSERT_STR_N_EQ(cases[i].name, "$argon2id$", task->params[2].value.string.ptr, 10);
          }
        }
      }

      complete = context.current_handler->func(&context, &db, task, &response);
      context.worker_result = NULL;
      password_job_free(job);
    }

    ASSERT_TRUE(cases[i].name, complete);
    ASSERT_EQ(cases[i].name, cases[i].expected, response.status);
    ASSERT_TRUE(cases[i].name, response.no_store);

    if (response.status == HTTP_STATUS_CREATED || response.status == HTTP_STATUS_OK)
    {
      ASSERT_TRUE(cases[i].name, inserted);
      ASSERT_STR_EQ(cases[i].name, "application/json", response.content_type);
      json_error_t error;
      json_t *body = json_loadb(response.body, response.body_len, JSON_REJECT_DUPLICATES, &error);
      ASSERT_NOT_NULL(cases[i].name, body);

      if (cases[i].login)
      {
        const char *token = json_string_value(json_object_get(body, "token"));
        auth_claims_t claims;
        ASSERT_NOT_NULL(cases[i].name, token);
        ASSERT_EQ(cases[i].name, AUTH_OK, auth_token_verify(&config, string_from_cstr(token), now, &claims));
        ASSERT_STR_EQ(cases[i].name, USER_ID, claims.user_id);
      }
      else
      {
        ASSERT_STR_EQ(cases[i].name, "Alice", json_string_value(json_object_get(body, "username")));
        ASSERT_TRUE(cases[i].name, json_object_get(body, "password_hash") == NULL);
      }

      json_decref(body);
    }
    else
    {
      ASSERT_EQ(cases[i].name, (size_t)0, response.body_len);
    }

    ASSERT_NULL(cases[i].name, db.task_queue->head);
    task_queue_free(db.task_queue);
    http_request_dispose(request);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }

  intercept_worker = false;
  password_worker_free(worker);
  close(epoll_fd);
  auth_config_dispose(&config);
}

static unsigned protected_calls;
static bool
protected_handler(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  protected_calls++;
  *response = (http_response_t){.status = auth_request_user(ctx->request).ptr ? HTTP_STATUS_OK : HTTP_STATUS_INTERNAL_SERVER_ERROR};
  return true;
}

static void
test_handle_auth(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *scheme;
    bool missing;
    bool duplicate;
    bool revoked;
    bool wrong_user;
    bool wrong_expiry;
    bool db_error;
    bool advance_time;
    bool allocation_failure;
    http_status expected;
  } cases[] = {
      {.name = "verified session", .scheme = "Bearer ", .expected = HTTP_STATUS_OK},
      {.name = "case insensitive scheme", .scheme = "bEaReR  ", .expected = HTTP_STATUS_OK},
      {.name = "missing header", .missing = true, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "duplicate header", .scheme = "Bearer ", .duplicate = true, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "wrong scheme", .scheme = "Basic ", .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "tab separator", .scheme = "Bearer\t", .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "revoked session", .scheme = "Bearer ", .revoked = true, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "wrong session user", .scheme = "Bearer ", .wrong_user = true, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "wrong session expiry", .scheme = "Bearer ", .wrong_expiry = true, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "expired while DB pending", .scheme = "Bearer ", .advance_time = true, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "session DB error", .scheme = "Bearer ", .db_error = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "identity allocation", .scheme = "Bearer ", .allocation_failure = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
  };
  auth_config_t config;
  auth_config_load(&config, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "ctf", "client", NULL);
  char *token;
  auth_token_issue(&config, string_from_cstr(USER_ID), string_from_cstr(SESSION_ID), 1000, &token);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    protected_calls = 0;
    int64_t now = 1000;
    auth_runtime_t runtime = {.config = &config, .now = fixed_now, .clock_data = &now};
    http_handler_t nodes[3];
    const http_handler_func_t functions[] = {handle_auth_1, handle_auth_2, protected_handler};
    make_chain(nodes, functions, 3);
    http_request_t *request = calloc(1, sizeof(*request));
    char header[AUTH_TOKEN_MAX + 32];
    snprintf(header, sizeof(header), "%s%s", cases[i].scheme ? cases[i].scheme : "Bearer ", token);
    request->headers[0] = (http_header_t){.name = "authorization", .name_len = 13, .value = header, .value_len = strlen(header)};
    request->headers[1] = request->headers[0];
    request->header_count = cases[i].missing ? 0 : cases[i].duplicate ? 2
                                                                      : 1;
    http_request_context_t context = {.request = request, .app_context = &runtime, .current_handler = nodes};
    db_pool_t db = {.task_queue = task_queue_new()};
    http_response_t response;
    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    bool complete = handle_auth_1(&context, &db, NULL, &response);
    test_set_calloc_failure(-1);

    if (!complete)
    {
      ASSERT_NULL(cases[i].name, auth_request_user(request).ptr);
      db_task_t *task = task_queue_pop(db.task_queue);
      ASSERT_TRUE(cases[i].name, strstr(task->query, "JOIN users") != NULL);
      ASSERT_TRUE(cases[i].name, strstr(task->query, "password_hash") == NULL);
      const char *rows[][6] = {{SESSION_ID, cases[i].wrong_user ? OTHER_ID : USER_ID, "1000", cases[i].wrong_expiry ? "4601" : "4600"}};
      task->result->success = !cases[i].db_error;
      task->result->res = test_mysql_result(rows, cases[i].revoked ? 0 : 1, 4);
      now = cases[i].advance_time ? 4630 : 1000;
      ASSERT_TRUE(cases[i].name, handle_auth_2(&context, &db, task, &response));
    }

    ASSERT_EQ(cases[i].name, cases[i].expected, response.status);
    ASSERT_EQ(cases[i].name, cases[i].expected == HTTP_STATUS_OK ? 1U : 0U, protected_calls);

    if (response.status == HTTP_STATUS_UNAUTHORIZED)
    {
      ASSERT_TRUE(cases[i].name, response.bearer_challenge);
    }

    ASSERT_NULL(cases[i].name, db.task_queue->head);
    task_queue_free(db.task_queue);
    http_request_dispose(request);
    CHECK_TEST(cases[i].name);
  }

  auth_token_free(token);
  auth_config_dispose(&config);
}

static void
test_handle_logout(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool verified;
    bool db_error;
    uint64_t affected;
    http_status expected;
  } cases[] = {
      {.name = "logout", .verified = true, .affected = 1, .expected = HTTP_STATUS_OK},
      {.name = "concurrent logout", .verified = true, .affected = 0, .expected = HTTP_STATUS_OK},
      {.name = "logout DB failure", .verified = true, .db_error = true, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "unexpected rows", .verified = true, .affected = 2, .expected = HTTP_STATUS_INTERNAL_SERVER_ERROR},
      {.name = "unverified logout", .expected = HTTP_STATUS_UNAUTHORIZED},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    auth_identity_t identity = {.claims.user_id = USER_ID, .claims.session_id = SESSION_ID, .verified = cases[i].verified};
    http_request_t request = {.auth_data = &identity};
    http_handler_t last = {.func = handle_logout_2};
    http_handler_t first = {.func = handle_logout_1, .next = &last};
    http_request_context_t context = {.request = &request, .current_handler = &first};
    db_pool_t db = {.task_queue = task_queue_new()};
    http_response_t response;

    if (!handle_logout_1(&context, &db, NULL, &response))
    {
      db_task_t *task = task_queue_pop(db.task_queue);
      ASSERT_STR_EQ(cases[i].name, SESSION_ID, task->params[0].value.string.ptr);
      ASSERT_STR_EQ(cases[i].name, USER_ID, task->params[1].value.string.ptr);
      task->result->success = !cases[i].db_error;
      task->result->affected = cases[i].affected;
      ASSERT_TRUE(cases[i].name, handle_logout_2(&context, &db, task, &response));
    }

    ASSERT_EQ(cases[i].name, cases[i].expected, response.status);
    ASSERT_EQ(cases[i].name, (size_t)0, response.body_len);
    task_queue_free(db.task_queue);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_protected_entries(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    http_handler_func_t handler;
  } cases[] = {
      {.name = "POST challenges", .handler = handle_post_challenges_1},
      {.name = "PUT challenges", .handler = handle_put_challenges_1},
      {.name = "DELETE challenges", .handler = handle_delete_challenges_1},
      {.name = "POST answers", .handler = handle_post_answers_1},
      {.name = "GET own answers", .handler = handle_get_own_answers_1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    http_request_t request = {0};
    http_request_context_t context = {.request = &request};
    http_response_t response;
    ASSERT_TRUE(cases[i].name, cases[i].handler(&context, NULL, NULL, &response));
    ASSERT_EQ(cases[i].name, (http_status)HTTP_STATUS_UNAUTHORIZED, response.status);
    ASSERT_TRUE(cases[i].name, response.bearer_challenge);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_auth_lifecycle(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const http_handler_func_t *functions;
    size_t count;
    size_t token_index;
    http_status expected;
  } cases[] = {
      {.name = "register", .functions = signup_functions, .count = 5, .expected = HTTP_STATUS_CREATED},
      {.name = "first login", .functions = login_functions, .count = 5, .token_index = 0, .expected = HTTP_STATUS_OK},
      {.name = "second login", .functions = login_functions, .count = 5, .token_index = 1, .expected = HTTP_STATUS_OK},
      {.name = "protected request", .token_index = 0, .expected = HTTP_STATUS_OK},
      {.name = "logout first session", .token_index = 0, .expected = HTTP_STATUS_OK},
      {.name = "reject logged out token", .token_index = 0, .expected = HTTP_STATUS_UNAUTHORIZED},
      {.name = "retain other session", .token_index = 1, .expected = HTTP_STATUS_OK},
  };
  const http_handler_func_t protected_functions[] = {handle_auth_1, handle_auth_2, protected_handler};
  const http_handler_func_t logout_functions[] = {handle_auth_1, handle_auth_2, handle_logout_1, handle_logout_2};
  auth_config_t config;
  auth_config_load(&config, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "ctf", "client", NULL);
  auth_password_hash(string_from_cstr("password"), config.dummy_hash);
  int epoll_fd = epoll_create1(0);
  password_worker_t *worker = password_worker_new(&config, epoll_fd);
  int64_t now = 1000;
  auth_runtime_t runtime = {.config = &config, .password_worker = worker, .now = fixed_now, .clock_data = &now};
  char user_id[AUTH_ID_LENGTH + 1] = {0};
  char hash[crypto_pwhash_STRBYTES] = {0};
  char session_ids[2][AUTH_ID_LENGTH + 1] = {{0}};
  char *tokens[2] = {0};
  bool active[2] = {false};
  intercept_worker = false;

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    bool logout = i == 4;
    http_handler_t nodes[5];
    make_chain(nodes, cases[i].functions ? cases[i].functions : logout ? logout_functions
                                                                       : protected_functions,
               cases[i].count ? cases[i].count : logout ? 4
                                                        : 3);
    http_request_t *request = calloc(1, sizeof(*request));
    request->body = CREDENTIALS;
    request->content_length = strlen(CREDENTIALS);
    char header[AUTH_TOKEN_MAX + 16];

    if (!cases[i].functions)
    {
      snprintf(header, sizeof(header), "Bearer %s", tokens[cases[i].token_index]);
      request->headers[0] = (http_header_t){.name = "Authorization", .name_len = 13, .value = header, .value_len = strlen(header)};
      request->header_count = 1;
    }

    http_request_context_t context = {.request = request, .app_context = &runtime, .current_handler = nodes};
    db_pool_t db = {.task_queue = task_queue_new()};
    http_response_t response;
    bool complete = nodes[0].func(&context, &db, NULL, &response);
    unsigned steps = 0;

    while (!complete && steps++ < 8)
    {
      db_task_t *task = NULL;
      password_job_t *job = NULL;

      if (context.current_handler->func == handle_signup_3 || context.current_handler->func == handle_login_3)
      {
        job = wait_password(worker);
        ASSERT_NOT_NULL(cases[i].name, job);

        if (!job)
        {
          break;
        }

        context.worker_result = &job->result;
      }
      else
      {
        ASSERT_NOT_NULL(cases[i].name, db.task_queue->head);

        if (!db.task_queue->head)
        {
          break;
        }

        task = task_queue_pop(db.task_queue);
        task->result->success = true;
        task->result->affected = 1;

        if (strstr(task->query, "SELECT id, username"))
        {
          const char *rows[][6] = {{user_id, "Alice", hash}};
          task->result->res = test_mysql_result(rows, user_id[0] ? 1 : 0, 3);
        }
        else if (strstr(task->query, "JOIN users"))
        {
          const char *rows[2][6] = {{0}};
          size_t count = 0;

          for (size_t j = 0; j < 2; j++)
          {
            if (active[j])
            {
              rows[count][0] = session_ids[j];
              rows[count][1] = user_id;
              rows[count][2] = "1000";
              rows[count++][3] = "4600";
            }
          }

          task->result->res = test_mysql_result(rows, count, 4);
        }
        else if (strstr(task->query, "INSERT INTO users"))
        {
          snprintf(user_id, sizeof(user_id), "%s", task->params[0].value.string.ptr);
          snprintf(hash, sizeof(hash), "%s", task->params[2].value.string.ptr);
        }
        else if (strstr(task->query, "INSERT INTO auth_sessions"))
        {
          size_t index = cases[i].token_index;
          snprintf(session_ids[index], sizeof(session_ids[index]), "%s", task->params[0].value.string.ptr);
          ASSERT_STR_EQ(cases[i].name, user_id, task->params[1].value.string.ptr);
          active[index] = true;
        }
        else if (logout)
        {
          ASSERT_STR_EQ(cases[i].name, user_id, task->params[1].value.string.ptr);

          for (size_t j = 0; j < 2; j++)
          {
            if (strcmp(session_ids[j], task->params[0].value.string.ptr) == 0)
            {
              active[j] = false;
            }
          }
        }
      }

      complete = context.current_handler->func(&context, &db, task, &response);
      context.worker_result = NULL;
      password_job_free(job);
    }

    ASSERT_TRUE(cases[i].name, complete);
    ASSERT_EQ(cases[i].name, cases[i].expected, response.status);

    if (cases[i].functions == login_functions && response.status == HTTP_STATUS_OK)
    {
      json_t *body = json_loadb(response.body, response.body_len, 0, NULL);
      const char *token = json_string_value(json_object_get(body, "token"));
      ASSERT_NOT_NULL(cases[i].name, token);
      tokens[cases[i].token_index] = malloc(strlen(token) + 1);
      strcpy(tokens[cases[i].token_index], token);
      json_decref(body);
    }

    task_queue_free(db.task_queue);
    http_request_dispose(request);
    ASSERT_EQ(cases[i].name, 0, test_mysql_live_results);
    CHECK_TEST(cases[i].name);
  }

  auth_token_free(tokens[0]);
  auth_token_free(tokens[1]);
  sodium_memzero(hash, sizeof(hash));
  password_worker_free(worker);
  close(epoll_fd);
  auth_config_dispose(&config);
}

void
test_app_auth_api(test_ctx_t *ctx)
{
  test_signup_login(ctx);
  test_handle_auth(ctx);
  test_handle_logout(ctx);
  test_protected_entries(ctx);
  test_auth_lifecycle(ctx);
}
