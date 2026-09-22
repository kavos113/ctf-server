#define _POSIX_C_SOURCE 200809L

#include "test.h"
#include "util.h"

#include <app/auth.h>
#include <jansson.h>
#include <jwt.h>

#define TEST_KEY   "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
#define USER_ID    "0123456789abcdef0123456789abcdef"
#define SESSION_ID "fedcba9876543210fedcba9876543210"
#define PAYLOAD    "{\"iss\":\"ctf-server\",\"aud\":\"ctf-client\",\"sub\":\"" USER_ID "\",\"jti\":\"" SESSION_ID "\",\"iat\":1000,\"nbf\":1000,\"exp\":4600}"

char *__real_jwt_encode_str(jwt_t *jwt);
static bool fail_signing;
static bool fail_hashing;
int __real_crypto_pwhash_str_alg(char *out, const char *password, unsigned long long length,
                                 unsigned long long opslimit, size_t memlimit, int algorithm);

int
__wrap_crypto_pwhash_str_alg(char *out, const char *password, unsigned long long length,
                             unsigned long long opslimit, size_t memlimit, int algorithm)
{
  if (fail_hashing)
  {
    memset(out, 'x', crypto_pwhash_STRBYTES);
    return -1;
  }

  return __real_crypto_pwhash_str_alg(out, password, length, opslimit, memlimit, algorithm);
}

char *
__wrap_jwt_encode_str(jwt_t *jwt)
{
  return fail_signing ? NULL : __real_jwt_encode_str(jwt);
}

static void
test_auth_config_load(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *key;
    const char *issuer;
    const char *audience;
    const char *ttl;
    auth_result expected;
    int64_t seconds;
  } cases[] = {
      {.name = "default ttl", .key = TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .expected = AUTH_OK, .seconds = 3600},
      {.name = "minimum ttl", .key = TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .ttl = "1", .expected = AUTH_OK, .seconds = 1},
      {.name = "maximum ttl", .key = TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .ttl = "86400", .expected = AUTH_OK, .seconds = 86400},
      {.name = "64 byte key", .key = TEST_KEY TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .expected = AUTH_OK, .seconds = 3600},
      {.name = "missing key", .issuer = "ctf-server", .audience = "ctf-client", .expected = AUTH_INVALID},
      {.name = "short key", .key = "0123", .issuer = "ctf-server", .audience = "ctf-client", .expected = AUTH_INVALID},
      {.name = "odd hex", .key = TEST_KEY "a", .issuer = "ctf-server", .audience = "ctf-client", .expected = AUTH_INVALID},
      {.name = "nonhex", .key = TEST_KEY "xx", .issuer = "ctf-server", .audience = "ctf-client", .expected = AUTH_INVALID},
      {.name = "long key", .key = TEST_KEY TEST_KEY "00", .issuer = "ctf-server", .audience = "ctf-client", .expected = AUTH_INVALID},
      {.name = "missing issuer", .key = TEST_KEY, .audience = "ctf-client", .expected = AUTH_INVALID},
      {.name = "empty audience", .key = TEST_KEY, .issuer = "ctf-server", .audience = "", .expected = AUTH_INVALID},
      {.name = "issuer control", .key = TEST_KEY, .issuer = "ctf\n", .audience = "ctf-client", .expected = AUTH_INVALID},
      {.name = "zero ttl", .key = TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .ttl = "0", .expected = AUTH_INVALID},
      {.name = "negative ttl", .key = TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .ttl = "-1", .expected = AUTH_INVALID},
      {.name = "empty ttl", .key = TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .ttl = "", .expected = AUTH_INVALID},
      {.name = "ttl suffix", .key = TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .ttl = "3600s", .expected = AUTH_INVALID},
      {.name = "ttl overflow", .key = TEST_KEY, .issuer = "ctf-server", .audience = "ctf-client", .ttl = "999999999999999999", .expected = AUTH_INVALID},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    auth_config_t config;
    memset(&config, 0xff, sizeof(config));
    ASSERT_EQ(cases[i].name, cases[i].expected,
              auth_config_load(&config, cases[i].key, cases[i].issuer, cases[i].audience, cases[i].ttl));

    if (cases[i].expected == AUTH_OK)
    {
      ASSERT_EQ(cases[i].name, strlen(cases[i].key) / 2, config.key_len);
      ASSERT_EQ(cases[i].name, cases[i].seconds, config.ttl);
      ASSERT_STR_EQ(cases[i].name, cases[i].issuer, config.issuer);
      ASSERT_STR_EQ(cases[i].name, cases[i].audience, config.audience);
    }
    else
    {
      ASSERT_TRUE(cases[i].name, sodium_is_zero((unsigned char *)&config, sizeof(config)));
    }

    auth_config_dispose(&config);
    ASSERT_TRUE(cases[i].name, sodium_is_zero((unsigned char *)&config, sizeof(config)));
    CHECK_TEST(cases[i].name);
  }
}

