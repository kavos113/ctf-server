#ifndef APP_CONTEST_H
#define APP_CONTEST_H

#include <stdbool.h>
#include <stdint.h>

// Parse YYYY-MM-DDTHH:MM:SSZ or YYYY-MM-DDTHH:MM:SS+/-HH:MM into Unix seconds.
bool contest_start_parse(const char *value, int64_t *start_at);

#endif // APP_CONTEST_H
