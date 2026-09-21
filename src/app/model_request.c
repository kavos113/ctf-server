#include "model_request.h"

void
free_create_challenge_request(create_challenge_request_t *request)
{
  if (!request)
  {
    return;
  }

  if (request->is_string_allocated)
  {
    free(request->name.ptr);
    free(request->description.ptr);
    free(request->flag.ptr);
  }

  free(request);
}

void
free_submit_answer_request(submit_answer_request_t *request)
{
  if (!request)
  {
    return;
  }

  if (request->is_string_allocated)
  {
    free(request->answer.ptr);
  }

  free(request);
}