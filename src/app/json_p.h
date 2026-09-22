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

typedef bool (*json_field_reader)(json_parser_t *parser, string_t key, unsigned depth, void *context);

bool read_string(json_parser_t *parser, string_t *slice);
bool read_value(json_parser_t *parser, unsigned depth);
bool read_object(json_parser_t *parser, unsigned depth, json_field_reader read_field, void *context);
bool read_positive_integer(json_parser_t *parser, int *value);
int json_string_equal_decoded(string_t left, string_t right);

// Decode escaped contents into owned, NUL-terminated UTF-8 (which may include NUL).
// 0: success, -1: invalid input, -2: allocation failure. Release with free.
int json_string_decode(string_t slice, string_t *out);

#endif // APP_JSON_P_H
