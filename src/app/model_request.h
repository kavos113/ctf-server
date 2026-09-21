#ifndef APP_MODEL_REQUEST_H
#define APP_MODEL_REQUEST_H

#include "model.h"
#include "str.h"

typedef struct
{
  string_t name;
  string_t description;
  string_t flag;
  ctf_genre genre;
} create_challenge_request_t;

typedef struct
{
  int challenge_id;
  string_t answer;
} submit_answer_request_t;

// string作ったほうがよいよね

#endif // APP_MODEL_REQUEST_H
