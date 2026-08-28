#include <stdio.h>

#include "test.h"

int
main(void)
{
  printf("---------- test started -----------\n");

  test_http();

  printf("---------- test ended -------------\n");

  return 0;
}