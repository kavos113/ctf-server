#include "password_worker.h"

#include "http_server.h"
#include "server_p.h"

#include <errno.h>
#include <sys/eventfd.h>

void
password_worker_handler(server_t *server, connection_t *connection)
{
  password_worker_t *worker = server->password_worker;
  eventfd_t count;

  while (eventfd_read(password_worker_notify_fd(worker), &count) < 0 && errno == EINTR)
  {
  }

  password_job_t *job;

  while ((job = password_worker_pop(worker)))
  {
    http_request_context_t *ctx = job->data;

    if (!ctx->request->conn)
    {
      password_job_free(job);
      http_request_context_dispose(ctx);
      continue;
    }

    http_response_t response = {.status = HTTP_STATUS_SERVICE_UNAVAILABLE};
    bool complete = true;

    if (!job->result.cancelled)
    {
      ctx->worker_result = &job->result;
      complete = ctx->current_handler->func(ctx, server->db_pool, NULL, &response);
      ctx->worker_result = NULL;
    }

    password_job_free(job);

    if (complete)
    {
      start_send_http_response(server, ctx->request->conn, response);
      http_request_context_dispose(ctx);
    }
  }
}
