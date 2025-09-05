#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// バージョン情報
#define BASIC_VERSION_MAJOR 1
#define BASIC_VERSION_MINOR 1
#define BASIC_VERSION_STRING "Microsoft BASIC M6502 v1.1 (C Port)"

// 設定定数
#define MAX_LINE_LENGTH 72
#define MAX_VARIABLES 256
#define MAX_PROGRAM_LINES 1000
#define MAX_STRING_LENGTH 255
#define MAX_ARRAY_DIMENSIONS 8
#define STACK_SIZE 512

// エラーコード定義
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
    ERR_DIVISION_BY_ZERO = 11,
    ERR_SUBSCRIPT_OUT_OF_RANGE = 12,
    ERR_REDIMENSIONED_ARRAY = 13,
    ERR_RETURN_WITHOUT_GOSUB = 14,
    ERR_NEXT_WITHOUT_FOR = 15
} error_code_t;

// 変数型定義
typedef enum {
    VAR_NUMERIC,
    VAR_STRING,
    VAR_ARRAY_NUMERIC,
    VAR_ARRAY_STRING
} variable_type_t;

// 浮動小数点数表現
typedef struct {
    uint8_t exponent;       // 指数部
    uint8_t mantissa[4];    // 仮数部
    uint8_t sign;           // 符号
} basic_float_t;

typedef union {
    basic_float_t legacy;   // 互換性のための形式
    double modern;          // 現代的な形式
} numeric_value_t;

// 変数構造体
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
            uint16_t total_elements;
        } array;
    } value;
    struct variable* next;
} variable_t;

// プログラム行構造体
typedef struct program_line {
    uint16_t line_number;
    uint16_t length;
    char* text;
    struct program_line* next;
} program_line_t;

// FOR-NEXTループスタック
typedef struct for_stack_entry {
    char var_name[3];       // ループ変数名
    numeric_value_t limit;  // 上限値
    numeric_value_t step;   // ステップ値
    program_line_t* line;   // FOR文の行
    uint16_t position;      // FOR文内の位置
    struct for_stack_entry* next;
} for_stack_entry_t;

// GOSUB-RETURNスタック
typedef struct gosub_stack_entry {
    program_line_t* line;   // 戻り先の行
    uint16_t position;      // 戻り先の位置
    struct gosub_stack_entry* next;
} gosub_stack_entry_t;

// システム状態構造体
typedef struct {
    // メモリポインター
    program_line_t* program_start;  // プログラム開始
    variable_t* variables;          // 変数リスト
    
    // 実行状態
    program_line_t* current_line;   // 現在実行中の行
    uint16_t current_position;      // 行内の位置
    bool running;                   // 実行中フラグ
    bool immediate_mode;            // 即座実行モード
    
    // 制御フラグ
    uint8_t valtyp;         // 値型 (0=数値, 1=文字列)
    uint8_t dimflg;         // DIM処理フラグ
    uint8_t subflg;         // 添字変数フラグ
    uint8_t inpflg;         // INPUT処理フラグ
    
    // 入出力関連
    uint8_t trmpos;         // 端末位置
    uint8_t linwid;         // 行幅
    uint16_t linnum;        // 現在行番号
    char input_buffer[MAX_LINE_LENGTH + 1];
    
    // スタック
    for_stack_entry_t* for_stack;
    gosub_stack_entry_t* gosub_stack;
    
    // エラー処理
    error_code_t error_code;
    char error_msg[128];
    
    // 一時変数
    uint8_t charac;         // 区切り文字
    uint8_t endchr;         // 終端文字
    uint8_t count;          // 汎用カウンター
    
    // 乱数シード
    uint32_t rnd_seed;
} basic_state_t;

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
    uint16_t position;      // トークンの位置
} token_t;

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

// パーサー状態
typedef struct {
    const char* text;
    uint16_t position;
    uint16_t length;
    char current_char; // parser.c uses a cached current character
} parser_state_t;

