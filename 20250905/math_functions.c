#include "basic.h"
#include <math.h>
#include <time.h>

// 外部関数の宣言
extern numeric_value_t double_to_numeric(double d);
extern double numeric_to_double(numeric_value_t n);

// SGN関数 - 符号を返す
numeric_value_t func_sgn(numeric_value_t x) {
    double val = numeric_to_double(x);
    if (val > 0.0) return double_to_numeric(1.0);
    if (val < 0.0) return double_to_numeric(-1.0);
    return double_to_numeric(0.0);
}

// INT関数 - 整数部を返す（切り捨て）
numeric_value_t func_int(numeric_value_t x) {
    double val = numeric_to_double(x);
    return double_to_numeric(floor(val));
}

// ABS関数 - 絶対値を返す
numeric_value_t func_abs(numeric_value_t x) {
    double val = numeric_to_double(x);
    return double_to_numeric(fabs(val));
}

// SQR関数 - 平方根を返す
numeric_value_t func_sqr(numeric_value_t x) {
    double val = numeric_to_double(x);
    if (val < 0.0) {
        // 元のBASICでは負数の平方根でエラー
        return double_to_numeric(0.0); // エラー処理は呼び出し側で
    }
    return double_to_numeric(sqrt(val));
}

// EXP関数 - 指数関数（e^x）
numeric_value_t func_exp(numeric_value_t x) {
    double val = numeric_to_double(x);
    // オーバーフロー防止
    if (val > 88.0) {
        return double_to_numeric(1.7e38); // 最大値
    }
    if (val < -88.0) {
        return double_to_numeric(0.0);
    }
    return double_to_numeric(exp(val));
}

// LOG関数 - 自然対数
numeric_value_t func_log(numeric_value_t x) {
    double val = numeric_to_double(x);
    if (val <= 0.0) {
        // 元のBASICでは0以下の対数でエラー
        return double_to_numeric(0.0); // エラー処理は呼び出し側で
    }
    return double_to_numeric(log(val));
}

// SIN関数 - サイン
numeric_value_t func_sin(numeric_value_t x) {
    double val = numeric_to_double(x);
    return double_to_numeric(sin(val));
}

// COS関数 - コサイン
numeric_value_t func_cos(numeric_value_t x) {
    double val = numeric_to_double(x);
    return double_to_numeric(cos(val));
}

// TAN関数 - タンジェント
numeric_value_t func_tan(numeric_value_t x) {
    double val = numeric_to_double(x);
    return double_to_numeric(tan(val));
}

// ATN関数 - アークタンジェント
numeric_value_t func_atn(numeric_value_t x) {
    double val = numeric_to_double(x);
    return double_to_numeric(atan(val));
}

// RND関数 - 乱数生成
numeric_value_t func_rnd(basic_state_t* state, numeric_value_t x) {
    double val = numeric_to_double(x);
    
    if (val < 0.0) {
        // 負数の場合、シードを設定
        state->rnd_seed = (uint32_t)(-val);
        srand(state->rnd_seed);
    } else if (val == 0.0) {
        // 0の場合、前回の値を繰り返し
        srand(state->rnd_seed);
    }
    // 正数の場合は新しい乱数を生成
    
    double random_val = (double)rand() / RAND_MAX;
    return double_to_numeric(random_val);
}

// 算術演算関数

// 加算
numeric_value_t math_add(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    return double_to_numeric(val_a + val_b);
}

// 減算
numeric_value_t math_subtract(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    return double_to_numeric(val_a - val_b);
}

// 乗算
numeric_value_t math_multiply(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    return double_to_numeric(val_a * val_b);
}

// 除算
numeric_value_t math_divide(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    
    if (val_b == 0.0) {
        // ゼロ除算エラー（呼び出し側でチェック）
        return double_to_numeric(0.0);
    }
    
    return double_to_numeric(val_a / val_b);
}

// べき乗
numeric_value_t math_power(numeric_value_t base, numeric_value_t exponent) {
    double val_base = numeric_to_double(base);
    double val_exp = numeric_to_double(exponent);
    
    // 特殊ケースの処理
    if (val_base == 0.0 && val_exp <= 0.0) {
        return double_to_numeric(0.0); // エラーケース
    }
    
    if (val_base < 0.0 && val_exp != floor(val_exp)) {
        // 負数の非整数乗はエラー
        return double_to_numeric(0.0);
    }
    
    return double_to_numeric(pow(val_base, val_exp));
}

// 比較関数

// 等しい
int math_equal(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    return (fabs(val_a - val_b) < 1e-9) ? -1 : 0; // BASICでは真は-1
}

// より小さい
int math_less_than(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    return (val_a < val_b) ? -1 : 0;
}

// より大きい
int math_greater_than(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    return (val_a > val_b) ? -1 : 0;
}

// 以下
int math_less_equal(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    return (val_a <= val_b) ? -1 : 0;
}

// 以上
int math_greater_equal(numeric_value_t a, numeric_value_t b) {
    double val_a = numeric_to_double(a);
    double val_b = numeric_to_double(b);
    return (val_a >= val_b) ? -1 : 0;
}

// 等しくない
int math_not_equal(numeric_value_t a, numeric_value_t b) {
    return !math_equal(a, b) ? -1 : 0;
}

// 論理演算

// AND演算
numeric_value_t math_and(numeric_value_t a, numeric_value_t b) {
    // BASICのANDは整数のビット演算
    int val_a = (int)numeric_to_double(a);
    int val_b = (int)numeric_to_double(b);
    return double_to_numeric((double)(val_a & val_b));
}

// OR演算
numeric_value_t math_or(numeric_value_t a, numeric_value_t b) {
    // BASICのORは整数のビット演算
    int val_a = (int)numeric_to_double(a);
    int val_b = (int)numeric_to_double(b);
    return double_to_numeric((double)(val_a | val_b));
}

// NOT演算
numeric_value_t math_not(numeric_value_t a) {
    // BASICのNOTは整数のビット反転
    int val_a = (int)numeric_to_double(a);
    return double_to_numeric((double)(~val_a));
}

// 単項マイナス
numeric_value_t math_negate(numeric_value_t a) {
    double val_a = numeric_to_double(a);
    return double_to_numeric(-val_a);
}

// 数値の正規化（元のBASICの浮動小数点形式に合わせる）
numeric_value_t normalize_number(numeric_value_t n) {
    double val = numeric_to_double(n);
    
    // 非常に小さい値は0にする
    if (fabs(val) < 1e-38) {
        return double_to_numeric(0.0);
    }
    
    // 非常に大きい値は制限する
    if (val > 1.7e38) {
        return double_to_numeric(1.7e38);
    }
    if (val < -1.7e38) {
        return double_to_numeric(-1.7e38);
    }
    
    return double_to_numeric(val);
}
