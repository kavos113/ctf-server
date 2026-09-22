#define _POSIX_C_SOURCE 200809L

#include "password_worker.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

struct password_worker
{
  pthread_mutex_t mutex;
  pthread_cond_t ready;
  pthread_t thread;

  bool started;
  bool stopped;
  
  password_job_t *pending_head;
  password_job_t *pending_tail;
  size_t waiting;
  password_job_t *done_head;
  password_job_t *done_tail;
  
  int epoll_fd;
  connection_t notification;
  
  char dummy_hash[crypto_pwhash_STRBYTES];
};

static void
notify_completion(password_worker_t *worker)
{
  while (eventfd_write(worker->notification.fd, 1) < 0 && errno == EINTR)
  {
  }
}

// Caller holds the mutex.
static void
append_done(password_worker_t *worker, password_job_t *job)
{
  job->next = NULL;

  if (worker->done_tail)
  {
    worker->done_tail->next = job;
  }
  else
  {
    worker->done_head = job;
  }

  worker->done_tail = job;
}

static void
clear_input(password_job_t *job)
{
  sodium_memzero(job->password, sizeof(job->password));
  job->password_length = 0;
  sodium_memzero(job->stored_hash, sizeof(job->stored_hash));
}

static void *
run_password_worker(void *data)
{
  password_worker_t *worker = data;

  while (true)
  {
    pthread_mutex_lock(&worker->mutex);

    while (!worker->pending_head && !worker->stopped)
    {
      pthread_cond_wait(&worker->ready, &worker->mutex);
    }

    if (worker->stopped)
    {
      pthread_mutex_unlock(&worker->mutex);
      return NULL;
    }

    password_job_t *job = worker->pending_head;
    worker->pending_head = job->next;
    worker->waiting--;

    if (!worker->pending_head)
    {
      worker->pending_tail = NULL;
    }

    pthread_mutex_unlock(&worker->mutex);
    string_t password = {.ptr = job->password, .len = job->password_length};

    if (job->operation == PASSWORD_HASH)
    {
      job->result.status = auth_password_hash(password, job->result.hash);
    }
    else
    {
      job->result.status = auth_password_verify(NULL, job->stored_hash, password);

      if (job->unknown_user && job->result.status == AUTH_OK)
      {
        job->result.status = AUTH_INVALID;
      }
    }

    clear_input(job);
    pthread_mutex_lock(&worker->mutex);
    append_done(worker, job);
    pthread_mutex_unlock(&worker->mutex);
    notify_completion(worker);
  }
}

password_worker_t *
password_worker_new(const auth_config_t *config, int epoll_fd)
{
  if (!config || !config->dummy_hash[0] ||
      strnlen(config->dummy_hash, sizeof(config->dummy_hash)) >= sizeof(config->dummy_hash))
  {
    return NULL;
  }

  password_worker_t *worker = calloc(1, sizeof(*worker));

  if (!worker)
  {
    return NULL;
  }

  worker->epoll_fd = epoll_fd;
  worker->notification.fd = -1;
  worker->notification.type = FD_TYPE_PASSWORD;
  memcpy(worker->dummy_hash, config->dummy_hash, sizeof(worker->dummy_hash));

  if (pthread_mutex_init(&worker->mutex, NULL) != 0)
  {
    sodium_memzero(worker, sizeof(*worker));
    free(worker);
    return NULL;
  }

  if (pthread_cond_init(&worker->ready, NULL) != 0)
  {
    pthread_mutex_destroy(&worker->mutex);
    sodium_memzero(worker, sizeof(*worker));
    free(worker);
    return NULL;
  }

  worker->notification.fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

  if (worker->notification.fd < 0)
  {
    goto error;
  }

  struct epoll_event event = {.events = EPOLLIN | EPOLLET, .data.ptr = &worker->notification};

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, worker->notification.fd, &event) < 0 ||
      pthread_create(&worker->thread, NULL, run_password_worker, worker) != 0)
  {
    goto error;
  }

  worker->started = true;
  return worker;

error:
  password_worker_free(worker);
  return NULL;
}

static void
consume_password(string_t *password)
{
  if (password)
  {
    if (password->ptr)
    {
      sodium_memzero(password->ptr, password->len);
      free(password->ptr);
    }

    *password = (string_t){0};
  }
}