// 公開API関数
int basic_init(basic_state_t* state);
void basic_cleanup(basic_state_t* state);
int basic_load_program(basic_state_t* state, const char* filename);
int basic_run_program(basic_state_t* state);
int basic_execute_line(basic_state_t* state, const char* line);
void basic_list_program(basic_state_t* state);
void basic_new_program(basic_state_t* state);

// エラー処理
void set_error(basic_state_t* state, error_code_t code, const char* msg);
void clear_error(basic_state_t* state);
bool has_error(basic_state_t* state);
void print_error(basic_state_t* state);

// ユーティリティ関数
char* safe_string_dup(const char* src, uint16_t max_len);
char* number_to_string(numeric_value_t n);
numeric_value_t string_to_number(const char* str);
void cleanup_data_state(void);
int count_variables(basic_state_t* state);
numeric_value_t double_to_numeric(double d);
double numeric_to_double(numeric_value_t n);

// コマンド関数
int cmd_print(basic_state_t* state, parser_state_t* parser);
int cmd_let(basic_state_t* state, parser_state_t* parser);
int cmd_list(basic_state_t* state, parser_state_t* parser);
int cmd_run(basic_state_t* state, parser_state_t* parser);
int cmd_new(basic_state_t* state, parser_state_t* parser);
int cmd_for(basic_state_t* state, parser_state_t* parser);
int cmd_next(basic_state_t* state, parser_state_t* parser);
int cmd_if(basic_state_t* state, parser_state_t* parser);
int cmd_goto(basic_state_t* state, parser_state_t* parser);
int cmd_goto_line(basic_state_t* state, uint16_t line_number);
int cmd_gosub(basic_state_t* state, parser_state_t* parser);
int cmd_return(basic_state_t* state, parser_state_t* parser);
int cmd_dim(basic_state_t* state, parser_state_t* parser);
int cmd_data(basic_state_t* state, parser_state_t* parser);
int cmd_read(basic_state_t* state, parser_state_t* parser);
int cmd_restore(basic_state_t* state, parser_state_t* parser);
int cmd_input(basic_state_t* state, parser_state_t* parser);
int cmd_input_ex(basic_state_t* state, parser_state_t* parser);
int cmd_clear(basic_state_t* state, parser_state_t* parser);
int cmd_stop(basic_state_t* state, parser_state_t* parser);
int cmd_end(basic_state_t* state, parser_state_t* parser);
int cmd_poke(basic_state_t* state, parser_state_t* parser);
int cmd_wait(basic_state_t* state, parser_state_t* parser);
int cmd_get(basic_state_t* state, parser_state_t* parser);
int cmd_def(basic_state_t* state, parser_state_t* parser);
int cmd_null(basic_state_t* state, parser_state_t* parser);
int cmd_on_goto(basic_state_t* state, parser_state_t* parser);
int cmd_cont(basic_state_t* state, parser_state_t* parser);
int cmd_rem(basic_state_t* state, parser_state_t* parser);

// パーサー関数
token_t get_next_token(basic_state_t* state, parser_state_t* parser);
eval_result_t evaluate_expression(basic_state_t* state, parser_state_t* parser);
eval_result_t evaluate_variable(basic_state_t* state, parser_state_t* parser, const char* var_name);
eval_result_t evaluate_function(basic_state_t* state, parser_state_t* parser, uint8_t function_id);
eval_result_t perform_operation(basic_state_t* state, eval_result_t left, char operator, eval_result_t right);

// 変数・配列関数
variable_t* find_variable(basic_state_t* state, const char* name);
variable_t* create_variable(basic_state_t* state, const char* name, variable_type_t type);
program_line_t* find_line(basic_state_t* state, uint16_t line_number);
eval_result_t access_array_element(basic_state_t* state, const char* var_name, uint16_t* indices, uint8_t index_count);
int assign_array_element(basic_state_t* state, const char* var_name, uint16_t* indices, uint8_t index_count, eval_result_t value);

#endif // BASIC_H
