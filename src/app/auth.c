#define _POSIX_C_SOURCE 200809L

#include "auth.h"

#include <errno.h>
#include <jansson.h>
#include <jwt.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_TIME_MAX INT64_C(253402300799)

static bool
valid_setting(const char *value)
{
  if (!value)
  {
    return false;
  }

  size_t length = strnlen(value, 129);

  if (!length || length > 128)
  {
    return false;
  }

  for (size_t i = 0; i < length; i++)
  {
    if (value[i] < '!' || value[i] > '~')
    {
      return false;
    }
  }

  return true;
}

void
auth_config_dispose(auth_config_t *config)
{
  if (config)
  {
    sodium_memzero(config, sizeof(*config));
  }
}

auth_result
auth_config_load(auth_config_t *config, const char *key_hex,
                 const char *issuer, const char *audience, const char *ttl)
{
  if (!config)
  {
    return AUTH_INVALID;
  }

  auth_config_dispose(config);

  if (!key_hex || !valid_setting(issuer) || !valid_setting(audience))
  {
    return AUTH_INVALID;
  }

  size_t length = strnlen(key_hex, 129);

  if (length < 64 || length > 128 || length % 2 != 0)
  {
    return AUTH_INVALID;
  }

  int64_t seconds = 3600;

  if (ttl)
  {
    seconds = 0;
    size_t ttl_length = strnlen(ttl, 6);

    if (!ttl_length || ttl_length > 5)
    {
      return AUTH_INVALID;
    }

    for (size_t i = 0; i < ttl_length; i++)
    {
      if (ttl[i] < '0' || ttl[i] > '9')
      {
        return AUTH_INVALID;
      }

      seconds = seconds * 10 + ttl[i] - '0';
    }

    if (seconds < 1 || seconds > AUTH_TTL_MAX)
    {
      return AUTH_INVALID;
    }
  }

  if (sodium_init() < 0)
  {
    return AUTH_ERROR;
  }

  if (sodium_hex2bin(config->key, sizeof(config->key), key_hex, length, NULL,
                     &config->key_len, NULL) != 0)
  {
    auth_config_dispose(config);
    return AUTH_INVALID;
  }

  memcpy(config->issuer, issuer, strlen(issuer) + 1);
  memcpy(config->audience, audience, strlen(audience) + 1);
  config->ttl = seconds;
  return AUTH_OK;
}

auth_result
auth_init_from_env(auth_config_t *config)
{
  auth_result result = auth_config_load(config, getenv("JWT_KEY_HEX"), getenv("JWT_ISSUER"),
                                        getenv("JWT_AUDIENCE"), getenv("JWT_TTL_SECONDS"));

  if (result != AUTH_OK)
  {
    return result;
  }

  unsigned char random[32];
  char password[65];
  randombytes_buf(random, sizeof(random));
  sodium_bin2hex(password, sizeof(password), random, sizeof(random));
  result = auth_password_hash((string_t){.ptr = password, .len = 64}, config->dummy_hash);
  sodium_memzero(random, sizeof(random));
  sodium_memzero(password, sizeof(password));

  if (result != AUTH_OK)
  {
    auth_config_dispose(config);
  }

  return result;
}

static bool
valid_config(const auth_config_t *config)
{
  return config && config->key_len >= 32 && config->key_len <= sizeof(config->key) &&
         valid_setting(config->issuer) && valid_setting(config->audience) &&
         config->ttl >= 1 && config->ttl <= AUTH_TTL_MAX;
}

static bool
valid_id(string_t id)
{
  if (!id.ptr || id.len != AUTH_ID_LENGTH)
  {
    return false;
  }

  for (size_t i = 0; i < id.len; i++)
  {
    if (!((id.ptr[i] >= '0' && id.ptr[i] <= '9') || (id.ptr[i] >= 'a' && id.ptr[i] <= 'f')))
    {
      return false;
    }
  }

  return true;
}

auth_result
auth_generate_id(char out[AUTH_ID_LENGTH + 1], auth_random_fn random, void *context)
{
  if (!out)
  {
    return AUTH_INVALID;
  }

  memset(out, 0, AUTH_ID_LENGTH + 1);
  unsigned char bytes[AUTH_ID_LENGTH / 2] = {0};

  if (sodium_init() < 0)
  {
    return AUTH_ERROR;
  }

  if (random)
  {
    if (random(context, bytes, sizeof(bytes)) != 0)
    {
      sodium_memzero(bytes, sizeof(bytes));
      return AUTH_ERROR;
    }
  }
  else
  {
    randombytes_buf(bytes, sizeof(bytes));
  }

  sodium_bin2hex(out, AUTH_ID_LENGTH + 1, bytes, sizeof(bytes));
  sodium_memzero(bytes, sizeof(bytes));
  return AUTH_OK;
}

