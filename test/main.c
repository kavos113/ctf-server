#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
      .detailed = false,
      .indent = 0,
      .is_canceled = false,
      .failed_count = 0,
      .passed_count = 0,
  };

  if (argc > 1 && strcmp(argv[1], "--detailed") == 0)
  {
    ctx.detailed = true;
  }

  struct timespec start_time, end_time;
  timespec_get(&start_time, TIME_UTC);
  printf("===== TEST STARTED =====\n\n");

  test_http_request(&ctx);
  test_http_server(&ctx);

  timespec_get(&end_time, TIME_UTC);
  double elapsed = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e6;
  printf("\n====== TEST ENDED ======\n");
  printf("All: %d, Passed: %d, Failed: %d, Time: %.3f ms\n", ctx.passed_count + ctx.failed_count, ctx.passed_count, ctx.failed_count, elapsed);

  return 0;
}