static int
fixed_random(void *context, unsigned char *out, size_t length)
{
  memset(out, 0xab, length);
  return *(bool *)context ? -1 : 0;
}

static void
test_auth_generate_id(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    bool fail;
  } cases[] = {
      {.name = "fixed random"},
      {.name = "random failure", .fail = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    char id[AUTH_ID_LENGTH + 1];
    bool fail = cases[i].fail;
    ASSERT_EQ(cases[i].name, fail ? AUTH_ERROR : AUTH_OK, auth_generate_id(id, fixed_random, &fail));

    if (fail)
    {
      ASSERT_TRUE(cases[i].name, sodium_is_zero((unsigned char *)id, sizeof(id)));
    }
    else
    {
      ASSERT_STR_EQ(cases[i].name, "abababababababababababababababab", id);
    }

    CHECK_TEST(cases[i].name);
  }

  ctx->is_canceled = false;
  char first[AUTH_ID_LENGTH + 1];
  char second[AUTH_ID_LENGTH + 1];
  ASSERT_EQ("real random", AUTH_OK, auth_generate_id(first, NULL, NULL));
  ASSERT_EQ("real random", AUTH_OK, auth_generate_id(second, NULL, NULL));
  ASSERT_EQ("real random", (size_t)32, strlen(first));
  ASSERT_STR_NE("real random", first, second);
  CHECK_TEST("real random");
}

static void
test_auth_token_issue(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *id;
    int64_t now;
    bool fail;
    auth_result result;
  } cases[] = {
      {.name = "round trip", .id = USER_ID, .now = 1000, .result = AUTH_OK},
      {.name = "invalid id", .id = "dummy", .now = 1000, .result = AUTH_INVALID},
      {.name = "negative time", .id = USER_ID, .now = -1, .result = AUTH_INVALID},
      {.name = "time overflow", .id = USER_ID, .now = INT64_MAX, .result = AUTH_INVALID},
      {.name = "signature failure", .id = USER_ID, .now = 1000, .fail = true, .result = AUTH_ERROR},
  };
  auth_config_t config;
  auth_config_load(&config, TEST_KEY, "ctf-server", "ctf-client", NULL);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    char *token = NULL;
    fail_signing = cases[i].fail;
    auth_result result = auth_token_issue(&config, string_from_cstr(cases[i].id),
                                          string_from_cstr(SESSION_ID), cases[i].now, &token);
    fail_signing = false;
    ASSERT_EQ(cases[i].name, cases[i].result, result);

    if (result == AUTH_OK)
    {
      auth_claims_t claims;
      ASSERT_EQ(cases[i].name, AUTH_OK, auth_token_verify(&config, string_from_cstr(token), cases[i].now, &claims));
      ASSERT_STR_EQ(cases[i].name, USER_ID, claims.user_id);
      ASSERT_STR_EQ(cases[i].name, SESSION_ID, claims.session_id);
      ASSERT_EQ(cases[i].name, cases[i].now, claims.issued_at);
      ASSERT_EQ(cases[i].name, cases[i].now, claims.not_before);
      ASSERT_EQ(cases[i].name, cases[i].now + 3600, claims.expires_at);
    }
    else
    {
      ASSERT_NULL(cases[i].name, token);
    }

    auth_token_free(token);
    CHECK_TEST(cases[i].name);
  }

  auth_config_dispose(&config);
}

