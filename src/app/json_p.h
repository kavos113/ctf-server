#ifndef APP_JSON_P_H
#define APP_JSON_P_H

// 戻り値は終了位置のポインタ
const char *skip_whitespace(const char *str, const char *end);
const char *parse_json_str(const char *str, const char *end, char *out_buf);
const char *parse_json_int(const char *str, const char *end, int *out_value);
const char *skip_json_value(const char *str, const char *end);

#endif //APP_JSON_P_H