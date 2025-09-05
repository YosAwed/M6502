#include "basic.h"
#include <ctype.h>

// LEN関数 - 文字列の長さを返す
eval_result_t func_len(const char* str) {
    eval_result_t result;
    result.type = 0; // 数値
    
    if (!str) {
        result.value.num = double_to_numeric(0.0);
    } else {
        result.value.num = double_to_numeric((double)strlen(str));
    }
    
    return result;
}

// ASC関数 - 文字列の最初の文字のASCIIコードを返す
eval_result_t func_asc(const char* str) {
    eval_result_t result;
    result.type = 0; // 数値
    
    if (!str || strlen(str) == 0) {
        result.value.num = double_to_numeric(0.0);
    } else {
        result.value.num = double_to_numeric((double)(unsigned char)str[0]);
    }
    
    return result;
}

// CHR$関数 - ASCIIコードから文字を生成
eval_result_t func_chr(int ascii_code) {
    eval_result_t result;
    result.type = 1; // 文字列
    
    if (ascii_code < 0 || ascii_code > 255) {
        // 無効なASCIIコード
        result.value.str.data = (char*)malloc(1);
        if (result.value.str.data) {
            result.value.str.data[0] = '\0';
            result.value.str.length = 0;
        }
        return result;
    }
    
    result.value.str.data = (char*)malloc(2);
    if (!result.value.str.data) {
        result.value.str.length = 0;
        return result;
    }
    
    result.value.str.data[0] = (char)ascii_code;
    result.value.str.data[1] = '\0';
    result.value.str.length = 1;
    
    return result;
}

// STR$関数 - 数値を文字列に変換
eval_result_t func_str(numeric_value_t num) {
    eval_result_t result;
    result.type = 1; // 文字列
    
    double val = numeric_to_double(num);
    char temp[32];
    
    // 元のBASICの形式に合わせた数値表示
    if (val >= 0) {
        sprintf(temp, " %g", val); // 正数には先頭にスペース
    } else {
        sprintf(temp, "%g", val);
    }
    
    result.value.str.data = safe_string_dup(temp, MAX_STRING_LENGTH);
    result.value.str.length = result.value.str.data ? strlen(result.value.str.data) : 0;
    
    return result;
}

// VAL関数 - 文字列を数値に変換
eval_result_t func_val(const char* str) {
    eval_result_t result;
    result.type = 0; // 数値
    
    if (!str) {
        result.value.num = double_to_numeric(0.0);
        return result;
    }
    
    // 先頭の空白をスキップ
    while (*str == ' ' || *str == '\t') str++;
    
    // 空文字列は0
    if (*str == '\0') {
        result.value.num = double_to_numeric(0.0);
        return result;
    }
    
    // 数値部分のみを解析
    char* endptr;
    double val = strtod(str, &endptr);
    
    result.value.num = double_to_numeric(val);
    return result;
}

// LEFT$関数 - 文字列の左側から指定文字数を取得
eval_result_t func_left(const char* str, int n) {
    eval_result_t result;
    result.type = 1; // 文字列
    
    if (!str || n <= 0) {
        result.value.str.data = (char*)malloc(1);
        if (result.value.str.data) {
            result.value.str.data[0] = '\0';
            result.value.str.length = 0;
        }
        return result;
    }
    
    int str_len = strlen(str);
    int copy_len = (n > str_len) ? str_len : n;
    
    result.value.str.data = (char*)malloc(copy_len + 1);
    if (!result.value.str.data) {
        result.value.str.length = 0;
        return result;
    }
    
    strncpy(result.value.str.data, str, copy_len);
    result.value.str.data[copy_len] = '\0';
    result.value.str.length = copy_len;
    
    return result;
}

