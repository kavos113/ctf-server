#ifndef MYSQL_H
#define MYSQL_H

#define DEFAULT_QUERY_SIZE 256

#include <mysql/mysql.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
  int success;
  MYSQL_RES *res;
  uint64_t affected;
  uint64_t insert_id;

  char err_msg[256];
  int err_msg_len;
} db_result_t;

typedef enum
{
  DB_PARAM_STRING,
  DB_PARAM_INT64,
  DB_PARAM_NULL,
} db_param_type;

typedef struct
{
  db_param_type type;
  union
  {
    struct
    {
      const char *ptr;
      size_t len;
    } string;
    int64_t integer;
  } value;
} db_param_t;

typedef struct db_task
{
  char query[DEFAULT_QUERY_SIZE];

  db_result_t *result;

  // commonly used for http_request_context_t*
  void *data;

  struct db_task *next;

  int is_param_query;
  size_t query_len;
  size_t param_count;
  db_param_t params[];
} db_task_t;

typedef struct
{
  db_task_t *head;
  db_task_t *tail;

  pthread_mutex_t mtx;
  pthread_cond_t cond;
  int stop;
} task_queue_t;

typedef struct
{
  const char *host;
  int port;
  const char *user;
  const char *pass;
  const char *db;
} db_option_t;

struct db_pool_t
{
  int epoll_fd;
  int notify_fd;
  task_queue_t *task_queue; // for Worker
  task_queue_t *done_queue; // for Main

  int num_threads;
  pthread_t *threads;

  db_option_t db_options;
  struct connection *notification;
};
typedef struct db_pool_t db_pool_t;

task_queue_t *task_queue_new();
void task_queue_free(task_queue_t *queue);
void task_queue_push(task_queue_t *queue, db_task_t *task);
db_task_t *task_queue_pop(task_queue_t *queue);

// arg: db_pool_t
void *db_worker_thread(void *arg);

db_pool_t *db_pool_new(db_option_t option, int epoll_fd, int num_threads);
db_pool_t *db_pool_new_from_env(int epoll_fd, int num_threads);
// Main-thread shutdown: stop accepting work and join workers. Completed tasks
// remain available so the caller can release task->data before freeing the pool.
void db_pool_stop(db_pool_t *pool);
void db_pool_free(db_pool_t *pool);
void db_task_free(db_task_t *task);

// Submission returns -1 on failure; no completion notification will follow.
int db_pool_exec_query(db_pool_t *pool, const char *query, size_t query_len, void *data);
int db_exec_query_param(db_pool_t *pool,
                        const char *query,
                        size_t query_len,
                        const db_param_t *params,
                        size_t param_count,
                        void *data);

// Nonblocking: NULL when no completion is currently queued.
db_task_t *db_pool_get_latest_completed_task(db_pool_t *pool);

#endif // MYSQL_H