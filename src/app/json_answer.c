#include "json.h"
#include "json_p.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool
read_answer_field(json_parser_t *parser, string_t key, unsigned depth, void *context)
{
  submit_answer_request_t *request = context;

  if (string_equals_cstr(key, "challenge_id"))
  {
    return read_positive_integer(parser, &request->challenge_id);
  }

  if (string_equals_cstr(key, "answer"))
  {
    return read_string(parser, &request->answer);
  }

  return read_value(parser, depth);
}

int
json_to_submit_answer_request(const char *json, size_t len, submit_answer_request_t *request)
{
  if (!request)
  {
    return -1;
  }

  *request = (submit_answer_request_t){0};

  if (!json || !len)
  {
    return -1;
  }

  json_parser_t parser = {.cur = json, .end = json + len, .error = -1};
  submit_answer_request_t parsed = {0};

  if (!read_object(&parser, 0, read_answer_field, &parsed) ||
      skip_whitespace(parser.cur, parser.end) != parser.end || parsed.challenge_id == 0 ||
      parsed.answer.len == 0)
  {
    return parser.error;
  }

  size_t characters = 0;

  for (size_t i = 0; i < parsed.answer.len; i++)
  {
    if (((unsigned char)parsed.answer.ptr[i] & 0xc0) != 0x80)
    {
      characters++;
    }
  }

  if (characters > 255 || json_string_equal_decoded(parsed.answer, parsed.answer) < 0)
  {
    return -1;
  }

  *request = parsed;
  return 0;
}

static int
format_answer(char *buffer, size_t size, const answer_t *answer, bool public_view)
{
  if (public_view)
  {
    return snprintf(buffer,
                    size,
                    "{\"challenge_id\":%d,\"user_id\":\"%.*s\",\"answered_at\":\"%.*s\"}",
                    answer->challenge_id,
                    (int)answer->user_id.len,
                    answer->user_id.ptr,
                    (int)answer->created_at.len,
                    answer->created_at.ptr);
  }

  return snprintf(buffer,
                  size,
                  "{\"challenge_id\":%d,\"answer\":\"%.*s\",\"correct\":%s,"
                  "\"user_id\":\"%.*s\",\"answered_at\":\"%.*s\"}",
                  answer->challenge_id,
                  (int)answer->answer.len,
                  answer->answer.ptr,
                  answer->is_corrected ? "true" : "false",
                  (int)answer->user_id.len,
                  answer->user_id.ptr,
                  (int)answer->created_at.len,
                  answer->created_at.ptr);
}

void
answer_to_json(const answer_t *answer, string_t *json)
{
  *json = (string_t){0};
  int len = format_answer(NULL, 0, answer, false);

  if (len < 0)
  {
    return;
  }

  char *buffer = malloc((size_t)len + 1);

  if (!buffer)
  {
    return;
  }

  format_answer(buffer, (size_t)len + 1, answer, false);
  *json = (string_t){.ptr = buffer, .len = (size_t)len};
}

void
answers_to_json(const answer_t *answers, size_t count, string_t *json, bool public_view)
{
  *json = (string_t){0};
  size_t size = 3; // Brackets and NUL.

  for (size_t i = 0; i < count; i++)
  {
    int len = format_answer(NULL, 0, &answers[i], public_view);

    if (len < 0 || size > SIZE_MAX - (size_t)len - (i != 0))
    {
      return;
    }

    size += (size_t)len + (i != 0);
  }

  char *buffer = malloc(size);

  if (!buffer)
  {
    return;
  }

  size_t offset = 0;
  buffer[offset++] = '[';

  for (size_t i = 0; i < count; i++)
  {
    if (i)
    {
      buffer[offset++] = ',';
    }

    offset += (size_t)format_answer(buffer + offset, size - offset, &answers[i], public_view);
  }

  buffer[offset++] = ']';
  buffer[offset] = '\0';
  *json = (string_t){.ptr = buffer, .len = offset};
}
