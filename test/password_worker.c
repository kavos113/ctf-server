#define _POSIX_C_SOURCE 200809L

#include "test.h"
#include "util.h"

#include <app/password_worker.h>
#include <errno.h>
#include <fcntl.h>
#include <http_server.h>
#include <poll.h>
#include <pthread.h>
#include <server_p.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

static struct
{
  pthread_mutex_t mutex;
  pthread_cond_t changed;
  bool enabled;
  bool blocked;
  unsigned entered;
  bool off_main_thread;
  pthread_t main_thread;
  auth_result result;
  char password[129];
  char stored_hash[crypto_pwhash_STRBYTES];
} mock = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};

auth_result __real_auth_password_hash(string_t password, char *out);
auth_result __real_auth_password_verify(const auth_config_t *config, const char *hash, string_t password);

static bool
mock_calculation(string_t password, const char *hash, auth_result *result)
{
  pthread_mutex_lock(&mock.mutex);

  if (!mock.enabled)
  {
    pthread_mutex_unlock(&mock.mutex);
    return false;
  }

  mock.entered++;
  mock.off_main_thread &= !pthread_equal(mock.main_thread, pthread_self());
  pthread_cond_broadcast(&mock.changed);

  while (mock.blocked)
  {
    pthread_cond_wait(&mock.changed, &mock.mutex);
  }

  memcpy(mock.password, password.ptr, password.len);
  mock.password[password.len] = '\0';
  snprintf(mock.stored_hash, sizeof(mock.stored_hash), "%s", hash ? hash : "");
  *result = mock.result;
  pthread_mutex_unlock(&mock.mutex);
  return true;
}

auth_result
__wrap_auth_password_hash(string_t password, char *out)
{
  auth_result result;

  if (!mock_calculation(password, NULL, &result))
  {
    return __real_auth_password_hash(password, out);
  }

  if (result == AUTH_OK)
  {
    strcpy(out, "$argon2id$mock");
  }

  return result;
}

auth_result
__wrap_auth_password_verify(const auth_config_t *config, const char *hash, string_t password)
{
  auth_result result;
  return mock_calculation(password, hash, &result) ? result : __real_auth_password_verify(config, hash, password);
}

static void
set_mock(bool enabled, bool blocked, auth_result result)
{
  pthread_mutex_lock(&mock.mutex);
  mock.enabled = enabled;
  mock.blocked = blocked;
  mock.entered = 0;
  mock.off_main_thread = true;
  mock.main_thread = pthread_self();
  mock.result = result;
  memset(mock.password, 0, sizeof(mock.password));
  memset(mock.stored_hash, 0, sizeof(mock.stored_hash));
  pthread_mutex_unlock(&mock.mutex);
}

static bool
wait_running(void)
{
  struct timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_sec += 3;
  pthread_mutex_lock(&mock.mutex);

  while (!mock.entered)
  {
    if (pthread_cond_timedwait(&mock.changed, &mock.mutex, &deadline) != 0)
    {
      pthread_mutex_unlock(&mock.mutex);
      return false;
    }
  }

  pthread_mutex_unlock(&mock.mutex);
  return true;
}

static void
release_calculation(void)
{
  pthread_mutex_lock(&mock.mutex);
  mock.blocked = false;
  pthread_cond_broadcast(&mock.changed);
  pthread_mutex_unlock(&mock.mutex);
}

static password_job_t *
wait_job(password_worker_t *worker)
{
  for (int i = 0; i < 64; i++)
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

static auth_config_t
mock_config(void)
{
  auth_config_t config = {0};
  strcpy(config.dummy_hash, "$argon2id$dummy");
  return config;
}

enum startup_failure
{
  START_OK,
  FAIL_MUTEX,
  FAIL_COND,
  FAIL_EVENTFD,
  FAIL_THREAD,
};
static enum startup_failure startup_failure;
static int created_eventfd;
int __real_pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int __real_pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int __real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int __real_eventfd(unsigned int, int);

int
__wrap_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
  return startup_failure == FAIL_MUTEX ? EAGAIN : __real_pthread_mutex_init(mutex, attr);
}

int
__wrap_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  return startup_failure == FAIL_COND ? EAGAIN : __real_pthread_cond_init(cond, attr);
}

