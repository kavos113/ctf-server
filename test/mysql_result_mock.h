#ifndef TEST_MYSQL_RESULT_MOCK_H
#define TEST_MYSQL_RESULT_MOCK_H

#include <mysql/mysql.h>
#include <stddef.h>

typedef struct
{
  unsigned fields;
  size_t count;
  size_t cursor;
  char *rows[16][7];
  unsigned long lengths[7];
} test_mysql_rows;

extern int test_mysql_live_results;
MYSQL_RES *test_mysql_result(const char *const values[][6], size_t count, unsigned fields);

#endif
