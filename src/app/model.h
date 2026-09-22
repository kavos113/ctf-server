#ifndef APP_MODEL_H
#define APP_MODEL_H

#include <stdbool.h>

#include "str.h"

typedef enum
{
  CTF_GENRE_WEB,
  CTF_GENRE_CRYPTO,
  CTF_GENRE_PWN,
  CTF_GENRE_REV,
  CTF_GENRE_FORENSICS,
  CTF_GENRE_OSINT,
  CTF_GENRE_MISC,
} ctf_genre;

ctf_genre ctf_genre_from_string(string_t str);
string_t ctf_genre_to_string(ctf_genre genre);

typedef struct
{
  int id;
  string_t creator_id;
  string_t name;
  string_t description;
  string_t flag;
  ctf_genre genre;

  bool is_string_allocated;
} challenge_t;

typedef struct
{
  int id;
  int challenge_id;
  string_t user_id;
  string_t username;
  string_t answer;
  int is_corrected;
  string_t created_at;

  bool is_string_allocated;
} answer_t;

typedef struct
{
  int challenge_id;
  string_t user_id;
  string_t answered_at;

  bool is_string_allocated;
} corrected_answer_t;

void free_challenge(challenge_t *challenge);
void free_challenges(challenge_t *challenges, size_t count);
void free_answer(answer_t *answer);
void free_answers(answer_t *answers, size_t count);
void free_corrected_answer(corrected_answer_t *corrected_answer);

#endif // APP_MODEL_H