int
__wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*entry)(void *), void *arg)
{
  return startup_failure == FAIL_THREAD ? EAGAIN : __real_pthread_create(thread, attr, entry, arg);
}

int
__wrap_eventfd(unsigned int initial, int flags)
{
  if (startup_failure == FAIL_EVENTFD)
  {
    errno = EMFILE;
    return -1;
  }

  created_eventfd = __real_eventfd(initial, flags);
  return created_eventfd;
}

static void
test_password_worker_new(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    enum startup_failure failure;
    bool allocation_failure;
    bool invalid_epoll;
    bool invalid_config;
    bool success;
  } cases[] = {
      {.name = "start and stop idle", .success = true},
      {.name = "allocation", .allocation_failure = true},
      {.name = "mutex", .failure = FAIL_MUTEX},
      {.name = "condition", .failure = FAIL_COND},
      {.name = "eventfd", .failure = FAIL_EVENTFD},
      {.name = "epoll registration", .invalid_epoll = true},
      {.name = "thread creation", .failure = FAIL_THREAD},
      {.name = "missing dummy", .invalid_config = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    int epoll_fd = epoll_create1(0);
    auth_config_t config = mock_config();

    if (cases[i].invalid_config)
    {
      config.dummy_hash[0] = '\0';
    }

    created_eventfd = -1;
    startup_failure = cases[i].failure;
    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    password_worker_t *worker = password_worker_new(&config, cases[i].invalid_epoll ? -1 : epoll_fd);
    test_set_calloc_failure(-1);
    startup_failure = START_OK;
    ASSERT_EQ(cases[i].name, cases[i].success, worker != NULL);
    password_worker_free(worker);

    if (created_eventfd >= 0)
    {
      ASSERT_EQ(cases[i].name, -1, fcntl(created_eventfd, F_GETFD));
      ASSERT_EQ(cases[i].name, EBADF, errno);
    }

    struct epoll_event event;
    ASSERT_EQ(cases[i].name, 0, epoll_wait(epoll_fd, &event, 1, 0));
    close(epoll_fd);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_password_worker_submit(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    password_operation operation;
    bool unknown;
    bool stop;
    bool allocation_failure;
    const char *password;
    auth_result calculation;
    password_submit_result submit;
    auth_result expected;
  } cases[] = {
      {.name = "hash", .operation = PASSWORD_HASH, .password = "password", .calculation = AUTH_OK, .submit = PASSWORD_QUEUED, .expected = AUTH_OK},
      {.name = "verify", .operation = PASSWORD_VERIFY, .password = "password", .calculation = AUTH_OK, .submit = PASSWORD_QUEUED, .expected = AUTH_OK},
      {.name = "mismatch", .operation = PASSWORD_VERIFY, .password = "password", .calculation = AUTH_INVALID, .submit = PASSWORD_QUEUED, .expected = AUTH_INVALID},
      {.name = "unknown cannot succeed", .operation = PASSWORD_VERIFY, .password = "password", .unknown = true, .calculation = AUTH_OK, .submit = PASSWORD_QUEUED, .expected = AUTH_INVALID},
      {.name = "calculation error", .operation = PASSWORD_HASH, .password = "password", .calculation = AUTH_ERROR, .submit = PASSWORD_QUEUED, .expected = AUTH_ERROR},
      {.name = "short input", .operation = PASSWORD_HASH, .password = "short", .submit = PASSWORD_INVALID_INPUT},
      {.name = "invalid operation", .operation = (password_operation)99, .password = "password", .submit = PASSWORD_INVALID_INPUT},
      {.name = "allocation error", .operation = PASSWORD_HASH, .password = "password", .allocation_failure = true, .submit = PASSWORD_SUBMIT_ERROR},
      {.name = "stopped", .operation = PASSWORD_HASH, .password = "password", .stop = true, .submit = PASSWORD_WORKER_STOPPED},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    set_mock(true, true, cases[i].calculation);
    int epoll_fd = epoll_create1(0);
    auth_config_t config = mock_config();
    password_worker_t *worker = password_worker_new(&config, epoll_fd);
    char hash[] = "$argon2id$original";
    string_t password = string_from_cstr_dup(cases[i].password);

    if (cases[i].stop)
    {
      password_worker_stop(worker);
    }

    test_set_calloc_failure(cases[i].allocation_failure ? 0 : -1);
    password_submit_result submitted = password_worker_submit(worker, cases[i].operation, &password,
                                                              cases[i].unknown ? NULL : hash, ctx);
    test_set_calloc_failure(-1);
    ASSERT_EQ(cases[i].name, cases[i].submit, submitted);
    ASSERT_NULL(cases[i].name, password.ptr);
    ASSERT_EQ(cases[i].name, (size_t)0, password.len);
    memset(hash, 'x', strlen(hash));
    auth_config_dispose(&config);

    if (submitted == PASSWORD_QUEUED)
    {
      ASSERT_TRUE(cases[i].name, wait_running());
      release_calculation();
      password_job_t *job = wait_job(worker);
      ASSERT_NOT_NULL(cases[i].name, job);

      if (job)
      {
        ASSERT_EQ(cases[i].name, cases[i].expected, job->result.status);
        ASSERT_FALSE(cases[i].name, job->result.cancelled);
        ASSERT_TRUE(cases[i].name, job->data == ctx);
        ASSERT_TRUE(cases[i].name, sodium_is_zero((unsigned char *)job->password, sizeof(job->password)));
        ASSERT_TRUE(cases[i].name, sodium_is_zero((unsigned char *)job->stored_hash, sizeof(job->stored_hash)));
        ASSERT_EQ(cases[i].name, (size_t)0, job->password_length);

        if (cases[i].operation == PASSWORD_HASH && cases[i].expected == AUTH_OK)
        {
          ASSERT_STR_EQ(cases[i].name, "$argon2id$mock", job->result.hash);
        }

        password_job_free(job);
      }

      pthread_mutex_lock(&mock.mutex);
      ASSERT_TRUE(cases[i].name, mock.off_main_thread);
      ASSERT_STR_EQ(cases[i].name, cases[i].password, mock.password);

      if (cases[i].operation == PASSWORD_VERIFY)
      {
        ASSERT_STR_EQ(cases[i].name, cases[i].unknown ? "$argon2id$dummy" : "$argon2id$original", mock.stored_hash);
      }

      pthread_mutex_unlock(&mock.mutex);
    }

    release_calculation();
    password_worker_free(worker);
    close(epoll_fd);
    set_mock(false, false, AUTH_OK);
    CHECK_TEST(cases[i].name);
  }
}

static void *
stop_worker(void *worker)
{
  password_worker_stop(worker);
  return NULL;
}

static void
test_password_worker_stop(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool cancel;
  } cases[] = {
      {.name = "bounded queue and FIFO completion"},
      {.name = "cancel waiting jobs", .cancel = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    set_mock(true, true, AUTH_OK);
    int epoll_fd = epoll_create1(0);
    auth_config_t config = mock_config();
    password_worker_t *worker = password_worker_new(&config, epoll_fd);
    int identifiers[PASSWORD_QUEUE_CAPACITY + 1];
    string_t password = string_from_cstr_dup("password");
    ASSERT_EQ(cases[i].name, PASSWORD_QUEUED, password_worker_submit(worker, PASSWORD_HASH, &password, NULL, &identifiers[0]));
    ASSERT_TRUE(cases[i].name, wait_running());

    for (size_t j = 1; j <= PASSWORD_QUEUE_CAPACITY; j++)
    {
      password = string_from_cstr_dup("password");
      ASSERT_EQ(cases[i].name, PASSWORD_QUEUED, password_worker_submit(worker, PASSWORD_HASH, &password, NULL, &identifiers[j]));
    }

    password = string_from_cstr_dup("password");
    ASSERT_EQ(cases[i].name, PASSWORD_QUEUE_FULL, password_worker_submit(worker, PASSWORD_HASH, &password, NULL, NULL));
    ASSERT_NULL(cases[i].name, password.ptr);
    pthread_t stopping;

    if (cases[i].cancel)
    {
      pthread_create(&stopping, NULL, stop_worker, worker);
      struct pollfd fd = {.fd = password_worker_notify_fd(worker), .events = POLLIN};
      ASSERT_EQ(cases[i].name, 1, poll(&fd, 1, 3000));
    }

    release_calculation();

    if (cases[i].cancel)
    {
      pthread_join(stopping, NULL);
    }

    unsigned cancelled = 0;
    bool seen[PASSWORD_QUEUE_CAPACITY + 1] = {0};

    for (size_t j = 0; j <= PASSWORD_QUEUE_CAPACITY; j++)
    {
      password_job_t *job = wait_job(worker);
      ASSERT_NOT_NULL(cases[i].name, job);

      if (!job)
      {
        break;
      }

      ptrdiff_t index = (int *)job->data - identifiers;
      ASSERT_TRUE(cases[i].name, index >= 0 && index <= PASSWORD_QUEUE_CAPACITY);

      if (index >= 0 && index <= PASSWORD_QUEUE_CAPACITY)
      {
        ASSERT_FALSE(cases[i].name, seen[index]);
        seen[index] = true;
      }

      if (!cases[i].cancel)
      {
        ASSERT_EQ(cases[i].name, (ptrdiff_t)j, index);
      }

      cancelled += job->result.cancelled;
      ASSERT_TRUE(cases[i].name, sodium_is_zero((unsigned char *)job->password, sizeof(job->password)));
      password_job_free(job);
    }

    ASSERT_EQ(cases[i].name, cases[i].cancel ? (unsigned)PASSWORD_QUEUE_CAPACITY : 0U, cancelled);
    ASSERT_NULL(cases[i].name, password_worker_pop(worker));
    password_worker_stop(worker);
    password_worker_stop(worker);
    password_worker_free(worker);
    close(epoll_fd);
    set_mock(false, false, AUTH_OK);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_password_worker_real(test_ctx_t *ctx)
{
  ctx->is_canceled = false;
  set_mock(false, false, AUTH_OK);
  auth_config_t config = {0};
  ASSERT_EQ("real worker", AUTH_OK, auth_password_hash(string_from_cstr("password"), config.dummy_hash));
  int epoll_fd = epoll_create1(0);
  password_worker_t *worker = password_worker_new(&config, epoll_fd);
  const struct
  {
    password_operation operation;
    const char *password;
    auth_result result;
  } cases[] = {
      {.operation = PASSWORD_HASH, .password = "password", .result = AUTH_OK},
      {.operation = PASSWORD_VERIFY, .password = "password", .result = AUTH_OK},
      {.operation = PASSWORD_VERIFY, .password = "different", .result = AUTH_INVALID},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    string_t password = string_from_cstr_dup(cases[i].password);
    ASSERT_EQ("real worker", PASSWORD_QUEUED, password_worker_submit(worker, cases[i].operation, &password, config.dummy_hash, NULL));
    password_job_t *job = wait_job(worker);
    ASSERT_NOT_NULL("real worker", job);

    if (job)
    {
      ASSERT_EQ("real worker", cases[i].result, job->result.status);

      if (cases[i].operation == PASSWORD_HASH)
      {
        ASSERT_EQ("real worker hash", AUTH_OK, auth_password_verify(NULL, job->result.hash, string_from_cstr("password")));
      }

      password_job_free(job);
    }
  }

  password_worker_free(worker);
  auth_config_dispose(&config);
  close(epoll_fd);
  CHECK_TEST("real worker");
}

static int disposals;
static int callbacks;
static bool correct_callback;
static pthread_t callback_thread;

static void
dispose_state(void *data)
{
  disposals++;
  free(data);
}

static bool
complete_password(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  callbacks++;
  password_result_t *result = ctx->worker_result;
  correct_callback &= task == NULL && result && result->status == AUTH_OK &&
                      pthread_equal(callback_thread, pthread_self());
  *response = (http_response_t){.status = HTTP_STATUS_OK};

  if (ctx->current_handler->next)
  {
    ctx->current_handler = ctx->current_handler->next;
    const char query[] = "INSERT INTO users (password_hash) VALUES (?)";
    db_param_t param = {.type = DB_PARAM_STRING,
                        .value.string = {.ptr = result->hash, .len = strlen(result->hash)}};
    return db_exec_query_param(db, query, sizeof(query) - 1, &param, 1, ctx) < 0;
  }

  return true;
}

static bool
complete_database(http_request_context_t *ctx, db_pool_t *db, db_task_t *task, http_response_t *response)
{
  callbacks++;
  correct_callback &= ctx->worker_result == NULL && task != NULL && task->param_count == 1 &&
                      strcmp(task->params[0].value.string.ptr, "$argon2id$mock") == 0;
  db_task_free(task);
  *response = (http_response_t){.status = HTTP_STATUS_OK};
  return true;
}

static http_request_context_t *
new_context(server_t *server, http_handler_t *handler)
{
  connection_t *connection = calloc(1, sizeof(*connection));
  connection->fd = eventfd(0, EFD_NONBLOCK);
  connection->type = FD_TYPE_CLIENT;
  connection->next = server->clients;

  if (server->clients)
  {
    server->clients->previous = connection;
  }

  server->clients = connection;
  http_request_t *request = calloc(1, sizeof(*request));
  connection->state.client.request = request;
  request->conn = connection;
  request->app_data = calloc(1, 1);
  request->dispose_app_data = dispose_state;
  http_request_context_t *context = calloc(1, sizeof(*context));
  context->request = request;
  context->current_handler = handler;
  request->context = context;
  return context;
}

static void
test_password_worker_handler(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool disconnect;
    bool shutdown;
    bool database;
    unsigned count;
  } cases[] = {
      {.name = "worker completion", .count = 1},
      {.name = "multiple notifications", .count = 3},
      {.name = "disconnect while computing", .disconnect = true, .count = 1},
      {.name = "shutdown pending requests", .shutdown = true, .count = 3},
      {.name = "worker then database", .database = true, .count = 1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    disposals = callbacks = 0;
    correct_callback = true;
    callback_thread = pthread_self();
    set_mock(true, true, AUTH_OK);
    server_t *server = calloc(1, sizeof(*server));
    server->epoll_fd = epoll_create1(0);
    server->listen_conn.fd = server->signal_conn.fd = -1;
    server->db_pool = calloc(1, sizeof(*server->db_pool));
    server->db_pool->task_queue = task_queue_new();
    server->db_pool->done_queue = task_queue_new();
    server->db_pool->epoll_fd = -1;
    server->db_pool->notify_fd = eventfd(0, EFD_NONBLOCK);
    auth_config_t config = mock_config();
    server->password_worker = password_worker_new(&config, server->epoll_fd);
    http_handler_t db_handler_node = {.func = complete_database};
    http_handler_t handler = {.func = complete_password, .next = cases[i].database ? &db_handler_node : NULL};

    for (unsigned j = 0; j < cases[i].count; j++)
    {
      http_request_context_t *context = new_context(server, &handler);
      string_t password = string_from_cstr_dup("password");
      ASSERT_EQ(cases[i].name, PASSWORD_QUEUED,
                password_worker_submit(server->password_worker, PASSWORD_HASH, &password, NULL, context));

      if (j == 0)
      {
        ASSERT_TRUE(cases[i].name, wait_running());
      }
    }

    if (cases[i].disconnect)
    {
      remove_connection(server, server->clients);
      reap_connections(server);
      ASSERT_EQ(cases[i].name, 0, disposals);
    }

    release_calculation();

    if (!cases[i].shutdown)
    {
      for (unsigned j = 0; j < cases[i].count && (unsigned)callbacks < cases[i].count && (unsigned)disposals < cases[i].count; j++)
      {
        struct epoll_event event;
        ASSERT_EQ(cases[i].name, 1, epoll_wait(server->epoll_fd, &event, 1, 3000));
        password_worker_handler(server, event.data.ptr);
      }

      password_worker_handler(server, NULL);

      if (cases[i].database)
      {
        ASSERT_EQ(cases[i].name, 0, disposals);
        db_task_t *task = task_queue_pop(server->db_pool->task_queue);
        task->result->success = 1;
        task_queue_push(server->db_pool->done_queue, task);
        eventfd_write(server->db_pool->notify_fd, 1);
        db_handler(server, NULL);
      }

      ASSERT_EQ(cases[i].name, (int)cases[i].count, disposals);
    }

    destroy_server(server);
    ASSERT_EQ(cases[i].name, (int)cases[i].count, disposals);
    ASSERT_EQ(cases[i].name, cases[i].disconnect || cases[i].shutdown ? 0 : (int)cases[i].count + cases[i].database, callbacks);
    ASSERT_TRUE(cases[i].name, correct_callback);
    set_mock(false, false, AUTH_OK);
    CHECK_TEST(cases[i].name);
  }
}

void
test_password_worker(test_ctx_t *ctx)
{
  test_password_worker_new(ctx);
  test_password_worker_submit(ctx);
  test_password_worker_stop(ctx);
  test_password_worker_real(ctx);
  test_password_worker_handler(ctx);
}
