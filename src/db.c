#define _POSIX_C_SOURCE 200809L // for strdup

#include "db.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "server.h"

task_queue_t *
task_queue_new()
{
  task_queue_t *q = malloc(sizeof(task_queue_t));

  q->head = q->tail = NULL;
  pthread_mutex_init(&q->mtx, NULL);
  pthread_cond_init(&q->cond, NULL);
  q->stop = 0;

  return q;
}

void
task_queue_free(task_queue_t *queue)
{
  while (queue->head)
  {
    db_task_t *top = task_queue_pop(queue);
    if (top->is_param_query)
    {
      free(top->result);
    }
    free(top);
  }

  free(queue);
}

void
task_queue_push(task_queue_t *queue, db_task_t *task)
{
  pthread_mutex_lock(&queue->mtx);

  task->next = NULL;
  if (queue->tail)
  {
    queue->tail->next = task;
    queue->tail = task;
  }
  else
  {
    queue->head = queue->tail = task;
  }

  pthread_cond_signal(&queue->cond);
  pthread_mutex_unlock(&queue->mtx);
}

db_task_t *
task_queue_pop(task_queue_t *queue)
{
  pthread_mutex_lock(&queue->mtx);

  // キューに何かタスクが来るまで待つ
  while (!queue->head && !queue->stop)
  {
    pthread_cond_wait(&queue->cond, &queue->mtx);
  }
  if (queue->stop && !queue->head)
  {
    pthread_mutex_unlock(&queue->mtx);
    return NULL;
  }

  db_task_t *task = queue->head;
  queue->head = task->next;

  if (!queue->head)
  {
    queue->tail = NULL;
  }

  pthread_mutex_unlock(&queue->mtx);
  return task;
}

static void
set_param_query_error(db_result_t *result, const char *message)
{
  snprintf(result->err_msg, sizeof(result->err_msg), "%s", message);
  result->err_msg_len = strlen(result->err_msg);
}

static void
execute_param_query(MYSQL *conn, db_task_t *task)
{
  db_result_t *result = task->result;
  MYSQL_STMT *stmt = mysql_stmt_init(conn);
  if (!stmt)
  {
    set_param_query_error(result, "cannot allocate statement");
    return;
  }

  MYSQL_BIND *bindings = NULL;
  if (mysql_stmt_prepare(stmt, task->query, task->query_len) != 0)
  {
    set_param_query_error(result, mysql_stmt_error(stmt));
    goto done;
  }

  if (mysql_stmt_field_count(stmt) != 0)
  {
    set_param_query_error(result, "queries returning rows are not supported");
    goto done;
  }

  if (mysql_stmt_param_count(stmt) != task->param_count)
  {
    set_param_query_error(result, "parameter count mismatch");
    goto done;
  }

  if (task->param_count)
  {
    bindings = calloc(task->param_count, sizeof(*bindings));
    if (!bindings)
    {
      set_param_query_error(result, "cannot allocate parameter bindings");
      goto done;
    }

    for (size_t i = 0; i < task->param_count; i++)
    {
      db_param_t *param = &task->params[i];
      switch (param->type)
      {
      case DB_PARAM_STRING:
        bindings[i].buffer_type = MYSQL_TYPE_STRING;
        bindings[i].buffer = (void *)param->value.string.ptr;
        bindings[i].buffer_length = param->value.string.len;
        bindings[i].length = &bindings[i].buffer_length;
        break;

      case DB_PARAM_INT64:
        bindings[i].buffer_type = MYSQL_TYPE_LONGLONG;
        bindings[i].buffer = &param->value.integer;
        bindings[i].buffer_length = sizeof(param->value.integer);
        break;

      case DB_PARAM_NULL:
        bindings[i].buffer_type = MYSQL_TYPE_NULL;
        break;
      }
    }
    if (mysql_stmt_bind_param(stmt, bindings) != 0)
    {
      set_param_query_error(result, mysql_stmt_error(stmt));
      goto done;
    }
  }

  if (mysql_stmt_execute(stmt) != 0)
  {
    set_param_query_error(result, mysql_stmt_error(stmt));
    goto done;
  }

  result->success = 1;
  result->affected = mysql_stmt_affected_rows(stmt);
  result->insert_id = mysql_stmt_insert_id(stmt);

done:
  mysql_stmt_close(stmt);
  free(bindings);
}

void *
db_worker_thread(void *arg)
{
  db_pool_t *pool = (db_pool_t *)arg;

  MYSQL *conn = mysql_init(NULL);
  if (!mysql_real_connect(
          conn,
          pool->db_options.host,
          pool->db_options.user,
          pool->db_options.pass,
          pool->db_options.db,
          pool->db_options.port,
          NULL, 0))
  {
    fprintf(stderr, "[MYSQL] mysql connect error: %s\n", mysql_error(conn));
    return NULL;
  }

  fprintf(stderr, "[MYSQL] connected to db successfully. \n");

  while (1)
  {
    db_task_t *task = task_queue_pop(pool->task_queue);
    if (!task)
    {
      break;
    }

    if (task->is_param_query)
    {
      execute_param_query(conn, task);
    }
    else
    {
      int err = mysql_query(conn, task->query);
      task->result = calloc(1, sizeof(db_result_t));
      if (err == 0)
      {
        MYSQL_RES *res = mysql_store_result(conn);
        task->result->res = res;
        task->result->success = 1;
      }
      else
      {
        int len = snprintf(task->result->err_msg, sizeof(task->result->err_msg), "error: %s", mysql_error(conn));
        task->result->err_msg_len = len;
        task->result->success = 0;
      }

      my_ulonglong affected = mysql_affected_rows(conn);
      task->result->affected = affected;
    }

    task_queue_push(pool->done_queue, task);

    eventfd_t val = 1;
    eventfd_write(pool->notify_fd, val);
  }

  mysql_close(conn);
  mysql_thread_end();

  return NULL;
}

