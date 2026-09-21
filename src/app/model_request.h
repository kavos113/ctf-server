#ifndef APP_MODEL_REQUEST_H
#define APP_MODEL_REQUEST_H

#include <stdbool.h>

#include "model.h"
#include "str.h"

typedef struct
{
  string_t name;
  string_t description;
  string_t flag;
  ctf_genre genre;

  bool is_string_allocated;
} create_challenge_request_t;

typedef struct
{
  int challenge_id;
  string_t answer;

  bool is_string_allocated;
} submit_answer_request_t;

void free_create_challenge_request(create_challenge_request_t *request);
void free_submit_answer_request(submit_answer_request_t *request);

#endif // APP_MODEL_REQUEST_H
