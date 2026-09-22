#ifndef APP_AUTH_H
#define APP_AUTH_H

#include "model_auth.h"

#include <sodium.h>

#define AUTH_TOKEN_MAX  4096
#define AUTH_CLOCK_SKEW 30
#define AUTH_TTL_MAX    86400

typedef enum
{
  AUTH_OK = 0,
  AUTH_INVALID = -1,
  AUTH_ERROR = -2,
} auth_result;

typedef struct
{
  unsigned char key[64];
  size_t key_len;
  char issuer[129];
  char audience[129];
  int64_t ttl;
  char dummy_hash[crypto_pwhash_STRBYTES];
} auth_config_t;

typedef struct
{
  char user_id[AUTH_ID_LENGTH + 1];
  char session_id[AUTH_ID_LENGTH + 1];
  int64_t issued_at;
  int64_t not_before;
  int64_t expires_at;
} auth_claims_t;

auth_result auth_config_load(auth_config_t *config, const char *key_hex,
                             const char *issuer, const char *audience, const char *ttl);
auth_result auth_init_from_env(auth_config_t *config);
void auth_config_dispose(auth_config_t *config);

typedef int (*auth_random_fn)(void *context, unsigned char *out, size_t length);
auth_result auth_generate_id(char out[AUTH_ID_LENGTH + 1], auth_random_fn random, void *context);

auth_result auth_token_issue(const auth_config_t *config, string_t user_id, string_t session_id,
                             int64_t now, char **out_token);
auth_result auth_token_verify(const auth_config_t *config, string_t token, int64_t now,
                              auth_claims_t *out_claims);
void auth_token_free(char *token);

auth_result auth_password_hash(string_t password, char out[crypto_pwhash_STRBYTES]);
auth_result auth_password_verify(const auth_config_t *config, const char *stored_hash, string_t password);

#endif // APP_AUTH_H
