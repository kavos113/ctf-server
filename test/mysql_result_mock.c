#include "mysql_result_mock.h"

#include <app/str.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int test_mysql_live_results;

MYSQL_RES *
test_mysql_result(const char *const values[][6], size_t count, unsigned fields)
{
  assert(count <= 16 && fields <= 6);
  test_mysql_rows *rows = calloc(1, sizeof(*rows));
  rows->fields = fields;
  rows->count = count;

  for (size_t i = 0; i < count; i++)
  {
    for (unsigned j = 0; j < fields; j++)
    {
      if (values[i][j])
      {
        rows->rows[i][j] = string_from_cstr_dup(values[i][j]).ptr;
      }
    }
  }

  test_mysql_live_results++;
  return (MYSQL_RES *)rows;
}

my_ulonglong
__wrap_mysql_num_rows(MYSQL_RES *result)
{
  return ((test_mysql_rows *)result)->count;
}

unsigned int
__wrap_mysql_num_fields(MYSQL_RES *result)
{
  return ((test_mysql_rows *)result)->fields;
}

MYSQL_ROW
__wrap_mysql_fetch_row(MYSQL_RES *result)
{
  test_mysql_rows *rows = (test_mysql_rows *)result;

  if (rows->cursor == rows->count)
  {
    return NULL;
  }

  return rows->rows[rows->cursor++];
}

unsigned long *
__wrap_mysql_fetch_lengths(MYSQL_RES *result)
{
  test_mysql_rows *rows = (test_mysql_rows *)result;

  for (unsigned i = 0; i < rows->fields; i++)
  {
    const char *value = rows->rows[rows->cursor - 1][i];
    rows->lengths[i] = value ? strlen(value) : 0;
  }

  return rows->lengths;
}

void
__wrap_mysql_free_result(MYSQL_RES *result)
{
  test_mysql_rows *rows = (test_mysql_rows *)result;

  for (size_t i = 0; i < rows->count; i++)
  {
    for (size_t j = 0; j < 7; j++)
    {
      free(rows->rows[i][j]);
    }
  }

  free(rows);
  test_mysql_live_results--;
}
