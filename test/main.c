#include <stdio.h>
#include <string.h>

#include "test.h"

int
main(int argc, char **argv)
{
  if (argc > 1 && strcmp(argv[1], "--help") == 0)
  {
    printf("Usage: %s [--detailed]\n", argv[0]);
    return 0;
  }

  test_ctx_t ctx = {
      .detailed = 0,
      .indent = 0};

  if (argc > 1 && strcmp(argv[1], "--detailed") == 0)
  {
    ctx.detailed = 1;
  }

  printf("===== TEST STARTED =====\n\n");

  test_http(&ctx);

  printf("\n====== TEST ENDED ======\n");

  return 0;
}