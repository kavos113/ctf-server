#include "mysql.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <mysql/mysql.h>

task_queue_t * task_queue_new()
{
  task_queue_t *q = malloc(sizeof(task_queue_t));

  q->head = q->tail = NULL;
  pthread_mutex_init(&q->mtx, NULL);
  pthread_cond_init(&q->cond, NULL);
  q->stop = 0;

  return q;
}

void task_queue_free(task_queue_t *queue)
{
  while (queue->head)
  {
    db_task_t *top = task_queue_pop(queue);
    free(top);
  }

  free(queue);
}

void task_queue_push(task_queue_t *queue, db_task_t *task)
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
    queue->head = queue->head = task;
  }

  pthread_cond_signal(&queue->cond);
  pthread_mutex_unlock(&queue->mtx);
}

db_task_t * task_queue_pop(task_queue_t *queue)
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

void * db_worker_thread(void *arg)
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
    NULL, 0
  ))
  {
    fprintf(stderr, "[MYSQL] mysql connect error: %s\n", mysql_error(conn));
    return NULL;
  }

  while (1)
  {
    db_task_t *task = task_queue_pop(pool->task_queue);
    if (!task)
    {
      break;
    }

    int err = mysql_query(conn, task->query);
    if (err == 0)
    {
      MYSQL_RES *res = mysql_store_result(conn);
      if (res)
      {
        MYSQL_ROW row = mysql_fetch_row(res);

        // TODO
        task->result_body = strdup(row[0]);
        task->result_len = strlen(row[0]);
        task->status_code = 200;

        mysql_free_result(res);
      }
      else
      {
        task->result_body = "affected: 0";
        task->result_len = strlen(task->result_body);
        task->status_code = 200;
      }
    }
    else
    {
      char buf[256];
      int len = snprintf(buf, sizeof(buf), "error: %s", mysql_error(conn));
      task->result_body = strdup(buf);
      task->result_len = len;
      task->status_code = 500;
    }

    task_queue_push(pool->done_queue, task);

    eventfd_t val = 1;
    eventfd_write(pool->notify_fd, val);
  }

  mysql_close(conn);
  mysql_thread_end();

  return NULL;
}

db_pool_t *db_pool_new(int epoll_fd, int num_threads)
{
  db_pool_t *pool = malloc(sizeof(db_pool_t));

  pool->epoll_fd = epoll_fd;
  pool->num_threads = num_threads;

  pool->task_queue = task_queue_new();
  pool->done_queue = task_queue_new();

  pool->notify_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

  struct epoll_event event;
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = pool->notify_fd;

  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pool->notify_fd, &event);

  mysql_library_init(0, NULL, NULL);

  pool->threads = malloc(sizeof(pthread_t) * num_threads);
  for (int i = 0; i < num_threads; i++)
  {
    pthread_create(&pool->threads[i], NULL, db_worker_thread, pool);
  }

  return pool;
}

void db_pool_free(db_pool_t *pool)
{
  task_queue_free(pool->task_queue);
  task_queue_free(pool->done_queue);

  free(pool);
}