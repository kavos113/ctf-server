// Values serialized by src/app/model.c (ctf_genre_to_string).
export const Genre = {
  Web: 'web',
  Crypto: 'crypto',
  Pwn: 'pwn',
  Rev: 'rev',
  Forensics: 'forensics',
  Osint: 'osint',
  Misc: 'misc'
} as const;

export type Genre = (typeof Genre)[keyof typeof Genre];

export const genres = Object.values(Genre);
