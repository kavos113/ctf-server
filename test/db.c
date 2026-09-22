#include "test.h"
#include "util.h"

#include <db.h>
#include <limits.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>

// Only the test executable wraps these calls; no DB connection is established.
void *__real_calloc(size_t count, size_t size);
static int fail_calloc_after = -1;

void
test_set_calloc_failure(int after)
{
  fail_calloc_after = after;
}

void *
__wrap_calloc(size_t count, size_t size)
{
  if (fail_calloc_after == 0)
  {
    return NULL;
  }

  if (fail_calloc_after > 0)
  {
    fail_calloc_after--;
  }

  return __real_calloc(count, size);
}

static struct
{
  bool init_fail;
  bool prepare_fail;
  bool bind_fail;
  bool execute_fail;
  bool returns_rows;
  bool valid_bindings;
  unsigned long param_count;
  unsigned prepare_calls;
  unsigned bind_calls;
  unsigned execute_calls;
  unsigned close_calls;
  unsigned query_calls;
  const char *query;
  const db_param_t *params;
  MYSQL_BIND *bindings;
} mock;
static MYSQL connection;
static MYSQL_STMT statement;

MYSQL *
__wrap_mysql_init(MYSQL *mysql)
{
  return &connection;
}

MYSQL *
__wrap_mysql_real_connect(MYSQL *mysql,
                          const char *host,
                          const char *user,
                          const char *password,
                          const char *db,
                          unsigned int port,
                          const char *socket,
                          unsigned long flags)
{
  return mysql;
}

void
__wrap_mysql_close(MYSQL *mysql)
{
}

void
__wrap_mysql_thread_end(void)
{
}

MYSQL_STMT *
__wrap_mysql_stmt_init(MYSQL *mysql)
{
  return mock.init_fail ? NULL : &statement;
}

int
__wrap_mysql_stmt_prepare(MYSQL_STMT *stmt, const char *query, unsigned long length)
{
  mock.prepare_calls++;
  mock.valid_bindings &= length == strlen(mock.query) && memcmp(query, mock.query, length) == 0;
  return mock.prepare_fail;
}

unsigned int
__wrap_mysql_stmt_field_count(MYSQL_STMT *stmt)
{
  return mock.returns_rows ? 1 : 0;
}

unsigned long
__wrap_mysql_stmt_param_count(MYSQL_STMT *stmt)
{
  return mock.param_count;
}

bool
__wrap_mysql_stmt_bind_param(MYSQL_STMT *stmt, MYSQL_BIND *bindings)
{
  mock.bind_calls++;
  mock.bindings = bindings;
  return mock.bind_fail;
}

int
__wrap_mysql_stmt_execute(MYSQL_STMT *stmt)
{
  mock.execute_calls++;

  // Read buffers at execute time to verify their lifetime as well as their types.

  for (unsigned long i = 0; i < mock.param_count; i++)
  {
    const db_param_t *param = &mock.params[i];
    MYSQL_BIND *binding = &mock.bindings[i];

    switch (param->type)
    {
    case DB_PARAM_STRING:
      mock.valid_bindings &= binding->buffer_type == MYSQL_TYPE_STRING && binding->length &&
                             *binding->length == param->value.string.len &&
                             binding->buffer_length == param->value.string.len;

      if (param->value.string.len)
      {
        mock.valid_bindings &=
            memcmp(binding->buffer, param->value.string.ptr, param->value.string.len) == 0;
      }

      break;

    case DB_PARAM_INT64:
      mock.valid_bindings &= binding->buffer_type == MYSQL_TYPE_LONGLONG && !binding->is_unsigned &&
                             *(int64_t *)binding->buffer == param->value.integer;
      break;

    case DB_PARAM_NULL:
      mock.valid_bindings &= binding->buffer_type == MYSQL_TYPE_NULL;
      break;
    }
  }

  return mock.execute_fail;
}

bool
__wrap_mysql_stmt_close(MYSQL_STMT *stmt)
{
  mock.close_calls++;
  return false;
}

