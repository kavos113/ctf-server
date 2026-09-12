#include "model.h"

#include <string.h>

ctf_genre
ctf_genre_from_string(const char *str)
{
  if (strcmp(str, "web") == 0)
  {
    return CTF_GENRE_WEB;
  }
  else if (strcmp(str, "crypto") == 0)
  {
    return CTF_GENRE_CRYPTO;
  }
  else if (strcmp(str, "pwn") == 0)
  {
    return CTF_GENRE_PWN;
  }
  else if (strcmp(str, "rev") == 0)
  {
    return CTF_GENRE_REV;
  }
  else if (strcmp(str, "forensics") == 0)
  {
    return CTF_GENRE_FORENSICS;
  }
  else if (strcmp(str, "osint") == 0)
  {
    return CTF_GENRE_OSINT;
  }
  else
  {
    return CTF_GENRE_MISC;
  }
}

const char *
ctf_genre_to_string(ctf_genre genre)
{
  switch (genre)
  {
  case CTF_GENRE_WEB:
    return "web";
  case CTF_GENRE_CRYPTO:
    return "crypto";
  case CTF_GENRE_PWN:
    return "pwn";
  case CTF_GENRE_REV:
    return "rev";
  case CTF_GENRE_FORENSICS:
    return "forensics";
  case CTF_GENRE_OSINT:
    return "osint";
  case CTF_GENRE_MISC:
  default:
    return "misc";
  }
}