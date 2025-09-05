# Microsoft BASIC M6502 C言語移植設計

## 1. 全体アーキテクチャ

### 1.1 モジュール構成
```
basic_interpreter/
├── src/
│   ├── main.c              # メインエントリーポイント
│   ├── basic_core.h        # 核心データ構造定義
│   ├── basic_core.c        # 核心機能実装
│   ├── memory.h            # メモリ管理
│   ├── memory.c
│   ├── parser.h            # 構文解析
│   ├── parser.c
│   ├── evaluator.h         # 式評価
│   ├── evaluator.c
│   ├── commands.h          # BASICコマンド
│   ├── commands.c
│   ├── functions.h         # BASIC関数
│   ├── functions.c
│   ├── math.h              # 数学演算
│   ├── math.c
│   ├── strings.h           # 文字列処理
│   ├── strings.c
│   ├── io.h                # 入出力処理
│   └── io.c
├── include/
│   └── basic.h             # 公開API
└── tests/
    └── test_*.c            # テストファイル
```

## 2. 基本データ構造

### 2.1 システム状態構造体
```c
typedef struct {
    // メモリポインター (元のPage Zero変数)
    uint16_t txttab;        // プログラムテキスト開始
    uint16_t vartab;        // 変数領域開始
    uint16_t arytab;        // 配列テーブル開始
    uint16_t strend;        // ストレージ終端
    
    // 制御フラグ
    uint8_t valtyp;         // 値型 (0=数値, 1=文字列)
    uint8_t dimflg;         // DIM処理フラグ
    uint8_t subflg;         // 添字変数フラグ
    uint8_t inpflg;         // INPUT処理フラグ
    
    // 入出力関連
    uint8_t trmpos;         // 端末位置
    uint8_t linwid;         // 行幅
    uint16_t linnum;        // 現在行番号
    
    // 一時変数
    uint8_t charac;         // 区切り文字
    uint8_t endchr;         // 終端文字
    uint8_t count;          // 汎用カウンター
    
    // エラー処理
    uint8_t error_code;     // エラーコード
    char error_msg[64];     // エラーメッセージ
} basic_state_t;
```

### 2.2 浮動小数点数表現
```c
// 元の6502 BASICの浮動小数点形式をエミュレート
typedef struct {
    uint8_t exponent;       // 指数部
    uint8_t mantissa[4];    // 仮数部 (32ビット)
    uint8_t sign;           // 符号
} basic_float_t;

// 現代的な実装では標準のfloat/doubleも使用可能
typedef union {
    basic_float_t legacy;   // 互換性のための形式
    double modern;          // 現代的な形式
} numeric_value_t;
```

### 2.3 変数管理
```c
typedef enum {
    VAR_NUMERIC,
    VAR_STRING,
    VAR_ARRAY_NUMERIC,
    VAR_ARRAY_STRING
} variable_type_t;

typedef struct variable {
    char name[3];           // 変数名 (最大2文字 + NULL)
    variable_type_t type;
    union {
        numeric_value_t num;
        struct {
            char* data;
            uint16_t length;
        } str;
        struct {
            void* data;
            uint16_t* dimensions;
            uint8_t dim_count;
        } array;
    } value;
    struct variable* next;
} variable_t;
```

### 2.4 プログラム行管理
```c
typedef struct program_line {
    uint16_t line_number;
    uint16_t length;
    char* text;
    struct program_line* next;
} program_line_t;
```

## 3. 主要機能モジュール設計

### 3.1 メモリ管理 (memory.h/c)
```c
// メモリ初期化
int memory_init(basic_state_t* state, size_t memory_size);

// 動的メモリ割り当て
void* basic_malloc(size_t size);
void basic_free(void* ptr);

// ガベージコレクション
void garbage_collect(basic_state_t* state);

// 文字列メモリ管理
char* string_alloc(uint16_t length);
void string_free(char* str);
```

### 3.2 構文解析 (parser.h/c)
```c
// トークン定義
typedef enum {
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_VARIABLE,
    TOKEN_KEYWORD,
    TOKEN_OPERATOR,
    TOKEN_DELIMITER,
    TOKEN_EOL,
    TOKEN_EOF
} token_type_t;

typedef struct {
    token_type_t type;
    union {
        numeric_value_t number;
        char* string;
        uint8_t keyword_id;
        char operator;
    } value;
} token_t;

// パーサー関数
int parse_line(basic_state_t* state, const char* line);
token_t get_next_token(basic_state_t* state);
int parse_statement(basic_state_t* state);
```

