#ifndef APP_MODEL_H
#define APP_MODEL_H

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
} challenge_t;

typedef struct
{
  int id;
  int challenge_id;
  string_t user_id;
  string_t answer;
  int is_corrected;
  string_t created_at;
} answer_t;

#endif // APP_MODEL_H