db_pool_t *
db_pool_new(db_option_t option, int epoll_fd, int num_threads)
{
  db_pool_t *pool = malloc(sizeof(db_pool_t));

  pool->epoll_fd = epoll_fd;
  pool->num_threads = num_threads;
  pool->db_options = option;

  pool->task_queue = task_queue_new();
  pool->done_queue = task_queue_new();

  pool->notify_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

  connection_t *conn = malloc(sizeof(connection_t));
  conn->fd = pool->notify_fd;
  conn->type = FD_TYPE_DB;

  struct epoll_event event;
  event.events = EPOLLIN | EPOLLET;
  event.data.ptr = conn;

  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pool->notify_fd, &event);

  mysql_library_init(0, NULL, NULL);

  pool->threads = malloc(sizeof(pthread_t) * num_threads);
  for (int i = 0; i < num_threads; i++)
  {
    pthread_create(&pool->threads[i], NULL, db_worker_thread, pool);
  }

  return pool;
}

db_pool_t *
db_pool_new_from_env(int epoll_fd, int num_threads)
{
  const char *host = getenv("MARIADB_HOST");
  if (!host)
  {
    fprintf(stderr, "[DB FAIL] MARIADB_HOST is null\n");
    return NULL;
  }

  const char *user = getenv("MARIADB_USER");
  if (!user)
  {
    fprintf(stderr, "[DB FAIL] MARIADB_USERNAME is null\n");
    return NULL;
  }

  const char *pass = getenv("MARIADB_PASS");
  if (!pass)
  {
    fprintf(stderr, "[DB_FAIL] MARIADB_PASS is null\n");
    return NULL;
  }

  const char *db = getenv("MARIADB_DB");
  if (!db)
  {
    fprintf(stderr, "[DB FAIL] MARIADB_DB is null\n");
    return NULL;
  }

  const char *portstr = getenv("MARIADB_PORT");
  if (!portstr)
  {
    fprintf(stderr, "[DB FAIL] MARIADB_PORT is null\n");
    return NULL;
  }

  int port = (int)strtol(portstr, NULL, 10);

  db_option_t option = {
      .host = host,
      .port = port,
      .user = user,
      .pass = pass,
      .db = db,
  };

  return db_pool_new(option, epoll_fd, num_threads);
}

void
db_pool_free(db_pool_t *pool)
{
  task_queue_free(pool->task_queue);
  task_queue_free(pool->done_queue);

  free(pool);
}

void
db_pool_exec_query(db_pool_t *pool, const char *query, size_t query_len, void *data)
{
  db_task_t *task = calloc(1, sizeof(db_task_t));

  if (query_len >= DEFAULT_QUERY_SIZE)
  {
    return;
  }

  memcpy(task->query, query, query_len);
  task->data = data;

  task_queue_push(pool->task_queue, task);
}

int
db_exec_query_param(db_pool_t *pool, const char *query, size_t query_len,
                    const db_param_t *params, size_t param_count, void *data)
{
  if (!pool || !pool->task_queue || !query || query_len == 0 || query_len >= DEFAULT_QUERY_SIZE || memchr(query, '\0', query_len) || (param_count && !params))
  {
    return -1;
  }
  size_t size = sizeof(db_task_t) + param_count * sizeof(db_param_t);
  for (size_t i = 0; i < param_count; i++)
  {
    switch (params[i].type)
    {
    case DB_PARAM_STRING:
    {
      size_t len = params[i].value.string.len;
      if ((!params[i].value.string.ptr && len) || len > ULONG_MAX || len >= SIZE_MAX - size)
      {
        return -1;
      }
      size += len + 1;
      break;
    }

    case DB_PARAM_INT64:
    case DB_PARAM_NULL:
      break;
    default:
      return -1;
    }
  }

  db_task_t *task = calloc(1, size);
  if (!task)
  {
    return -1;
  }

  // Allocate the result before enqueueing so allocation failure is synchronous.
  task->result = calloc(1, sizeof(db_result_t));
  if (!task->result)
  {
    free(task);
    return -1;
  }

  memcpy(task->query, query, query_len);
  task->query_len = query_len;
  task->is_param_query = 1;
  task->param_count = param_count;
  task->data = data;

  char *strings = (char *)(task->params + param_count);
  for (size_t i = 0; i < param_count; i++)
  {
    task->params[i] = params[i];
    if (params[i].type == DB_PARAM_STRING)
    {
      size_t len = params[i].value.string.len;
      if (len)
      {
        memcpy(strings, params[i].value.string.ptr, len);
      }
      strings[len] = '\0';
      task->params[i].value.string.ptr = strings;
      strings += len + 1;
    }
  }
  
  task_queue_push(pool->task_queue, task);
  return 0;
}

db_task_t *
db_pool_get_latest_completed_task(db_pool_t *pool)
{
  return task_queue_pop(pool->done_queue);
}