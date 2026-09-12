#ifndef APP_JSON_P_H
#define APP_JSON_P_H

const char *skip_whitespace(const char *str, const char *end);
const char *parse_json_str(const char *str, const char *end, char *out_buf);

#endif //APP_JSON_P_H