// Construct correctly signed JWTs with malformed original JSON, including duplicates.
static char *
sign_raw(const auth_config_t *config, const char *header, const char *payload)
{
  char buffer[8192];
  sodium_bin2base64(buffer, sizeof(buffer), (const unsigned char *)header, strlen(header), sodium_base64_VARIANT_URLSAFE_NO_PADDING);
  size_t length = strlen(buffer);
  buffer[length++] = '.';
  sodium_bin2base64(buffer + length, sizeof(buffer) - length, (const unsigned char *)payload, strlen(payload), sodium_base64_VARIANT_URLSAFE_NO_PADDING);
  length = strlen(buffer);
  crypto_auth_hmacsha256_state state;
  unsigned char signature[crypto_auth_hmacsha256_BYTES];
  crypto_auth_hmacsha256_init(&state, config->key, config->key_len);
  crypto_auth_hmacsha256_update(&state, (const unsigned char *)buffer, length);
  crypto_auth_hmacsha256_final(&state, signature);
  buffer[length++] = '.';
  sodium_bin2base64(buffer + length, sizeof(buffer) - length, signature, sizeof(signature), sodium_base64_VARIANT_URLSAFE_NO_PADDING);
  return string_from_cstr_dup(buffer).ptr;
}

static void
test_auth_token_verify(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *header;
    const char *field;
    const char *value;
    const char *suffix;
    int64_t now;
    bool tamper;
    bool wrong_key;
    auth_result result;
  } cases[] = {
      {.name = "valid", .now = 1000, .result = AUTH_OK},
      {.name = "skew before start", .now = 970, .result = AUTH_OK},
      {.name = "too early", .now = 969, .result = AUTH_INVALID},
      {.name = "expiration grace", .now = 4629, .result = AUTH_OK},
      {.name = "expiration boundary", .now = 4630, .result = AUTH_INVALID},
      {.name = "negative now", .now = -1, .result = AUTH_INVALID},
      {.name = "overflow now", .now = INT64_MAX, .result = AUTH_INVALID},
      {.name = "wrong key", .now = 1000, .wrong_key = true, .result = AUTH_INVALID},
      {.name = "tampered", .now = 1000, .tamper = true, .result = AUTH_INVALID},
      {.name = "none", .header = "{\"alg\":\"none\"}", .now = 1000, .result = AUTH_INVALID},
      {.name = "HS384", .header = "{\"alg\":\"HS384\"}", .now = 1000, .result = AUTH_INVALID},
      {.name = "header duplicate", .header = "{\"alg\":\"none\",\"alg\":\"HS256\"}", .now = 1000, .result = AUTH_INVALID},
      {.name = "escaped header duplicate", .header = "{\"alg\":\"HS256\",\"\\u0061lg\":\"HS256\"}", .now = 1000, .result = AUTH_INVALID},
      {.name = "critical header", .header = "{\"alg\":\"HS256\",\"crit\":[\"exp\"]}", .now = 1000, .result = AUTH_INVALID},
      {.name = "external key", .header = "{\"alg\":\"HS256\",\"jku\":\"https://example.test/key\"}", .now = 1000, .result = AUTH_INVALID},
      {.name = "wrong typ", .header = "{\"alg\":\"HS256\",\"typ\":\"other\"}", .now = 1000, .result = AUTH_INVALID},
      {.name = "array header", .header = "[]", .now = 1000, .result = AUTH_INVALID},
      {.name = "string exp", .field = "exp", .value = "\"4600\"", .now = 1000, .result = AUTH_INVALID},
      {.name = "real exp", .field = "exp", .value = "4600.0", .now = 1000, .result = AUTH_INVALID},
      {.name = "boolean exp", .field = "exp", .value = "true", .now = 1000, .result = AUTH_INVALID},
      {.name = "missing exp", .field = "exp", .now = 1000, .result = AUTH_INVALID},
      {.name = "missing iat", .field = "iat", .now = 1000, .result = AUTH_INVALID},
      {.name = "missing nbf", .field = "nbf", .now = 1000, .result = AUTH_INVALID},
      {.name = "missing issuer", .field = "iss", .now = 1000, .result = AUTH_INVALID},
      {.name = "issuer mismatch", .field = "iss", .value = "\"other\"", .now = 1000, .result = AUTH_INVALID},
      {.name = "audience mismatch", .field = "aud", .value = "\"other\"", .now = 1000, .result = AUTH_INVALID},
      {.name = "audience array", .field = "aud", .value = "[\"ctf-client\"]", .now = 1000, .result = AUTH_INVALID},
      {.name = "invalid sub", .field = "sub", .value = "\"dummy\"", .now = 1000, .result = AUTH_INVALID},
      {.name = "missing session", .field = "jti", .now = 1000, .result = AUTH_INVALID},
      {.name = "future iat", .field = "iat", .value = "1031", .now = 1000, .result = AUTH_INVALID},
      {.name = "future nbf", .field = "nbf", .value = "1031", .now = 1000, .result = AUTH_INVALID},
      {.name = "nbf before iat", .field = "nbf", .value = "999", .now = 1000, .result = AUTH_INVALID},
      {.name = "nbf equals exp", .field = "nbf", .value = "4600", .now = 4600, .result = AUTH_INVALID},
      {.name = "exp before iat", .field = "exp", .value = "999", .now = 1000, .result = AUTH_INVALID},
      {.name = "excessive lifetime", .field = "exp", .value = "4601", .now = 1000, .result = AUTH_INVALID},
      {.name = "negative iat", .field = "iat", .value = "-1", .now = 1000, .result = AUTH_INVALID},
      {.name = "overflow exp", .field = "exp", .value = "9223372036854775807", .now = 1000, .result = AUTH_INVALID},
      {.name = "duplicate claim", .suffix = ",\"exp\":4600}", .now = 1000, .result = AUTH_INVALID},
      {.name = "escaped duplicate claim", .suffix = ",\"\\u0065xp\":4600}", .now = 1000, .result = AUTH_INVALID},
      {.name = "NUL claim", .suffix = ",\"extra\":\"\\u0000\"}", .now = 1000, .result = AUTH_INVALID},
  };
  auth_config_t config;
  auth_config_load(&config, TEST_KEY, "ctf-server", "ctf-client", NULL);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    json_error_t error;
    json_t *claims = json_loads(PAYLOAD, 0, &error);

    if (cases[i].field)
    {
      if (cases[i].value)
      {
        json_object_set_new(claims, cases[i].field, json_loads(cases[i].value, JSON_DECODE_ANY, &error));
      }
      else
      {
        json_object_del(claims, cases[i].field);
      }
    }

    char *payload = json_dumps(claims, JSON_COMPACT);
    char raw[2048];
    snprintf(raw, sizeof(raw), "%s", payload);

    if (cases[i].suffix)
    {
      snprintf(raw + strlen(raw) - 1, sizeof(raw) - strlen(raw) + 1, "%s", cases[i].suffix);
    }

    char *token = sign_raw(&config, cases[i].header ? cases[i].header : "{\"alg\":\"HS256\"}", raw);

    if (cases[i].tamper)
    {
      char *signature = strrchr(token, '.') + 1;
      signature[0] = signature[0] == 'a' ? 'b' : 'a';
    }

    auth_config_t verifier = config;

    if (cases[i].wrong_key)
    {
      verifier.key[0] ^= 1;
    }

    auth_claims_t output;
    memset(&output, 0xff, sizeof(output));
    ASSERT_EQ(cases[i].name, cases[i].result,
              auth_token_verify(&verifier, string_from_cstr(token), cases[i].now, &output));

    if (cases[i].result != AUTH_OK)
    {
      ASSERT_TRUE(cases[i].name, sodium_is_zero((unsigned char *)&output, sizeof(output)));
    }

    auth_config_dispose(&verifier);
    free(token);
    free(payload);
    json_decref(claims);
    CHECK_TEST(cases[i].name);
  }

  const char *invalid[] = {"", "a", "a.b", "a.b.", "a.b.c.d", "!.!.!", "e30=.e30=.AAAA"};

  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
  {
    ctx->is_canceled = false;
    auth_claims_t claims;
    ASSERT_EQ("malformed token", AUTH_INVALID, auth_token_verify(&config, string_from_cstr(invalid[i]), 1000, &claims));
    CHECK_TEST("malformed token");
  }

  ctx->is_canceled = false;
  char oversized[AUTH_TOKEN_MAX + 2];
  memset(oversized, 'a', sizeof(oversized));
  auth_claims_t claims;
  ASSERT_EQ("oversized token", AUTH_INVALID,
            auth_token_verify(&config, (string_t){.ptr = oversized, .len = sizeof(oversized)}, 1000, &claims));
  CHECK_TEST("oversized token");
  auth_config_dispose(&config);
}

