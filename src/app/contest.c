#include "contest.h"

#include <stddef.h>
#include <string.h>

static int
read_digits(const char *value, size_t length)
{
  int result = 0;

  for (size_t i = 0; i < length; i++)
  {
    if (value[i] < '0' || value[i] > '9')
    {
      return -1;
    }

    result = result * 10 + value[i] - '0';
  }

  return result;
}

bool
contest_time_parse(const char *value, int64_t *timestamp)
{
  *timestamp = 0;

  if (!value || !*value)
  {
    return true;
  }

  size_t length = strlen(value);

  if ((length != 20 && length != 25) || value[4] != '-' || value[7] != '-' ||
      value[10] != 'T' || value[13] != ':' || value[16] != ':')
  {
    return false;
  }

  int year = read_digits(value, 4);
  int month = read_digits(value + 5, 2);
  int day = read_digits(value + 8, 2);
  int hour = read_digits(value + 11, 2);
  int minute = read_digits(value + 14, 2);
  int second = read_digits(value + 17, 2);
  int offset = 0;

  if (length == 20)
  {
    if (value[19] != 'Z')
    {
      return false;
    }
  }
  else
  {
    int offset_hour = read_digits(value + 20, 2);
    int offset_minute = read_digits(value + 23, 2);

    if ((value[19] != '+' && value[19] != '-') || value[22] != ':' ||
        offset_hour < 0 || offset_hour > 23 || offset_minute < 0 || offset_minute > 59)
    {
      return false;
    }

    offset = (offset_hour * 60 + offset_minute) * 60;

    if (value[19] == '-')
    {
      offset = -offset;
    }
  }

  if (year < 1970 || month < 1 || month > 12 || day < 1 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
  {
    return false;
  }

  const int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);

  if (day > month_days[month - 1] + (month == 2 && leap))
  {
    return false;
  }

  // Gregorian days since 1970-01-01; conversion does not depend on the host timezone.
  int64_t days = (int64_t)(year - 1970) * 365 + (year - 1) / 4 - 1969 / 4 - (year - 1) / 100 + 1969 / 100 + (year - 1) / 400 - 1969 / 400;

  for (int i = 1; i < month; i++)
  {
    days += month_days[i - 1] + (i == 2 && leap);
  }

  int64_t parsed = (days + day - 1) * 86400 + hour * 3600 + minute * 60 + second - offset;

  if (parsed < 0)
  {
    return false;
  }

  *timestamp = parsed;
  return true;
}