void
auth_token_free(char *token)
{
  if (token)
  {
    sodium_memzero(token, strlen(token));
    jwt_free_str(token);
  }
}

auth_result
auth_token_issue(const auth_config_t *config, string_t user_id, string_t session_id,
                 int64_t now, char **out_token)
{
  if (!out_token)
  {
    return AUTH_INVALID;
  }

  *out_token = NULL;

  if (!valid_config(config) || !valid_id(user_id) || !valid_id(session_id) || now < 0 ||
      now > AUTH_TIME_MAX - config->ttl || now > LONG_MAX - config->ttl)
  {
    return AUTH_INVALID;
  }

  char subject[AUTH_ID_LENGTH + 1] = {0};
  char id[AUTH_ID_LENGTH + 1] = {0};
  memcpy(subject, user_id.ptr, user_id.len);
  memcpy(id, session_id.ptr, session_id.len);
  jwt_t *jwt = NULL;

  if (jwt_new(&jwt) != 0)
  {
    return AUTH_ERROR;
  }

  auth_result result = AUTH_ERROR;

  if (jwt_set_alg(jwt, JWT_ALG_HS256, config->key, (int)config->key_len) != 0 ||
      jwt_add_header(jwt, "typ", "JWT") != 0 ||
      jwt_add_grant(jwt, "iss", config->issuer) != 0 ||
      jwt_add_grant(jwt, "aud", config->audience) != 0 ||
      jwt_add_grant(jwt, "sub", subject) != 0 ||
      jwt_add_grant(jwt, "jti", id) != 0 ||
      jwt_add_grant_int(jwt, "iat", (long)now) != 0 ||
      jwt_add_grant_int(jwt, "nbf", (long)now) != 0 ||
      jwt_add_grant_int(jwt, "exp", (long)(now + config->ttl)) != 0)
  {
    goto done;
  }

  char *token = jwt_encode_str(jwt);

  if (!token)
  {
    goto done;
  }

  if (strlen(token) > AUTH_TOKEN_MAX)
  {
    auth_token_free(token);
    goto done;
  }

  *out_token = token;
  result = AUTH_OK;

done:
  jwt_free(jwt);
  return result;
}