static void
test_auth_password(test_ctx_t *ctx)
{
  ctx->is_canceled = false;
  auth_config_t config = {0};
  char hash[crypto_pwhash_STRBYTES];
  ASSERT_EQ("password hash", AUTH_OK, auth_password_hash(string_from_cstr("password"), hash));
  memcpy(config.dummy_hash, hash, sizeof(hash));
  const struct
  {
    const char *name;
    const char *password;
    const char *hash;
    auth_result result;
  } cases[] = {
      {.name = "matching", .password = "password", .hash = hash, .result = AUTH_OK},
      {.name = "mismatch", .password = "different", .hash = hash, .result = AUTH_INVALID},
      {.name = "unknown user never matches", .password = "password", .result = AUTH_INVALID},
      {.name = "malformed hash", .password = "password", .hash = "bad", .result = AUTH_ERROR},
      {.name = "short password", .password = "short", .hash = hash, .result = AUTH_INVALID},
  };
  CHECK_TEST("password hash");

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    ASSERT_EQ(cases[i].name, cases[i].result,
              auth_password_verify(&config, cases[i].hash, string_from_cstr(cases[i].password)));
    CHECK_TEST(cases[i].name);
  }

  ctx->is_canceled = false;
  ASSERT_EQ("invalid hash input", AUTH_INVALID, auth_password_hash(string_from_cstr("short"), hash));
  ASSERT_TRUE("invalid hash cleanup", sodium_is_zero((unsigned char *)hash, sizeof(hash)));
  fail_hashing = true;
  ASSERT_EQ("hash failure", AUTH_ERROR, auth_password_hash(string_from_cstr("password"), hash));
  fail_hashing = false;
  ASSERT_TRUE("hash failure cleanup", sodium_is_zero((unsigned char *)hash, sizeof(hash)));
  CHECK_TEST("hash error cleanup");
  sodium_memzero(hash, sizeof(hash));
  auth_config_dispose(&config);
}

