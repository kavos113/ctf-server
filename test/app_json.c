#include "test.h"

#include <app/json.h>
#include <app/json_p.h>

#include "util.h"
#include "util_app.h"

void test_skip_whitespace(test_ctx_t *ctx);
void test_parse_json_str(test_ctx_t *ctx);
void test_parse_json_int(test_ctx_t *ctx);
void test_skip_json_value(test_ctx_t *ctx);
void test_json_to_challenge(test_ctx_t *ctx);
void test_json_to_challenges(test_ctx_t *ctx);
void test_challenge_to_json(test_ctx_t *ctx);
void test_challenge_to_json_without_flag(test_ctx_t *ctx);
void test_challenges_to_json(test_ctx_t *ctx);
void test_challenges_to_json_without_flag(test_ctx_t *ctx);

void
test_app_json(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_app_json");
  ctx->indent += PREFACE_INDENT;

  test_skip_whitespace(ctx);
  test_parse_json_str(ctx);
  test_parse_json_int(ctx);
  test_skip_json_value(ctx);
  test_json_to_challenge(ctx);
  test_json_to_challenges(ctx);
  test_challenge_to_json(ctx);
  test_challenge_to_json_without_flag(ctx);
  test_challenges_to_json(ctx);
  test_challenges_to_json_without_flag(ctx);

  ctx->indent -= PREFACE_INDENT;
}