const char *
__wrap_mysql_stmt_error(MYSQL_STMT *stmt)
{
  return "mock statement error";
}

uint64_t
__wrap_mysql_stmt_affected_rows(MYSQL_STMT *stmt)
{
  return 3;
}

uint64_t
__wrap_mysql_stmt_insert_id(MYSQL_STMT *stmt)
{
  return 123;
}

int
__wrap_mysql_query(MYSQL *mysql, const char *query)
{
  mock.query_calls++;
  mock.valid_bindings &= strcmp(query, mock.query) == 0;
  return 0;
}

MYSQL_RES *
__wrap_mysql_store_result(MYSQL *mysql)
{
  return NULL;
}

uint64_t
__wrap_mysql_affected_rows(MYSQL *mysql)
{
  return 7;
}

static void
test_db_exec_query_param(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_db_exec_query_param");

  char max_query[DEFAULT_QUERY_SIZE];
  memset(max_query, 'a', sizeof(max_query));
  char long_value[65536];
  memset(long_value, 'b', sizeof(long_value));
  const db_param_t params[] = {
      {.type = DB_PARAM_STRING, .value.string = {.ptr = "a\0b'\\", .len = 5}},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = NULL, .len = 0}},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = long_value, .len = sizeof(long_value)}},
      {.type = DB_PARAM_INT64, .value.integer = INT64_MIN},
      {.type = DB_PARAM_INT64, .value.integer = INT64_MAX},
      {.type = DB_PARAM_NULL},
  };
  const db_param_t bad_type = {.type = (db_param_type)999};
  const db_param_t bad_string = {.type = DB_PARAM_STRING, .value.string = {.ptr = NULL, .len = 1}};
  const db_param_t overflow = {.type = DB_PARAM_STRING, .value.string = {.ptr = "x", .len = SIZE_MAX}};
  struct test_case
  {
    const char *name;
    const char *query;
    size_t query_len;
    const db_param_t *params;
    size_t count;
    int fail_after;
    int expected;
    bool no_pool;
    bool no_queue;
  } cases[] = {
      {
          .name = "all types",
          .query = "INSERT INTO t VALUES (?, ?, ?, ?, ?, ?)",
          .query_len = 38,
          .params = params,
          .count = 6,
          .fail_after = -1,
          .expected = 0,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "no parameters",
          .query = "DELETE FROM t",
          .query_len = 13,
          .params = NULL,
          .count = 0,
          .fail_after = -1,
          .expected = 0,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "SQL boundary",
          .query = max_query,
          .query_len = DEFAULT_QUERY_SIZE - 1,
          .params = NULL,
          .count = 0,
          .fail_after = -1,
          .expected = 0,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "SQL too long",
          .query = max_query,
          .query_len = DEFAULT_QUERY_SIZE,
          .params = NULL,
          .count = 0,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "empty SQL",
          .query = "",
          .query_len = 0,
          .params = NULL,
          .count = 0,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "NULL SQL",
          .query = NULL,
          .query_len = 1,
          .params = NULL,
          .count = 0,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "NUL in SQL",
          .query = "a\0b",
          .query_len = 3,
          .params = NULL,
          .count = 0,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "NULL parameter array",
          .query = "UPDATE t SET a=?",
          .query_len = 16,
          .params = NULL,
          .count = 1,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "unknown type",
          .query = "UPDATE t SET a=?",
          .query_len = 16,
          .params = &bad_type,
          .count = 1,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "NULL nonempty string",
          .query = "UPDATE t SET a=?",
          .query_len = 16,
          .params = &bad_string,
          .count = 1,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "string size overflow",
          .query = "UPDATE t SET a=?",
          .query_len = 16,
          .params = &overflow,
          .count = 1,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "parameter count overflow",
          .query = "UPDATE t SET a=?",
          .query_len = 16,
          .params = params,
          .count = SIZE_MAX,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "task allocation failure",
          .query = "UPDATE t SET a=?",
          .query_len = 16,
          .params = params,
          .count = 1,
          .fail_after = 0,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "result allocation failure",
          .query = "UPDATE t SET a=?",
          .query_len = 16,
          .params = params,
          .count = 1,
          .fail_after = 1,
          .expected = -1,
          .no_pool = false,
          .no_queue = false,
      },
      {
          .name = "NULL pool",
          .query = "DELETE FROM t",
          .query_len = 13,
          .params = NULL,
          .count = 0,
          .fail_after = -1,
          .expected = -1,
          .no_pool = true,
          .no_queue = false,
      },
      {
          .name = "NULL queue",
          .query = "DELETE FROM t",
          .query_len = 13,
          .params = NULL,
          .count = 0,
          .fail_after = -1,
          .expected = -1,
          .no_pool = false,
          .no_queue = true,
      },
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &cases[i];

    task_queue_t *queue = task_queue_new();
    db_pool_t pool = {.task_queue = tc->no_queue ? NULL : queue};
    fail_calloc_after = tc->fail_after;
    int result = db_exec_query_param(
        tc->no_pool ? NULL : &pool, tc->query, tc->query_len, tc->params, tc->count, ctx);
    fail_calloc_after = -1;

    ASSERT_EQ(tc->name, tc->expected, result);

    if (result == 0)
    {
      db_task_t *task = task_queue_pop(queue);

      ASSERT_EQ(tc->name, tc->query_len, task->query_len);
      ASSERT_EQ(tc->name, 0, memcmp(tc->query, task->query, task->query_len));
      ASSERT_EQ(tc->name, (int)'\0', (int)task->query[task->query_len]);
      ASSERT_EQ(tc->name, tc->count, task->param_count);
      ASSERT_TRUE(tc->name, task->data == ctx && task->result != NULL);

      for (size_t j = 0; j < tc->count; j++)
      {
        ASSERT_EQ(tc->name, tc->params[j].type, task->params[j].type);

        if (tc->params[j].type == DB_PARAM_STRING)
        {
          size_t len = tc->params[j].value.string.len;

          ASSERT_EQ(tc->name, len, task->params[j].value.string.len);
          ASSERT_TRUE(tc->name, task->params[j].value.string.ptr != tc->params[j].value.string.ptr);

          if (len)
          {
            ASSERT_EQ(
                tc->name,
                0,
                memcmp(tc->params[j].value.string.ptr, task->params[j].value.string.ptr, len));
          }
        }
        else if (tc->params[j].type == DB_PARAM_INT64)
        {
          ASSERT_EQ(tc->name, tc->params[j].value.integer, task->params[j].value.integer);
        }
      }

      free(task->result);
      free(task);
    }

    ASSERT_TRUE(tc->name, queue->head == NULL && queue->tail == NULL);
    task_queue_free(queue);

    CHECK_TEST(tc->name);
  }

  ctx->is_canceled = false;
  db_pool_t pool = {.task_queue = task_queue_new()};
  char query[] = "UPDATE t SET a=?";
  char value[] = "original";
  db_param_t param = {.type = DB_PARAM_STRING, .value.string = {.ptr = value, .len = sizeof(value) - 1}};

  ASSERT_EQ(
      "copied inputs", 0, db_exec_query_param(&pool, query, sizeof(query) - 1, &param, 1, ctx));
  memset(query, 'x', sizeof(query));
  memset(value, 'x', sizeof(value));
  param = (db_param_t){.type = DB_PARAM_NULL};
  db_task_t *task = task_queue_pop(pool.task_queue);

  ASSERT_STR_EQ("copied inputs", "UPDATE t SET a=?", task->query);
  ASSERT_STR_EQ("copied inputs", "original", task->params[0].value.string.ptr);
  free(task->result);
  free(task);
  // Discarding a queued parameterized task must free its preallocated result too.
  ASSERT_EQ("copied inputs", 0, db_exec_query_param(&pool, "DELETE FROM t", 13, NULL, 0, NULL));
  task_queue_free(pool.task_queue);

  CHECK_TEST("copied inputs");
}