// RIGHT$関数 - 文字列の右側から指定文字数を取得
eval_result_t func_right(const char* str, int n) {
    eval_result_t result;
    result.type = 1; // 文字列
    
    if (!str || n <= 0) {
        result.value.str.data = (char*)malloc(1);
        if (result.value.str.data) {
            result.value.str.data[0] = '\0';
            result.value.str.length = 0;
        }
        return result;
    }
    
    int str_len = strlen(str);
    int start_pos = (n >= str_len) ? 0 : str_len - n;
    int copy_len = str_len - start_pos;
    
    result.value.str.data = (char*)malloc(copy_len + 1);
    if (!result.value.str.data) {
        result.value.str.length = 0;
        return result;
    }
    
    strcpy(result.value.str.data, str + start_pos);
    result.value.str.length = copy_len;
    
    return result;
}

// MID$関数 - 文字列の中間部分を取得
eval_result_t func_mid(const char* str, int start, int len) {
    eval_result_t result;
    result.type = 1; // 文字列
    
    if (!str || start < 1 || len <= 0) {
        result.value.str.data = (char*)malloc(1);
        if (result.value.str.data) {
            result.value.str.data[0] = '\0';
            result.value.str.length = 0;
        }
        return result;
    }
    
    int str_len = strlen(str);
    int start_pos = start - 1; // BASICは1ベース
    
    if (start_pos >= str_len) {
        // 開始位置が文字列長を超えている
        result.value.str.data = (char*)malloc(1);
        if (result.value.str.data) {
            result.value.str.data[0] = '\0';
            result.value.str.length = 0;
        }
        return result;
    }
    
    int available_len = str_len - start_pos;
    int copy_len = (len > available_len) ? available_len : len;
    
    result.value.str.data = (char*)malloc(copy_len + 1);
    if (!result.value.str.data) {
        result.value.str.length = 0;
        return result;
    }
    
    strncpy(result.value.str.data, str + start_pos, copy_len);
    result.value.str.data[copy_len] = '\0';
    result.value.str.length = copy_len;
    
    return result;
}

// 文字列連結
eval_result_t string_concatenate(const char* str1, const char* str2) {
    eval_result_t result;
    result.type = 1; // 文字列
    
    int len1 = str1 ? strlen(str1) : 0;
    int len2 = str2 ? strlen(str2) : 0;
    int total_len = len1 + len2;
    
    if (total_len > MAX_STRING_LENGTH) {
        total_len = MAX_STRING_LENGTH;
    }
    
    result.value.str.data = (char*)malloc(total_len + 1);
    if (!result.value.str.data) {
        result.value.str.length = 0;
        return result;
    }
    
    result.value.str.data[0] = '\0';
    
    if (str1 && len1 > 0) {
        int copy_len1 = (len1 > total_len) ? total_len : len1;
        strncpy(result.value.str.data, str1, copy_len1);
        result.value.str.data[copy_len1] = '\0';
    }
    
    if (str2 && len2 > 0 && len1 < total_len) {
        int remaining = total_len - len1;
        int copy_len2 = (len2 > remaining) ? remaining : len2;
        strncat(result.value.str.data, str2, copy_len2);
    }
    
    result.value.str.length = strlen(result.value.str.data);
    return result;
}

// 文字列比較
int string_compare(const char* str1, const char* str2) {
    if (!str1 && !str2) return 0;
    if (!str1) return -1;
    if (!str2) return 1;
    
    return strcmp(str1, str2);
}

// 文字列の等価比較
int string_equal(const char* str1, const char* str2) {
    return (string_compare(str1, str2) == 0) ? -1 : 0; // BASICでは真は-1
}

// 文字列の大小比較
int string_less_than(const char* str1, const char* str2) {
    return (string_compare(str1, str2) < 0) ? -1 : 0;
}

int string_greater_than(const char* str1, const char* str2) {
    return (string_compare(str1, str2) > 0) ? -1 : 0;
}

int string_less_equal(const char* str1, const char* str2) {
    return (string_compare(str1, str2) <= 0) ? -1 : 0;
}

int string_greater_equal(const char* str1, const char* str2) {
    return (string_compare(str1, str2) >= 0) ? -1 : 0;
}

int string_not_equal(const char* str1, const char* str2) {
    return (string_compare(str1, str2) != 0) ? -1 : 0;
}