void
test_skip_whitespace(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_skip_whitespace");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    const char *expected_output;
  } test_cases[] = {
      {
          .name = "success: leading spaces",
          .input = "   hello",
          .input_len = 8,
          .expected_output = "hello",
      },
      {
          .name = "success: leading tabs",
          .input = "\t\thello",
          .input_len = 7,
          .expected_output = "hello",
      },
      {
          .name = "success: leading newlines",
          .input = "\n\nhello",
          .input_len = 7,
          .expected_output = "hello",
      },
      {
          .name = "success: no leading whitespace",
          .input = "hello",
          .input_len = 5,
          .expected_output = "hello",
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    const char *end = tc->input + tc->input_len;
    const char *result = skip_whitespace(tc->input, end);

    ASSERT_NOT_NULL(tc->name, result);
    ASSERT_STR_EQ(tc->name, tc->expected_output, result);

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_json_str(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_json_str");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    string_t expected_output;
    bool expect_null;
  } test_cases[] = {
      {
          .name = "success: simple string",
          .input = "\"hello\"",
          .input_len = 7,
          .expected_output = (string_t){"hello", 5},
          .expect_null = false,
      },
      {
          .name = "success: empty string",
          .input = "\"\"",
          .input_len = 2,
          .expected_output = (string_t){"", 0},
          .expect_null = false,
      },
      {
          .name = "failure: missing closing quote",
          .input = "\"hello",
          .input_len = 6,
          .expected_output = (string_t){NULL, 0},
          .expect_null = true,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    const char *end = tc->input + tc->input_len;
    char out_buf[256] = {0};
    string_t out_str = {out_buf, sizeof(out_buf) - 1};
    const char *result = parse_json_str(tc->input, end, &out_str);

    if (tc->expect_null)
    {
      ASSERT_NULL(tc->name, result);
    }
    else
    {
      ASSERT_NOT_NULL(tc->name, result);
      ASSERT_STRING_EQ(tc->name, tc->expected_output, out_str);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_parse_json_int(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_parse_json_int");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    int expected_output;
    bool expect_null;
  } test_cases[] = {
      {
          .name = "success: simple integer",
          .input = "123",
          .input_len = 3,
          .expected_output = 123,
          .expect_null = false,
      },
      {
          .name = "success: negative integer",
          .input = "-456",
          .input_len = 4,
          .expected_output = -456,
          .expect_null = false,
      },
      {
          .name = "success: integer with trailing backquote",
          .input = "123\"",
          .input_len = 4,
          .expected_output = 123,
          .expect_null = false,
      },
      {
          .name = "failure: non-integer input",
          .input = "abc",
          .input_len = 3,
          .expected_output = 0,
          .expect_null = true,
      },
      {
          .name = "failure: empty input",
          .input = "",
          .input_len = 0,
          .expected_output = 0,
          .expect_null = true,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    const char *end = tc->input + tc->input_len;
    int out_value = 0;
    const char *result = parse_json_int(tc->input, end, &out_value);

    if (tc->expect_null)
    {
      ASSERT_NULL(tc->name, result);
    }
    else
    {
      ASSERT_NOT_NULL(tc->name, result);
      ASSERT_EQ(tc->name, out_value, tc->expected_output);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_skip_json_value(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_skip_json_value");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    bool expect_null;
  } test_cases[] = {
      {
          .name = "success: skip string value",
          .input = "\"hello\",",
          .input_len = 8,
          .expect_null = false,
      },
      {
          .name = "success: skip integer value",
          .input = "123,",
          .input_len = 4,
          .expect_null = false,
      },
      {
          .name = "success: skip object value",
          .input = "{\"key\": \"value\"},",
          .input_len = 17,
          .expect_null = false,
      },
      {
          .name = "success: skip array value",
          .input = "[1, 2, 3],",
          .input_len = 10,
          .expect_null = false,
      },
      {
          .name = "success: skip nested object value",
          .input = "{\"outer\": {\"inner\": 42}},",
          .input_len = 25,
          .expect_null = false,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    const char *end = tc->input + tc->input_len;
    const char *result = skip_json_value(tc->input, end);

    if (tc->expect_null)
    {
      ASSERT_NULL(tc->name, result);
    }
    else
    {
      ASSERT_NOT_NULL(tc->name, result);
      ASSERT_STR_EQ(tc->name, ",", result);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_json_to_challenge(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_json_to_challenge");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    challenge_t expected_output;
    bool expect_failure;
  } test_cases[] = {
      {
          .name = "success: valid challenge JSON",
          .input = "{\"id\": 1, \"creator_id\": \"user123\", \"name\": \"Challenge 1\", \"description\": \"This is a test challenge.\", \"flag\": \"flag{test}\", \"genre\": \"web\"}",
          .input_len = 139,
          .expected_output = {
              .id = 1,
              .creator_id = (string_t){"user123", 7},
              .name = (string_t){"Challenge 1", 11},
              .description = (string_t){"This is a test challenge.", 25},
              .flag = (string_t){"flag{test}", 10},
              .genre = CTF_GENRE_WEB,
          },
          .expect_failure = false,
      },
      {
          .name = "success: include unknown key-value pair",
          .input = "{\"id\": 2, \"creator_id\": \"user456\", \"name\": \"Challenge 2\", \"description\": \"Another test challenge.\", \"flag\": \"flag{test2}\", \"genre\": \"crypto\", \"unknown_key\": \"unknown_value\"}",
          .input_len = 173,
          .expected_output = {
              .id = 2,
              .creator_id = (string_t){"user456", 7},
              .name = (string_t){"Challenge 2", 11},
              .description = (string_t){"Another test challenge.", 23},
              .flag = (string_t){"flag{test2}", 11},
              .genre = CTF_GENRE_CRYPTO,
          },
          .expect_failure = false,
      },
      {
          .name = "failure: invalid JSON",
          .input = "{\"id\": 1, \"creator_id\": \"user123\", \"name\": \"Challenge 1\", \"description\": \"This is a test challenge.\", \"flag\": \"flag{test}\", \"genre\": \"web\"",
          .input_len = 127,
          .expected_output = {0},
          .expect_failure = true,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    challenge_t challenge;
    int result = json_to_challenge(tc->input, tc->input_len, &challenge);

    if (tc->expect_failure)
    {
      ASSERT_NE(tc->name, result, 0);
    }
    else
    {
      ASSERT_EQ(tc->name, result, 0);
      ASSERT_CHALLENGE_EQ(tc->name, tc->expected_output, challenge);
    }

    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_json_to_challenges(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_json_to_challenges");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    const char *input;
    size_t input_len;
    challenge_t *expected_output;
    size_t expected_count;
    bool expect_failure;
  } test_cases[] = {
      {
          .name = "success: valid challenges JSON array",
          .input = "["
                   "{\"id\": 1, \"creator_id\": \"user123\", \"name\": \"Challenge 1\", \"description\": \"This is a test challenge.\", \"flag\": \"flag{test}\", \"genre\": \"web\"},"
                   "{\"id\": 2, \"creator_id\": \"user456\", \"name\": \"Challenge 2\", \"description\": \"Another test challenge.\", \"flag\": \"flag{test2}\", \"genre\": \"crypto\"}]",
          .input_len = 283,
          .expected_output = (challenge_t[]){
              {
                  .id = 1,
                  .creator_id = (string_t){"user123", 7},
                  .name = (string_t){"Challenge 1", 11},
                  .description = (string_t){"This is a test challenge.", 25},
                  .flag = (string_t){"flag{test}", 10},
                  .genre = CTF_GENRE_WEB,
              },
              {
                  .id = 2,
                  .creator_id = (string_t){"user456", 7},
                  .name = (string_t){"Challenge 2", 11},
                  .description = (string_t){"Another test challenge.", 23},
                  .flag = (string_t){"flag{test2}", 11},
                  .genre = CTF_GENRE_CRYPTO,
              },
          },
          .expected_count = 2,
          .expect_failure = false,
      },
      {
          .name = "failure: invalid JSON array",
          .input = "["
                   "{\"id\": 1, \"creator_id\": \"user123\", \"name\": \"Challenge 1\", \"description\": \"This is a test challenge.\", \"flag\": \"flag{test}\", \"genre\": \"web\"},"
                   "{\"id\": 2, \"creator_id\": \"user456\", \"name\": \"Challenge 2\", \"description\": \"Another test challenge.\", \"flag\": \"flag{test2}\", \"genre\": \"crypto\"]",
          .input_len = 282,
          .expected_output = NULL,
          .expected_count = 0,
          .expect_failure = true,
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    challenge_t *challenges = NULL;
    size_t count = 0;
    json_to_challenges(tc->input, tc->input_len, &challenges, &count);

    if (tc->expect_failure)
    {
      ASSERT_EQ(tc->name, count, 0);
      ASSERT_NULL(tc->name, challenges);
    }
    else
    {
      ASSERT_EQ(tc->name, count, tc->expected_count);
      for (size_t j = 0; j < count; j++)
      {
        ASSERT_CHALLENGE_EQ(tc->name, tc->expected_output[j], challenges[j]);
      }
    }

    free(challenges);
    CHECK_TEST(tc->name);
  }
}

void
test_challenge_to_json(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_challenge_to_json");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    challenge_t input;
    string_t expected_output;
  } test_cases[] = {
      {
          .name = "success: valid challenge",
          .input = {
              .id = 1,
              .creator_id = (string_t){"user123", 7},
              .name = (string_t){"Challenge 1", 11},
              .description = (string_t){"This is a test challenge.", 25},
              .flag = (string_t){"flag{test}", 10},
              .genre = CTF_GENRE_WEB,
          },
          .expected_output = (string_t){"{\"id\":1,\"creator_id\":\"user123\",\"name\":\"Challenge 1\",\"description\":\"This is a test challenge.\",\"flag\":\"flag{test}\",\"genre\":\"web\"}", 128},
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    string_t result;
    challenge_to_json(&tc->input, &result, false);

    ASSERT_STRING_EQ(tc->name, tc->expected_output, result);

    free(result.ptr);
    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_challenges_to_json(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_challenges_to_json");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;

    challenge_t *input;
    size_t input_count;
    string_t expected_output;
  } test_cases[] = {
      {
          .name = "success: valid challenges",
          .input = (challenge_t[]){
              {
                  .id = 1,
                  .creator_id = (string_t){"user123", 7},
                  .name = (string_t){"Challenge 1", 11},
                  .description = (string_t){"This is a test challenge.", 25},
                  .flag = (string_t){"flag{test}", 10},
                  .genre = CTF_GENRE_WEB,
              },
              {
                  .id = 2,
                  .creator_id = (string_t){"user456", 7},
                  .name = (string_t){"Challenge 2", 11},
                  .description = (string_t){"Another test challenge.", 23},
                  .flag = (string_t){"flag{test2}", 11},
                  .genre = CTF_GENRE_CRYPTO,
              },
          },
          .input_count = 2,
          .expected_output = (string_t){"[{\"id\":1,\"creator_id\":\"user123\",\"name\":\"Challenge 1\",\"description\":\"This is a test challenge.\",\"flag\":\"flag{test}\",\"genre\":\"web\"},"
                                        "{\"id\":2,\"creator_id\":\"user456\",\"name\":\"Challenge 2\",\"description\":\"Another test challenge.\",\"flag\":\"flag{test2}\",\"genre\":\"crypto\"}]",
                                        261},
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    string_t result;
    challenges_to_json(tc->input, tc->input_count, &result, false);

    ASSERT_STRING_EQ(tc->name, tc->expected_output, result);

    free(result.ptr);
    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_challenge_to_json_without_flag(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_challenge_to_json_without_flag");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;
    challenge_t input;
    const char *expected_output;
  } test_cases[] = {
      {
          .name = "success: flag omitted from challenge",
          .input = {
              .id = 1,
              .creator_id = (string_t){"user123", 7},
              .name = (string_t){"Challenge 1", 11},
              .description = (string_t){"This is a test challenge.", 25},
              .flag = (string_t){"flag{test}", 10},
              .genre = CTF_GENRE_WEB,
          },
          .expected_output = "{\"id\":1,\"creator_id\":\"user123\",\"name\":\"Challenge 1\",\"description\":\"This is a test challenge.\",\"genre\":\"web\"}",
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    string_t result = {0};
    challenge_to_json_without_flag(&tc->input, &result, false);

    ASSERT_STRING_EQ(tc->name, string_from_cstr(tc->expected_output), result);

    free(result.ptr);
    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}

void
test_challenges_to_json_without_flag(test_ctx_t *ctx)
{
  PRINT_TEST_PREFACE("test_challenges_to_json_without_flag");
  ctx->indent += PREFACE_INDENT;

  struct test_case
  {
    const char *name;
    challenge_t *input;
    size_t input_count;
    const char *expected_output;
  } test_cases[] = {
      {
          .name = "success: flags omitted from challenge list",
          .input = (challenge_t[]){
              {
                  .id = 1,
                  .creator_id = (string_t){"user123", 7},
                  .name = (string_t){"Challenge 1", 11},
                  .description = (string_t){"This is a test challenge.", 25},
                  .flag = (string_t){"flag{test}", 10},
                  .genre = CTF_GENRE_WEB,
              },
              {
                  .id = 2,
                  .creator_id = (string_t){"user456", 7},
                  .name = (string_t){"Challenge 2", 11},
                  .description = (string_t){"Another test challenge.", 23},
                  .flag = (string_t){"flag{test2}", 11},
                  .genre = CTF_GENRE_CRYPTO,
              },
          },
          .input_count = 2,
          .expected_output = "[{\"id\":1,\"creator_id\":\"user123\",\"name\":\"Challenge 1\",\"description\":\"This is a test challenge.\",\"genre\":\"web\"},"
                             "{\"id\":2,\"creator_id\":\"user456\",\"name\":\"Challenge 2\",\"description\":\"Another test challenge.\",\"genre\":\"crypto\"}]",
      },
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
  {
    ctx->is_canceled = false;
    struct test_case *tc = &test_cases[i];

    string_t result = {0};
    challenges_to_json_without_flag(tc->input, tc->input_count, &result, false);

    ASSERT_STRING_EQ(tc->name, string_from_cstr(tc->expected_output), result);

    free(result.ptr);
    CHECK_TEST(tc->name);
  }

  ctx->indent -= PREFACE_INDENT;
}