static void
test_db_worker_thread(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_db_worker_thread");

  char large_string[8192];
  memset(large_string, 'x', sizeof(large_string));
  const db_param_t params[] = {
      {.type = DB_PARAM_STRING, .value.string = {.ptr = "a\0b'\\", .len = 5}},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = NULL, .len = 0}},
      {.type = DB_PARAM_STRING, .value.string = {.ptr = large_string, .len = sizeof(large_string)}},
      {.type = DB_PARAM_INT64, .value.integer = INT64_MIN},
      {.type = DB_PARAM_INT64, .value.integer = INT64_MAX},
      {.type = DB_PARAM_NULL},
  };
  enum failure
  {
    NONE,
    INIT,
    PREPARE,
    BIND,
    EXECUTE,
    SELECT_ROWS,
    COUNT,
    ALLOCATION
  };
  struct test_case
  {
    const char *name;
    const char *query;
    enum failure failure;
    size_t count;
    bool legacy;
  } cases[] = {
      {
          .name = "INSERT",
          .query = "INSERT INTO t VALUES (?, ?, ?, ?, ?, ?)",
          .failure = NONE,
          .count = 6,
          .legacy = false,
      },
      {
          .name = "UPDATE",
          .query = "UPDATE t SET a=?, b=?, c=?, d=?, e=?, f=?",
          .failure = NONE,
          .count = 6,
          .legacy = false,
      },
      {
          .name = "DELETE",
          .query = "DELETE FROM t",
          .failure = NONE,
          .count = 0,
          .legacy = false,
      },
      {
          .name = "statement init failure",
          .query = "UPDATE t SET a=?",
          .failure = INIT,
          .count = 1,
          .legacy = false,
      },
      {
          .name = "prepare failure",
          .query = "UPDATE t SET a=?",
          .failure = PREPARE,
          .count = 1,
          .legacy = false,
      },
      {
          .name = "bind failure",
          .query = "UPDATE t SET a=?",
          .failure = BIND,
          .count = 1,
          .legacy = false,
      },
      {
          .name = "execute failure",
          .query = "UPDATE t SET a=?",
          .failure = EXECUTE,
          .count = 1,
          .legacy = false,
      },
      {
          .name = "SELECT rejected",
          .query = "SELECT a FROM t WHERE a=?",
          .failure = SELECT_ROWS,
          .count = 1,
          .legacy = false,
      },
      {
          .name = "parameter count mismatch",
          .query = "UPDATE t SET a=?",
          .failure = COUNT,
          .count = 1,
          .legacy = false,
      },
      {
          .name = "binding allocation failure",
          .query = "UPDATE t SET a=?",
          .failure = ALLOCATION,
          .count = 1,
          .legacy = false,
      },
      {
          .name = "legacy query",
          .query = "UPDATE t SET a=1",
          .failure = NONE,
          .count = 0,
          .legacy = true,
      },
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &cases[i];

    mock = (__typeof__(mock)){
        .init_fail = tc->failure == INIT,
        .prepare_fail = tc->failure == PREPARE,
        .bind_fail = tc->failure == BIND,
        .execute_fail = tc->failure == EXECUTE,
        .returns_rows = tc->failure == SELECT_ROWS,
        .valid_bindings = true,
        .param_count = tc->count + (tc->failure == COUNT),
        .query = tc->query,
        .params = params};

    db_pool_t pool = {
        .task_queue = task_queue_new(),
        .done_queue = task_queue_new(),
        .notify_fd = eventfd(0, EFD_NONBLOCK)};

    if (tc->legacy)
    {
      db_pool_exec_query(&pool, tc->query, strlen(tc->query), ctx);
    }
    else
    {
      ASSERT_EQ(tc->name,
                0,
                db_exec_query_param(&pool, tc->query, strlen(tc->query), params, tc->count, ctx));
    }

    pool.task_queue->stop = 1; // Drain the queued task and exit without a worker thread.
    pool.done_queue->stop = 1;
    fail_calloc_after = tc->failure == ALLOCATION ? 0 : -1;
    db_worker_thread(&pool);
    fail_calloc_after = -1;

    eventfd_t notifications = 0;

    ASSERT_EQ(tc->name, 0, eventfd_read(pool.notify_fd, &notifications));
    ASSERT_EQ(tc->name, (eventfd_t)1, notifications);

    db_task_t *task = db_pool_get_latest_completed_task(&pool);

    ASSERT_TRUE(tc->name, task != NULL);

    if (task)
    {
      ASSERT_TRUE(tc->name, task->data == ctx);
      ASSERT_EQ(tc->name, (int)(tc->failure == NONE), task->result->success);
      ASSERT_TRUE(tc->name, task->result->res == NULL);

      if (tc->failure == NONE)
      {
        ASSERT_EQ(tc->name, (uint64_t)(tc->legacy ? 7 : 3), task->result->affected);
        ASSERT_EQ(tc->name, (uint64_t)(tc->legacy ? 0 : 123), task->result->insert_id);
      }
      else
      {
        ASSERT_TRUE(tc->name, task->result->err_msg_len > 0);
        ASSERT_EQ(tc->name, strlen(task->result->err_msg), (size_t)task->result->err_msg_len);
        ASSERT_EQ(tc->name, (uint64_t)0, task->result->affected);
        ASSERT_EQ(tc->name, (uint64_t)0, task->result->insert_id);
      }

      free(task->result);
      free(task);
    }

    ASSERT_TRUE(tc->name, mock.valid_bindings);
    ASSERT_EQ(tc->name, (unsigned)(!tc->legacy && tc->failure != INIT), mock.close_calls);
    ASSERT_EQ(tc->name,
              (unsigned)(!tc->legacy && (tc->failure == NONE || tc->failure == EXECUTE)),
              mock.execute_calls);
    ASSERT_EQ(tc->name, (unsigned)tc->legacy, mock.query_calls);
    ASSERT_EQ(tc->name, (unsigned)(!tc->legacy && tc->failure != INIT), mock.prepare_calls);
    ASSERT_EQ(tc->name,
              (unsigned)(!tc->legacy && tc->count &&
                         (tc->failure == NONE || tc->failure == BIND || tc->failure == EXECUTE)),
              mock.bind_calls);

    close(pool.notify_fd);
    task_queue_free(pool.task_queue);
    task_queue_free(pool.done_queue);

    CHECK_TEST(tc->name);
  }
}

