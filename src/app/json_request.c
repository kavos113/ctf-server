#include "json.h"
#include "json_p.h"

typedef struct
{
  create_challenge_request_t request;
  unsigned fields;
} create_challenge_parser;

static bool
read_challenge_field(json_parser_t *parser, string_t key, unsigned depth, void *context)
{
  create_challenge_parser *parsed = context;
  string_t *field = NULL;
  string_t genre;
  unsigned bit = 0;

  if (string_equals_cstr(key, "name"))
  {
    field = &parsed->request.name;
    bit = 1;
  }
  else if (string_equals_cstr(key, "description"))
  {
    field = &parsed->request.description;
    bit = 2;
  }
  else if (string_equals_cstr(key, "flag"))
  {
    field = &parsed->request.flag;
    bit = 4;
  }
  else if (string_equals_cstr(key, "genre"))
  {
    field = &genre;
    bit = 8;
  }

  if (!field)
  {
    return read_value(parser, depth);
  }

  if (!read_string(parser, field))
  {
    return false;
  }

  parsed->fields |= bit;

  if (bit == 8)
  {
    parsed->request.genre = ctf_genre_from_string(genre);

    if (!string_equals(genre, ctf_genre_to_string(parsed->request.genre)))
    {
      return false;
    }
  }

  return true;
}

static size_t
stored_character_count(string_t value)
{
  size_t count = 0;

  for (size_t i = 0; i < value.len; i++)
  {
    if (((unsigned char)value.ptr[i] & 0xc0) != 0x80)
    {
      count++;
    }
  }

  return count;
}

int
json_to_create_challenge_request(const char *json, size_t len, create_challenge_request_t *request)
{
  if (!request)
  {
    return -1;
  }

  *request = (create_challenge_request_t){0};

  if (!json || !len)
  {
    return -1;
  }

  json_parser_t parser = {.cur = json, .end = json + len, .error = -1};
  create_challenge_parser parsed = {0};

  if (!read_object(&parser, 0, read_challenge_field, &parsed) ||
      skip_whitespace(parser.cur, parser.end) != parser.end ||
      parsed.fields != 15 ||
      parsed.request.name.len == 0 ||
      parsed.request.flag.len == 0 ||
      stored_character_count(parsed.request.name) > 255 ||
      stored_character_count(parsed.request.flag) > 255 ||
      parsed.request.description.len > 65535)
  {
    return parser.error;
  }

  *request = parsed.request;
  return 0;
}
