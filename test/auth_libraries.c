#include "test.h"
#include "util.h"

#include <errno.h>
#include <jansson.h>
#include <jwt.h>
#include <sodium.h>

static const unsigned char key[32] = "unit-test-key-not-for-production";

static int
provide_key(const jwt_t *token, jwt_key_t *out)
{
  if (jwt_get_alg((jwt_t *)token) != JWT_ALG_HS256)
  {
    return EINVAL;
  }

  out->jwt_key = key;
  out->jwt_key_len = sizeof(key);
  return 0;
}

static void
test_jwt_library(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    jwt_alg_t algorithm;
    bool wrong_key;
    bool tamper;
    bool success;
  } cases[] = {
      {.name = "HS256 round trip", .algorithm = JWT_ALG_HS256, .success = true},
      {.name = "none rejected", .algorithm = JWT_ALG_NONE},
      {.name = "HS384 rejected", .algorithm = JWT_ALG_HS384},
      {.name = "wrong key", .algorithm = JWT_ALG_HS256, .wrong_key = true},
      {.name = "tampered signature", .algorithm = JWT_ALG_HS256, .tamper = true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    jwt_t *token = NULL;
    jwt_t *decoded = NULL;
    const unsigned char other_key[32] = "different-unit-test-signing-key";
    ASSERT_EQ(cases[i].name, 0, jwt_new(&token));
    ASSERT_EQ(cases[i].name, 0, jwt_add_grant(token, "sub", "user"));
    ASSERT_EQ(cases[i].name, 0, jwt_add_grant_int(token, "exp", 3600));
    ASSERT_EQ(cases[i].name, 0,
              jwt_set_alg(token, cases[i].algorithm,
                          cases[i].algorithm == JWT_ALG_NONE ? NULL : cases[i].wrong_key ? other_key
                                                                                         : key,
                          cases[i].algorithm == JWT_ALG_NONE ? 0 : sizeof(key)));
    char *encoded = jwt_encode_str(token);
    ASSERT_NOT_NULL(cases[i].name, encoded);

    if (encoded)
    {
      if (cases[i].tamper)
      {
        char *signature = strrchr(encoded, '.') + 1;
        signature[0] = signature[0] == 'a' ? 'b' : 'a';
      }

      int result = jwt_decode_2(&decoded, encoded, provide_key);
      // libjwt 1.17 skips the key callback for alg=none. Check the resulting
      // algorithm as well; a successful decode alone is not authentication.
      bool verified = result == 0 && jwt_get_alg(decoded) == JWT_ALG_HS256;
      ASSERT_EQ(cases[i].name, cases[i].success, verified);

      if (result == 0)
      {
        ASSERT_STR_EQ(cases[i].name, "user", jwt_get_grant(decoded, "sub"));
        ASSERT_EQ(cases[i].name, 3600L, jwt_get_grant_int(decoded, "exp"));
      }
    }

    jwt_free(decoded);
    jwt_free(token);
    free(encoded);
    CHECK_TEST(cases[i].name);
  }
}

static void
test_claim_json(test_ctx_t *ctx)
{
  const struct
  {
    const char *name;
    const char *json;
    bool parse_success;
    bool integer_exp;
  } cases[] = {
      {.name = "integer", .json = "{\"exp\":3600}", .parse_success = true, .integer_exp = true},
      {.name = "string is not integer", .json = "{\"exp\":\"3600\"}", .parse_success = true},
      {.name = "real is not integer", .json = "{\"exp\":3600.0}", .parse_success = true},
      {.name = "boolean is not integer", .json = "{\"exp\":true}", .parse_success = true},
      {.name = "duplicate rejected", .json = "{\"exp\":1,\"exp\":2}"},
      {.name = "escaped duplicate rejected", .json = "{\"exp\":1,\"\\u0065xp\":2}"},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    ctx->is_canceled = false;
    json_error_t error;
    json_t *value = json_loads(cases[i].json, JSON_REJECT_DUPLICATES, &error);
    ASSERT_EQ(cases[i].name, cases[i].parse_success, value != NULL);

    if (value)
    {
      ASSERT_EQ(cases[i].name, cases[i].integer_exp, (bool)json_is_integer(json_object_get(value, "exp")));
      json_decref(value);
    }

    CHECK_TEST(cases[i].name);
  }
}

static void
test_password_library(test_ctx_t *ctx)
{
  ctx->is_canceled = false;
  char hash[crypto_pwhash_STRBYTES];
  ASSERT_TRUE("sodium init", sodium_init() >= 0);
  ASSERT_EQ("Argon2id", 0,
            crypto_pwhash_str_alg(hash, "password", 8,
                                  crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                  crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_ARGON2ID13));
  ASSERT_EQ("Argon2id", 0, crypto_pwhash_str_verify(hash, "password", 8));
  ASSERT_TRUE("Argon2id mismatch", crypto_pwhash_str_verify(hash, "incorrect", 9) != 0);
  sodium_memzero(hash, sizeof(hash));
  CHECK_TEST("Argon2id round trip");
}

void
test_auth_libraries(test_ctx_t *ctx)
{
  test_jwt_library(ctx);
  test_claim_json(ctx);
  test_password_library(ctx);
}
