#ifndef APP_PASSWORD_WORKER_H
#define APP_PASSWORD_WORKER_H

#include "auth.h"
#include "server.h"

#define PASSWORD_QUEUE_CAPACITY 16

typedef enum
{
  PASSWORD_HASH,
  PASSWORD_VERIFY,
} password_operation;

typedef enum
{
  PASSWORD_QUEUED,
  PASSWORD_QUEUE_FULL,
  PASSWORD_WORKER_STOPPED,
  PASSWORD_INVALID_INPUT,
  PASSWORD_SUBMIT_ERROR,
} password_submit_result;

typedef struct
{
  auth_result status;
  bool cancelled;
  char hash[crypto_pwhash_STRBYTES];
} password_result_t;

typedef struct password_job
{
  password_operation operation;
  password_result_t result;
  void *data; // Opaque to the worker; HTTP adapter uses http_request_context_t.
  struct password_job *next;
  char password[129];
  size_t password_length;
  char stored_hash[crypto_pwhash_STRBYTES];
  bool unknown_user;
} password_job_t;

typedef struct password_worker password_worker_t;

password_worker_t *password_worker_new(const auth_config_t *config, int epoll_fd);

password_submit_result password_worker_submit(password_worker_t *worker, password_operation operation,
                                              string_t *password, const char *stored_hash, void *data);

void password_worker_stop(password_worker_t *worker);
void password_worker_free(password_worker_t *worker);
int password_worker_notify_fd(const password_worker_t *worker);
password_job_t *password_worker_pop(password_worker_t *worker); // Nonblocking
void password_job_free(password_job_t *job);

void password_worker_handler(server_t *server, connection_t *connection);

#endif // APP_PASSWORD_WORKER_H