static void
test_auth_init_from_env(test_ctx_t *ctx)
{
  ctx->is_canceled = false;
  const char *names[] = {"JWT_KEY_HEX", "JWT_ISSUER", "JWT_AUDIENCE", "JWT_TTL_SECONDS"};
  const char *values[] = {TEST_KEY, "ctf-server", "ctf-client", "3600"};
  char *saved[4] = {0};

  for (size_t i = 0; i < 4; i++)
  {
    const char *value = getenv(names[i]);
    saved[i] = value ? string_from_cstr_dup(value).ptr : NULL;
    setenv(names[i], values[i], 1);
  }

  auth_config_t config;
  ASSERT_EQ("startup configuration", AUTH_OK, auth_init_from_env(&config));
  ASSERT_STR_N_EQ("startup dummy hash", "$argon2id$", config.dummy_hash, 10);
  auth_config_dispose(&config);
  fail_hashing = true;
  ASSERT_EQ("startup hash failure", AUTH_ERROR, auth_init_from_env(&config));
  fail_hashing = false;
  ASSERT_TRUE("startup hash cleanup", sodium_is_zero((unsigned char *)&config, sizeof(config)));
  unsetenv("JWT_KEY_HEX");
  ASSERT_EQ("missing startup key", AUTH_INVALID, auth_init_from_env(&config));
  ASSERT_TRUE("startup cleanup", sodium_is_zero((unsigned char *)&config, sizeof(config)));

  for (size_t i = 0; i < 4; i++)
  {
    if (saved[i])
    {
      setenv(names[i], saved[i], 1);
      sodium_memzero(saved[i], strlen(saved[i]));
      free(saved[i]);
    }
    else
    {
      unsetenv(names[i]);
    }
  }

  CHECK_TEST("startup configuration");
}

void
test_app_auth(test_ctx_t *ctx)
{
  test_auth_config_load(ctx);
  test_auth_generate_id(ctx);
  test_auth_token_issue(ctx);
  test_auth_token_verify(ctx);
  test_auth_password(ctx);
  test_auth_init_from_env(ctx);
}