password_submit_result
password_worker_submit(password_worker_t *worker, password_operation operation,
                       string_t *password, const char *stored_hash, void *data)
{
  if (!worker || !password || !password->ptr || password->len < 8 || password->len > 128 ||
      memchr(password->ptr, '\0', password->len) ||
      (operation != PASSWORD_HASH && operation != PASSWORD_VERIFY))
  {
    consume_password(password);
    return PASSWORD_INVALID_INPUT;
  }

  const char *hash = stored_hash ? stored_hash : worker->dummy_hash;

  if (operation == PASSWORD_VERIFY &&
      (!hash[0] || strnlen(hash, crypto_pwhash_STRBYTES) >= crypto_pwhash_STRBYTES))
  {
    consume_password(password);
    return PASSWORD_INVALID_INPUT;
  }

  pthread_mutex_lock(&worker->mutex);
  password_submit_result result = PASSWORD_QUEUED;

  if (worker->stopped)
  {
    result = PASSWORD_WORKER_STOPPED;
  }
  else if (worker->waiting == PASSWORD_QUEUE_CAPACITY)
  {
    result = PASSWORD_QUEUE_FULL;
  }

  if (result != PASSWORD_QUEUED)
  {
    pthread_mutex_unlock(&worker->mutex);
    consume_password(password);
    return result;
  }

  password_job_t *job = calloc(1, sizeof(*job));

  if (!job)
  {
    pthread_mutex_unlock(&worker->mutex);
    consume_password(password);
    return PASSWORD_SUBMIT_ERROR;
  }

  job->operation = operation;
  job->data = data;
  job->result.status = AUTH_ERROR;
  memcpy(job->password, password->ptr, password->len);
  job->password_length = password->len;

  if (operation == PASSWORD_VERIFY)
  {
    memcpy(job->stored_hash, hash, strlen(hash) + 1);
    job->unknown_user = stored_hash == NULL;
  }

  consume_password(password);

  if (worker->pending_tail)
  {
    worker->pending_tail->next = job;
  }
  else
  {
    worker->pending_head = job;
  }

  worker->pending_tail = job;
  worker->waiting++;
  pthread_cond_signal(&worker->ready);
  pthread_mutex_unlock(&worker->mutex);
  return PASSWORD_QUEUED;
}

void
password_worker_stop(password_worker_t *worker)
{
  if (!worker)
  {
    return;
  }

  pthread_mutex_lock(&worker->mutex);
  worker->stopped = true;
  bool cancelled = worker->pending_head != NULL;

  while (worker->pending_head)
  {
    password_job_t *job = worker->pending_head;
    worker->pending_head = job->next;
    clear_input(job);
    job->result.cancelled = true;
    append_done(worker, job);
  }

  worker->pending_tail = NULL;
  worker->waiting = 0;
  pthread_cond_broadcast(&worker->ready);
  pthread_mutex_unlock(&worker->mutex);

  if (cancelled)
  {
    notify_completion(worker);
  }

  if (worker->started)
  {
    pthread_join(worker->thread, NULL);
    worker->started = false;
  }
}

password_job_t *
password_worker_pop(password_worker_t *worker)
{
  pthread_mutex_lock(&worker->mutex);
  password_job_t *job = worker->done_head;

  if (job)
  {
    worker->done_head = job->next;
    job->next = NULL;

    if (!worker->done_head)
    {
      worker->done_tail = NULL;
    }
  }

  pthread_mutex_unlock(&worker->mutex);
  return job;
}

void
password_job_free(password_job_t *job)
{
  if (job)
  {
    sodium_memzero(job, sizeof(*job));
    free(job);
  }
}

int
password_worker_notify_fd(const password_worker_t *worker)
{
  return worker->notification.fd;
}

void
password_worker_free(password_worker_t *worker)
{
  if (!worker)
  {
    return;
  }

  password_worker_stop(worker);
  password_job_t *job;

  while ((job = password_worker_pop(worker)))
  {
    password_job_free(job);
  }

  if (worker->notification.fd >= 0)
  {
    epoll_ctl(worker->epoll_fd, EPOLL_CTL_DEL, worker->notification.fd, NULL);
    close(worker->notification.fd);
  }

  pthread_cond_destroy(&worker->ready);
  pthread_mutex_destroy(&worker->mutex);
  sodium_memzero(worker, sizeof(*worker));
  free(worker);
}
