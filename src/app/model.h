#ifndef APP_MODEL_H
#define APP_MODEL_H

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

typedef struct
{
  int id;
  const char *creator_id;
  const char *name;
  const char *description;
  const char *flag;
  ctf_genre genre;
} challenge_t;

typedef struct
{
  int id;
  int challenge_id;
  const char *user_id;
  const char *answer;
  int is_corrected;
  const char *created_at;
} answer_t;

#endif // APP_MODEL_H