static void
test_task_queue_push(test_ctx_t *ctx)
{
  struct test_case
  {
    size_t count;
  } cases[] = {
      {.count = 1},
      {.count = 2},
      {.count = 5},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    db_pool_t pool = {.task_queue = task_queue_new()};

    for (size_t j = 0; j < cases[i].count; j++)
    {
      db_param_t param = {.type = DB_PARAM_INT64, .value.integer = j};

      ASSERT_EQ("FIFO", 0, db_exec_query_param(&pool, "UPDATE t SET a=?", 16, &param, 1, NULL));
    }

    pool.task_queue->stop = 1;

    for (size_t j = 0; j < cases[i].count; j++)
    {
      db_task_t *task = task_queue_pop(pool.task_queue);

      ASSERT_TRUE("FIFO", task != NULL);

      if (task)
      {
        ASSERT_EQ("FIFO", (int64_t)j, task->params[0].value.integer);

        free(task->result);
        free(task);
      }
    }

    ASSERT_TRUE("FIFO", pool.task_queue->head == NULL && pool.task_queue->tail == NULL);
    task_queue_free(pool.task_queue);

    CHECK_TEST("FIFO");
  }
}

static void
test_db_pool_exec_query(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *query;
    size_t length;
    bool stopped;
    int allocation_failure;
    bool success;
  } cases[] = {
      {.name = "ordinary query", .query = "SELECT 1", .length = 8, .allocation_failure = -1, .success = true},
      {.name = "empty query", .query = "", .allocation_failure = -1},
      {.name = "NULL query", .length = 8, .allocation_failure = -1},
      {.name = "oversized query", .query = "SELECT 1", .length = DEFAULT_QUERY_SIZE, .allocation_failure = -1},
      {.name = "embedded NUL", .query = "SELECT\0x", .length = 8, .allocation_failure = -1},
      {.name = "stopped queue", .query = "SELECT 1", .length = 8, .stopped = true, .allocation_failure = -1},
      {.name = "task allocation", .query = "SELECT 1", .length = 8, .allocation_failure = 0},
      {.name = "result allocation", .query = "SELECT 1", .length = 8, .allocation_failure = 1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    db_pool_t pool = {.task_queue = task_queue_new(), .done_queue = task_queue_new()};
    pool.task_queue->stop = cases[i].stopped;
    test_set_calloc_failure(cases[i].allocation_failure);
    int result = db_pool_exec_query(&pool, cases[i].query, cases[i].length, ctx);
    test_set_calloc_failure(-1);
    ASSERT_EQ(cases[i].name, cases[i].success ? 0 : -1, result);
    ASSERT_EQ(cases[i].name, cases[i].success, pool.task_queue->head != NULL);

    if (result == 0)
    {
      db_task_t *task = task_queue_pop(pool.task_queue);
      ASSERT_NOT_NULL(cases[i].name, task->result);
      ASSERT_STR_EQ(cases[i].name, cases[i].query, task->query);
      ASSERT_TRUE(cases[i].name, task->data == ctx);
      task_queue_push(pool.done_queue, task);
      ASSERT_TRUE(cases[i].name, db_pool_get_latest_completed_task(&pool) == task);
      db_task_free(task);
    }

    ASSERT_NULL(cases[i].name, db_pool_get_latest_completed_task(&pool));
    task_queue_free(pool.task_queue);
    task_queue_free(pool.done_queue);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_db_pool_stop(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    size_t tasks;
  } cases[] = {
      {.name = "stop idle worker", .tasks = 0},
      {.name = "join and drain worker", .tasks = 3},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    int epoll_fd = epoll_create1(0);
    mock = (__typeof__(mock)){.query = "SELECT 1", .valid_bindings = true};
    db_pool_t *pool = db_pool_new((db_option_t){0}, epoll_fd, 1);
    ASSERT_NOT_NULL(cases[i].name, pool);

    if (pool)
    {
      for (size_t j = 0; j < cases[i].tasks; j++)
      {
        ASSERT_EQ(cases[i].name, 0, db_pool_exec_query(pool, "SELECT 1", 8, NULL));
      }

      db_pool_stop(pool);
      ASSERT_EQ(cases[i].name, 0, pool->num_threads);
      ASSERT_EQ(cases[i].name, -1, db_pool_exec_query(pool, "SELECT 1", 8, NULL));
      ASSERT_EQ(cases[i].name, -1, db_exec_query_param(pool, "DELETE FROM t", 13, NULL, 0, NULL));

      for (size_t j = 0; j < cases[i].tasks; j++)
      {
        db_task_t *task = db_pool_get_latest_completed_task(pool);
        ASSERT_NOT_NULL(cases[i].name, task);

        if (task)
        {
          ASSERT_EQ(cases[i].name, 1, task->result->success);
          db_task_free(task);
        }
      }

      ASSERT_NULL(cases[i].name, db_pool_get_latest_completed_task(pool));
      ASSERT_EQ(cases[i].name, (unsigned)cases[i].tasks, mock.query_calls);
      db_pool_free(pool);
    }

    close(epoll_fd);
    CHECK_TEST(cases[i].name);
  }
}

void
test_db(test_ctx_t *ctx)
{
  test_db_pool_exec_query(ctx);
  test_db_pool_stop(ctx);
  test_db_exec_query_param(ctx);
  test_db_worker_thread(ctx);
  test_task_queue_push(ctx);
}
