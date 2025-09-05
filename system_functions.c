#include "basic.h"
#include <time.h>
#include <ctype.h>

// 外部関数の宣言
extern numeric_value_t double_to_numeric(double d);
extern double numeric_to_double(numeric_value_t n);
extern variable_t* find_variable(basic_state_t* state, const char* name);
extern variable_t* create_variable(basic_state_t* state, const char* name, variable_type_t type);
extern token_t get_next_token(basic_state_t* state, parser_state_t* parser);
extern eval_result_t evaluate_expression(basic_state_t* state, parser_state_t* parser);

// 仮想メモリ（元の6502メモリをシミュレート）
static uint8_t virtual_memory[65536];
static bool memory_initialized = false;

// メモリ初期化
void init_virtual_memory(void) {
    if (!memory_initialized) {
        memset(virtual_memory, 0, sizeof(virtual_memory));
        memory_initialized = true;
    }
}

// PEEK関数 - メモリ読み取り
numeric_value_t func_peek(uint16_t address) {
    init_virtual_memory();
    return double_to_numeric((double)virtual_memory[address]);
}

// POKE文 - メモリ書き込み
int cmd_poke(basic_state_t* state, parser_state_t* parser_ptr) {
    // POKE address, value
    
    eval_result_t addr_result = evaluate_expression(state, parser_ptr);
    if (has_error(state) || addr_result.type != 0) {
        set_error(state, ERR_TYPE_MISMATCH, "Numeric address expected");
        return -1;
    }
    
    token_t comma = get_next_token(state, parser_ptr);
    if (comma.type != TOKEN_DELIMITER || comma.value.operator != ',') {
        set_error(state, ERR_SYNTAX, ", expected in POKE");
        return -1;
    }
    
    eval_result_t value_result = evaluate_expression(state, parser_ptr);
    if (has_error(state) || value_result.type != 0) {
        set_error(state, ERR_TYPE_MISMATCH, "Numeric value expected");
        return -1;
    }
    
    uint16_t address = (uint16_t)numeric_to_double(addr_result.value.num);
    uint8_t value = (uint8_t)numeric_to_double(value_result.value.num);
    
    init_virtual_memory();
    virtual_memory[address] = value;
    
    return 0;
}

// FRE関数 - 空きメモリ取得
numeric_value_t func_fre(numeric_value_t dummy) {
    (void)dummy; // 未使用パラメータ
    
    // 簡易的な空きメモリ計算
    // 実際のシステムでは malloc_stats() などを使用
    size_t free_memory = 32768; // 仮の値
    return double_to_numeric((double)free_memory);
}

// POS関数 - カーソル位置取得
numeric_value_t func_pos(numeric_value_t dummy) {
    (void)dummy; // 未使用パラメータ
    
    // 簡易的なカーソル位置（実際の実装では端末制御が必要）
    return double_to_numeric(0.0);
}