static json_t *
decode_object(const char *text, size_t length, auth_result *result)
{
  unsigned char decoded[AUTH_TOKEN_MAX];
  size_t decoded_length;

  if (!length || sodium_base642bin(decoded, sizeof(decoded), text, length, NULL,
                                   &decoded_length, NULL, sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0)
  {
    return NULL;
  }

  json_error_t error;
  json_t *value = json_loadb((const char *)decoded, decoded_length, JSON_REJECT_DUPLICATES, &error);

  if (!value)
  {
    if (json_error_code(&error) == json_error_out_of_memory)
    {
      *result = AUTH_ERROR;
    }

    return NULL;
  }

  if (!json_is_object(value))
  {
    json_decref(value);
    return NULL;
  }

  return value;
}

static bool
json_text_equals(json_t *value, const char *expected)
{
  return json_is_string(value) && json_string_length(value) == strlen(expected) &&
         memcmp(json_string_value(value), expected, strlen(expected)) == 0;
}

static bool
read_time_claim(json_t *claims, const char *name, int64_t *out)
{
  json_t *value = json_object_get(claims, name);

  if (!json_is_integer(value))
  {
    return false;
  }

  json_int_t number = json_integer_value(value);

  if (number < 0 || number > AUTH_TIME_MAX)
  {
    return false;
  }

  *out = (int64_t)number;
  return true;
}

static bool
read_id_claim(json_t *claims, const char *name, char out[AUTH_ID_LENGTH + 1])
{
  json_t *value = json_object_get(claims, name);

  if (!json_is_string(value))
  {
    return false;
  }

  string_t id = {.ptr = (char *)json_string_value(value), .len = json_string_length(value)};

  if (!valid_id(id))
  {
    return false;
  }

  memcpy(out, id.ptr, id.len);
  out[id.len] = '\0';
  return true;
}

auth_result
auth_token_verify(const auth_config_t *config, string_t token, int64_t now, auth_claims_t *out_claims)
{
  if (!out_claims)
  {
    return AUTH_INVALID;
  }

  *out_claims = (auth_claims_t){0};

  if (!valid_config(config) || !token.ptr || !token.len || token.len > AUTH_TOKEN_MAX ||
      memchr(token.ptr, '\0', token.len) || now < 0 || now > AUTH_TIME_MAX)
  {
    return AUTH_INVALID;
  }

  const char *first = memchr(token.ptr, '.', token.len);
  const char *end = token.ptr + token.len;
  const char *second = first ? memchr(first + 1, '.', end - first - 1) : NULL;

  if (!second || second + 1 == end || memchr(second + 1, '.', end - second - 1))
  {
    return AUTH_INVALID;
  }

  auth_result result = AUTH_INVALID;
  json_t *header = decode_object(token.ptr, first - token.ptr, &result);
  json_t *claims = NULL;
  jwt_t *jwt = NULL;

  if (!header || !json_text_equals(json_object_get(header, "alg"), "HS256"))
  {
    goto done;
  }

  const char *key;
  json_t *value;
  json_object_foreach(header, key, value)
  {
    // This profile supports only alg and optional typ. No external keys/crit/b64.
    if (strcmp(key, "alg") != 0 &&
        !(strcmp(key, "typ") == 0 && json_text_equals(value, "JWT")))
    {
      goto done;
    }
  }

  // Validate the original payload, before libjwt can normalize duplicate keys.
  claims = decode_object(first + 1, second - first - 1, &result);

  if (!claims)
  {
    goto done;
  }

  // Strict base64url signature encoding and the HS256 signature length.
  unsigned char signature[32];
  size_t signature_length;

  if (sodium_base642bin(signature, sizeof(signature), second + 1, end - second - 1, NULL,
                        &signature_length, NULL, sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0 ||
      signature_length != sizeof(signature))
  {
    goto done;
  }

  char encoded[AUTH_TOKEN_MAX + 1];
  memcpy(encoded, token.ptr, token.len);
  encoded[token.len] = '\0';
  int error = jwt_decode(&jwt, encoded, config->key, (int)config->key_len);
  sodium_memzero(encoded, token.len);

  if (error != 0)
  {
    result = error == ENOMEM ? AUTH_ERROR : AUTH_INVALID;
    goto done;
  }

  if (jwt_get_alg(jwt) != JWT_ALG_HS256)
  {
    goto done;
  }

  auth_claims_t parsed = {0};

  if (!json_text_equals(json_object_get(claims, "iss"), config->issuer) ||
      !json_text_equals(json_object_get(claims, "aud"), config->audience) ||
      !read_id_claim(claims, "sub", parsed.user_id) ||
      !read_id_claim(claims, "jti", parsed.session_id) ||
      !read_time_claim(claims, "iat", &parsed.issued_at) ||
      !read_time_claim(claims, "nbf", &parsed.not_before) ||
      !read_time_claim(claims, "exp", &parsed.expires_at) ||
      parsed.not_before < parsed.issued_at || parsed.not_before >= parsed.expires_at ||
      parsed.expires_at - parsed.issued_at > config->ttl ||
      parsed.issued_at > now + AUTH_CLOCK_SKEW || parsed.not_before > now + AUTH_CLOCK_SKEW ||
      now >= parsed.expires_at + AUTH_CLOCK_SKEW)
  {
    goto done;
  }

  *out_claims = parsed;
  result = AUTH_OK;

done:
  jwt_free(jwt);
  json_decref(header);
  json_decref(claims);
  return result;
}

static bool
valid_password(string_t password)
{
  return password.ptr && password.len >= 8 && password.len <= 128 &&
         !memchr(password.ptr, '\0', password.len);
}

auth_result
auth_password_hash(string_t password, char out[crypto_pwhash_STRBYTES])
{
  if (!out)
  {
    return AUTH_INVALID;
  }

  sodium_memzero(out, crypto_pwhash_STRBYTES);

  if (!valid_password(password))
  {
    return AUTH_INVALID;
  }

  if (sodium_init() < 0 ||
      crypto_pwhash_str_alg(out, password.ptr, password.len,
                            crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                            crypto_pwhash_ALG_ARGON2ID13) != 0)
  {
    sodium_memzero(out, crypto_pwhash_STRBYTES);
    return AUTH_ERROR;
  }

  return AUTH_OK;
}

auth_result
auth_password_verify(const auth_config_t *config, const char *stored_hash, string_t password)
{
  if (!valid_password(password))
  {
    return AUTH_INVALID;
  }

  const char *hash = stored_hash ? stored_hash : config ? config->dummy_hash
                                                        : NULL;

  if (!hash || strnlen(hash, crypto_pwhash_STRBYTES) >= crypto_pwhash_STRBYTES ||
      strncmp(hash, "$argon2id$", 10) != 0)
  {
    return AUTH_ERROR;
  }

  if (sodium_init() < 0)
  {
    return AUTH_ERROR;
  }

  int result = crypto_pwhash_str_verify(hash, password.ptr, password.len);
  return result == 0 && stored_hash ? AUTH_OK : AUTH_INVALID;
}
