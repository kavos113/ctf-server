#ifndef APP_JSON_P_H
#define APP_JSON_P_H

#include "str.h"

#include <stdbool.h>

// 戻り値は終了位置のポインタ
const char *skip_whitespace(const char *str, const char *end);
const char *parse_json_int(const char *str, const char *end, int *out_value);
const char *skip_json_value(const char *str, const char *end);

typedef struct
{
  const char *cur;
  const char *end;
  int error; // Initialize to -1 (invalid input); -2 indicates allocation failure.
} json_parser_t;

// The callback consumes one field value. depth is the depth to pass to read_value.
typedef bool (*json_field_reader)(json_parser_t *parser, string_t key, unsigned depth, void *context);

// Returned string bytes are borrowed, still-escaped slices of the input.
bool read_string(json_parser_t *parser, string_t *slice);

// Consume one JSON value; the caller checks trailing input. Start at depth 0.
bool read_value(json_parser_t *parser, unsigned depth);

// With a NULL callback, validate and skip all fields. Otherwise call read_field
// for each field. JSON syntax and duplicate keys are checked by the parser.
bool read_object(json_parser_t *parser, unsigned depth, json_field_reader read_field, void *context);

#endif // APP_JSON_P_H