// INPUT文の実装
int cmd_input(basic_state_t* state, parser_state_t* parser_ptr) {
    // INPUT [prompt;] var1, var2, ...
    
    bool has_prompt = false;
    char* prompt_text = NULL;
    
    // プロンプトの確認
    token_t first_token = get_next_token(state, parser_ptr);
    if (first_token.type == TOKEN_STRING) {
        prompt_text = first_token.value.string;
        has_prompt = true;
        
        token_t sep_token = get_next_token(state, parser_ptr);
        if (sep_token.type == TOKEN_DELIMITER && (sep_token.value.operator == ';' || sep_token.value.operator == ',')) {
            // プロンプト付き
        } else {
            // プロンプトなし、変数として扱う
            has_prompt = false;
            free(prompt_text);
            prompt_text = NULL;
        }
    }
    
    // プロンプト表示
    if (has_prompt && prompt_text) {
        printf("%s", prompt_text);
        free(prompt_text);
    } else {
        printf("? ");
    }
    fflush(stdout);
    
    // 入力読み取り
    if (!fgets(state->input_buffer, sizeof(state->input_buffer), stdin)) {
        set_error(state, ERR_SYNTAX, "Input error");
        return -1;
    }
    
    // 改行文字を削除
    size_t len = strlen(state->input_buffer);
    if (len > 0 && state->input_buffer[len - 1] == '\n') {
        state->input_buffer[len - 1] = '\0';
    }
    
    // 入力データの解析
    char* input_ptr = state->input_buffer;
    
    while (true) {
        token_t var_token;
        if (!has_prompt) {
            var_token = first_token;
            has_prompt = true; // 一度だけ使用
        } else {
            var_token = get_next_token(state, parser_ptr);
        }
        
        if (var_token.type != TOKEN_VARIABLE) {
            set_error(state, ERR_SYNTAX, "Variable expected in INPUT");
            return -1;
        }
        
        // 入力値の取得
        char* comma_pos = strchr(input_ptr, ',');
        char* value_str;
        
        if (comma_pos) {
            *comma_pos = '\0';
            value_str = input_ptr;
            input_ptr = comma_pos + 1;
        } else {
            value_str = input_ptr;
        }
        
        // 前後の空白を除去
        while (*value_str == ' ') value_str++;
        char* end = value_str + strlen(value_str) - 1;
        while (end > value_str && *end == ' ') *end-- = '\0';
        
        // 変数への代入
        bool is_string = strchr(var_token.value.string, '$') != NULL;
        variable_type_t var_type = is_string ? VAR_STRING : VAR_NUMERIC;
        
        variable_t* var = create_variable(state, var_token.value.string, var_type);
        if (!var) {
            free(var_token.value.string);
            return -1;
        }
        
        if (is_string) {
            if (var->value.str.data) free(var->value.str.data);
            var->value.str.data = (char*)malloc(strlen(value_str) + 1);
            if (var->value.str.data) {
                strcpy(var->value.str.data, value_str);
                var->value.str.length = strlen(value_str);
            }
        } else {
            // strict numeric parse: require entire field to be a number
            char* endptr = NULL;
            double v = strtod(value_str, &endptr);
            while (endptr && *endptr && isspace((unsigned char)*endptr)) endptr++;
            if (!endptr || *endptr != '\0' || strlen(value_str) == 0) {
                set_error(state, ERR_TYPE_MISMATCH, "Numeric expected");
                free(var_token.value.string);
                return -1;
            }
            var->value.num = double_to_numeric(v);
        }
        
        free(var_token.value.string);
        
        // 次の変数があるかチェック
        token_t next_token = get_next_token(state, parser_ptr);
        if (next_token.type == TOKEN_DELIMITER && next_token.value.operator == ',') {
            continue; // 次の変数へ
        } else {
            break; // INPUT文終了
        }
    }
    
    return 0;
}

// CLEAR文の実装
int cmd_clear(basic_state_t* state, parser_state_t* parser_ptr) {
    (void)parser_ptr; // 未使用パラメータ
    
    // 変数の解放
    variable_t* var = state->variables;
    while (var) {
        variable_t* next = var->next;
        if (var->type == VAR_STRING && var->value.str.data) {
            free(var->value.str.data);
        } else if (var->type == VAR_ARRAY_NUMERIC || var->type == VAR_ARRAY_STRING) {
            if (var->value.array.data) free(var->value.array.data);
            if (var->value.array.dimensions) free(var->value.array.dimensions);
        }
        free(var);
        var = next;
    }
    state->variables = NULL;
    
    // スタックのクリア
    for_stack_entry_t* for_entry = state->for_stack;
    while (for_entry) {
        for_stack_entry_t* next = for_entry->next;
        free(for_entry);
        for_entry = next;
    }
    state->for_stack = NULL;
    
    gosub_stack_entry_t* gosub_entry = state->gosub_stack;
    while (gosub_entry) {
        gosub_stack_entry_t* next = gosub_entry->next;
        free(gosub_entry);
        gosub_entry = next;
    }
    state->gosub_stack = NULL;
    
    // データ状態のクリア
    cleanup_data_state();
    
    return 0;
}

// STOP文の実装
int cmd_stop(basic_state_t* state, parser_state_t* parser_ptr) {
    (void)parser_ptr; // 未使用パラメータ
    
    state->running = false;
    printf("BREAK IN %d\n", state->current_line ? state->current_line->line_number : 0);
    
    return 0;
}

// END文の実装
int cmd_end(basic_state_t* state, parser_state_t* parser_ptr) {
    (void)parser_ptr; // 未使用パラメータ
    
    state->running = false;
    return 0;
}

// CONT文の実装
int cmd_cont(basic_state_t* state, parser_state_t* parser_ptr) {
    (void)parser_ptr; // 未使用パラメータ
    
    if (!state->current_line) {
        set_error(state, ERR_CANT_CONTINUE, NULL);
        return -1;
    }
    
    state->running = true;
    return 0;
}

// REM文の実装
int cmd_rem(basic_state_t* state, parser_state_t* parser_ptr) {
    (void)state;      // 未使用パラメータ
    (void)parser_ptr; // 未使用パラメータ
    
    // コメント文なので何もしない
    return 0;
}

