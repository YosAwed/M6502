#include "basic.h"

// 文字列の安全な複製
char* safe_string_dup(const char* src, uint16_t max_len) {
    if (!src) return NULL;
    
    uint16_t len = strlen(src);
    if (len > max_len) len = max_len;
    
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, src, len);
    result[len] = '\0';
    return result;
}

// 数値の文字列変換
char* number_to_string(numeric_value_t n) {
    double val = numeric_to_double(n);
    char* result = (char*)malloc(32);
    if (!result) return NULL;
    
    // 元のBASICの形式に合わせた数値表示
    if (val == floor(val) && fabs(val) < 1e9) {
        // 整数として表示
        sprintf(result, "%.0f", val);
    } else {
        // 浮動小数点として表示
        sprintf(result, "%g", val);
    }
    
    return result;
}

// 文字列の数値変換
numeric_value_t string_to_number(const char* str) {
    if (!str) return double_to_numeric(0.0);
    
    // 先頭の空白をスキップ
    while (*str == ' ' || *str == '\t') str++;
    
    // 空文字列は0
    if (*str == '\0') return double_to_numeric(0.0);
    
    // 数値部分のみを解析
    char* endptr;
    double val = strtod(str, &endptr);
    
    return double_to_numeric(val);
}

// 数値変換関数
numeric_value_t double_to_numeric(double d) {
    numeric_value_t result;
    result.modern = d;
    return result;
}

double numeric_to_double(numeric_value_t n) {
    return n.modern;
}
