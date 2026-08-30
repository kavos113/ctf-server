#ifndef MYSQL_H
#define MYSQL_H

#define DEFAULT_QUERY_SIZE 256

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <mysql/mysql.h>

typedef struct
{
  int success;
  MYSQL_RES *res;
  uint32_t affected;

  char err_msg[256];
  int err_msg_len;
} db_result_t;

typedef struct db_task
{
  char query[DEFAULT_QUERY_SIZE];

  db_result_t *result;

  // commonly used for http_request_context_t*
  void *data;

  struct db_task *next;
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
void db_pool_free(db_pool_t *pool);

void db_pool_exec_query(db_pool_t *pool, const char *query, size_t query_len, void *data);
db_task_t *db_pool_get_latest_completed_task(db_pool_t *pool);

#endif // MYSQL_H