// WAIT文の実装
int cmd_wait(basic_state_t* state, parser_state_t* parser_ptr) {
    // WAIT address, mask [, xor]
    
    eval_result_t addr_result = evaluate_expression(state, parser_ptr);
    if (has_error(state) || addr_result.type != 0) {
        set_error(state, ERR_TYPE_MISMATCH, "Numeric address expected");
        return -1;
    }
    
    token_t comma1 = get_next_token(state, parser_ptr);
    if (comma1.type != TOKEN_DELIMITER || comma1.value.operator != ',') {
        set_error(state, ERR_SYNTAX, ", expected in WAIT");
        return -1;
    }
    
    eval_result_t mask_result = evaluate_expression(state, parser_ptr);
    if (has_error(state) || mask_result.type != 0) {
        set_error(state, ERR_TYPE_MISMATCH, "Numeric mask expected");
        return -1;
    }
    
    uint8_t xor_val = 0;
    token_t comma2 = get_next_token(state, parser_ptr);
    if (comma2.type == TOKEN_DELIMITER && comma2.value.operator == ',') {
        eval_result_t xor_result = evaluate_expression(state, parser_ptr);
        if (has_error(state) || xor_result.type != 0) {
            set_error(state, ERR_TYPE_MISMATCH, "Numeric XOR value expected");
            return -1;
        }
        xor_val = (uint8_t)numeric_to_double(xor_result.value.num);
    }
    
    uint16_t address = (uint16_t)numeric_to_double(addr_result.value.num);
    uint8_t mask = (uint8_t)numeric_to_double(mask_result.value.num);
    
    init_virtual_memory();
    
    // WAIT条件の実装（簡略化）
    // 実際の6502では指定アドレスの値がマスクとXORの条件を満たすまで待機
    uint8_t value = virtual_memory[address];
    if ((value ^ xor_val) & mask) {
        // 条件を満たしている場合は即座に復帰
        return 0;
    }
    
    // 簡易実装：短時間待機
    struct timespec ts = {0, 10000000}; // 10ms
    nanosleep(&ts, NULL);
    
    return 0;
}

// GET文の実装（1文字入力）
int cmd_get(basic_state_t* state, parser_state_t* parser_ptr) {
    token_t var_token = get_next_token(state, parser_ptr);
    if (var_token.type != TOKEN_VARIABLE) {
        set_error(state, ERR_SYNTAX, "Variable expected in GET");
        return -1;
    }
    
    // 1文字入力
    int ch = getchar();
    if (ch == EOF) ch = 0;
    
    // 変数への代入
    bool is_string = strchr(var_token.value.string, '$') != NULL;
    variable_type_t var_type = is_string ? VAR_STRING : VAR_NUMERIC;
    
    variable_t* var = create_variable(state, var_token.value.string, var_type);
    if (!var) {
        free(var_token.value.string);
        return -1;
    }
    
    if (is_string) {
        if (var->value.str.data) free(var->value.str.data);
        var->value.str.data = (char*)malloc(2);
        if (var->value.str.data) {
            var->value.str.data[0] = (char)ch;
            var->value.str.data[1] = '\0';
            var->value.str.length = 1;
        }
    } else {
        var->value.num = double_to_numeric((double)ch);
    }
    
    free(var_token.value.string);
    return 0;
}

// DEF文の実装（ユーザー定義関数）
int cmd_def(basic_state_t* state, parser_state_t* parser_ptr) {
    // DEF FNname(param) = expression
    // 簡易実装：現在は未対応
    (void)state;
    (void)parser_ptr;
    
    set_error(state, ERR_UNDEF_STATEMENT, "DEF statement not implemented");
    return -1;
}

// NULL文の実装
int cmd_null(basic_state_t* state, parser_state_t* parser_ptr) {
    // NULL count
    
    eval_result_t count_result = evaluate_expression(state, parser_ptr);
    if (has_error(state) || count_result.type != 0) {
        set_error(state, ERR_TYPE_MISMATCH, "Numeric count expected");
        return -1;
    }
    
    int null_count = (int)numeric_to_double(count_result.value.num);
    
    // NULL文字の出力（実際の実装では端末制御）
    for (int i = 0; i < null_count; i++) {
        putchar('\0');
    }
    
    return 0;
}

// システム情報の取得
void get_system_info(basic_state_t* state) {
    (void)state; // 未使用パラメータ
    
    printf("Microsoft BASIC M6502 v1.1 (C Port)\n");
    printf("Memory: %zu bytes free\n", (size_t)32768);
    printf("Variables: %d defined\n", count_variables(state));
}

// 変数数のカウント
int count_variables(basic_state_t* state) {
    int count = 0;
    variable_t* var = state->variables;
    while (var) {
        count++;
        var = var->next;
    }
    return count;
}
