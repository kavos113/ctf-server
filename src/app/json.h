#ifndef APP_JSON_H
#define APP_JSON_H

#include <stddef.h>

#include "model.h"

void json_to_challenge(const char *json_str, challenge_t *challenge);
void json_to_challenges(const char *json_str, challenge_t **challenges, size_t *count);

void challenge_to_json(const challenge_t *challenge, char **json_str);
void challenges_to_json(const challenge_t *challenges, size_t count, char **json_str);

#endif //APP_JSON_H
