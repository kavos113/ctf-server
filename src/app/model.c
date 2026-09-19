#include "model.h"

#include <string.h>

// TODO: byte compare
ctf_genre
ctf_genre_from_string(string_t str)
{
  if (str.len == 3 && strncmp(str.ptr, "web", 3) == 0)
  {
    return CTF_GENRE_WEB;
  }
  else if (str.len == 6 && strncmp(str.ptr, "crypto", 6) == 0)
  {
    return CTF_GENRE_CRYPTO;
  }
  else if (str.len == 3 && strncmp(str.ptr, "pwn", 3) == 0)
  {
    return CTF_GENRE_PWN;
  }
  else if (str.len == 3 && strncmp(str.ptr, "rev", 3) == 0)
  {
    return CTF_GENRE_REV;
  }
  else if (str.len == 8 && strncmp(str.ptr, "forensics", 8) == 0)
  {
    return CTF_GENRE_FORENSICS;
  }
  else if (str.len == 5 && strncmp(str.ptr, "osint", 5) == 0)
  {
    return CTF_GENRE_OSINT;
  }
  else
  {
    return CTF_GENRE_MISC;
  }
}

string_t
ctf_genre_to_string(ctf_genre genre)
{
  switch (genre)
  {
  case CTF_GENRE_WEB:
    return (string_t){"web", 3};
  case CTF_GENRE_CRYPTO:
    return (string_t){"crypto", 6};
  case CTF_GENRE_PWN:
    return (string_t){"pwn", 3};
  case CTF_GENRE_REV:
    return (string_t){"rev", 3};
  case CTF_GENRE_FORENSICS:
    return (string_t){"forensics", 8};
  case CTF_GENRE_OSINT:
    return (string_t){"osint", 5};
  default:
    return (string_t){"misc", 4};
  }
}