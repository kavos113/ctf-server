#ifndef APP_JSON_H
#define APP_JSON_H

#include <stdbool.h>
#include <stddef.h>

#include "model.h"
#include "model_request.h"

int json_to_create_challenge_request(const char *json, size_t len, create_challenge_request_t *request);

int json_to_challenge(const char *json_str, size_t json_len, challenge_t *challenge);
void json_to_challenges(const char *json_str, size_t json_len, challenge_t **challenges, size_t *count);

void challenge_to_json(const challenge_t *challenge, string_t *json_str, bool only_size);
void challenge_to_json_without_flag(const challenge_t *challenge, string_t *json_str, bool only_size);
void challenges_to_json(const challenge_t *challenges, size_t count, string_t *json_str, bool only_size);
void challenges_to_json_without_flag(const challenge_t *challenges, size_t count, string_t *json_str, bool only_size);

int json_to_submit_answer_request(const char *json, size_t len, submit_answer_request_t *request);
void answer_to_json(const answer_t *answer, string_t *json);
void answers_to_json(const answer_t *answers, size_t count, string_t *json, bool public_view);

#endif // APP_JSON_H