### 3.3 式評価 (evaluator.h/c)
```c
// 式評価結果
typedef struct {
    uint8_t type;           // 0=数値, 1=文字列
    union {
        numeric_value_t num;
        struct {
            char* data;
            uint16_t length;
        } str;
    } value;
} eval_result_t;

// 式評価関数
eval_result_t evaluate_expression(basic_state_t* state);
eval_result_t evaluate_numeric_expression(basic_state_t* state);
eval_result_t evaluate_string_expression(basic_state_t* state);
```

### 3.4 BASICコマンド (commands.h/c)
```c
// コマンド関数ポインター型
typedef int (*command_func_t)(basic_state_t* state);

// コマンドテーブル
typedef struct {
    const char* name;
    command_func_t func;
} command_entry_t;

// 主要コマンド実装
int cmd_print(basic_state_t* state);
int cmd_let(basic_state_t* state);
int cmd_if(basic_state_t* state);
int cmd_for(basic_state_t* state);
int cmd_next(basic_state_t* state);
int cmd_goto(basic_state_t* state);
int cmd_gosub(basic_state_t* state);
int cmd_return(basic_state_t* state);
int cmd_input(basic_state_t* state);
int cmd_dim(basic_state_t* state);
int cmd_run(basic_state_t* state);
int cmd_list(basic_state_t* state);
int cmd_new(basic_state_t* state);
```

### 3.5 BASIC関数 (functions.h/c)
```c
// 関数実装
numeric_value_t func_abs(numeric_value_t x);
numeric_value_t func_sgn(numeric_value_t x);
numeric_value_t func_int(numeric_value_t x);
numeric_value_t func_sqr(numeric_value_t x);
numeric_value_t func_sin(numeric_value_t x);
numeric_value_t func_cos(numeric_value_t x);
numeric_value_t func_tan(numeric_value_t x);
numeric_value_t func_atn(numeric_value_t x);
numeric_value_t func_log(numeric_value_t x);
numeric_value_t func_exp(numeric_value_t x);
numeric_value_t func_rnd(numeric_value_t x);

// 文字列関数
eval_result_t func_len(const char* str);
eval_result_t func_left(const char* str, int n);
eval_result_t func_right(const char* str, int n);
eval_result_t func_mid(const char* str, int start, int len);
eval_result_t func_chr(int ascii);
eval_result_t func_asc(const char* str);
eval_result_t func_str(numeric_value_t num);
eval_result_t func_val(const char* str);
```

## 4. 実装方針

### 4.1 互換性
- 元のBASIC M6502の動作を可能な限り再現
- 浮動小数点演算の精度を維持
- エラーメッセージの互換性

### 4.2 現代化
- 標準Cライブラリの活用
- メモリ安全性の向上
- ポータビリティの確保
- デバッグ機能の追加

### 4.3 拡張性
- モジュラー設計
- プラグイン機能の検討
- 新しいBASIC機能の追加容易性

## 5. エラーハンドリング

### 5.1 エラーコード定義
```c
typedef enum {
    ERR_NONE = 0,
    ERR_SYNTAX = 1,
    ERR_ILLEGAL_QUANTITY = 2,
    ERR_OUT_OF_MEMORY = 3,
    ERR_UNDEF_STATEMENT = 4,
    ERR_UNDEF_FUNCTION = 5,
    ERR_OUT_OF_DATA = 6,
    ERR_TYPE_MISMATCH = 7,
    ERR_STRING_TOO_LONG = 8,
    ERR_FORMULA_TOO_COMPLEX = 9,
    ERR_CANT_CONTINUE = 10,
    ERR_DIVISION_BY_ZERO = 11
} error_code_t;
```

### 5.2 エラー処理関数
```c
void set_error(basic_state_t* state, error_code_t code, const char* msg);
void clear_error(basic_state_t* state);
int has_error(basic_state_t* state);
void print_error(basic_state_t* state);
```

