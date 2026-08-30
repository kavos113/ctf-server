#ifndef MYSQL_H
#define MYSQL_H

#define DEFAULT_QUERY_SIZE 256

#include <pthread.h>
#include <stddef.h>

typedef struct db_task
{
  int fd;
  char query[DEFAULT_QUERY_SIZE];

  char *result_body;
  size_t result_len;
  int status_code;

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

typedef struct
{
  int epoll_fd;
  int notify_fd;
  task_queue_t *task_queue; // for Worker
  task_queue_t *done_queue; // for Main

  int num_threads;
  pthread_t *threads;

  db_option_t db_options;
} db_pool_t;

task_queue_t *task_queue_new();
void task_queue_free(task_queue_t *queue);
void task_queue_push(task_queue_t *queue, db_task_t *task);
db_task_t *task_queue_pop(task_queue_t *queue);

// arg: db_pool_t
void *db_worker_thread(void *arg);

db_pool_t *db_pool_new(int epoll_fd, int num_threads);
void db_pool_free(db_pool_t *pool);

void db_pool_exec_query(db_pool_t *pool);

#endif // MYSQL_H