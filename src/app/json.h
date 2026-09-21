#ifndef APP_JSON_H
#define APP_JSON_H

#include <stdbool.h>
#include <stddef.h>

#include "model.h"

int json_to_challenge(const char *json_str, size_t json_len, challenge_t *challenge);
void json_to_challenges(const char *json_str, size_t json_len, challenge_t **challenges, size_t *count);

void challenge_to_json(const challenge_t *challenge, string_t *json_str, bool only_size);
void challenges_to_json(const challenge_t *challenges, size_t count, string_t *json_str, bool only_size);

#endif // APP_JSON_H
