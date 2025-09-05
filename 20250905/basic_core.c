#include "basic.h"
#include <time.h>

// 初期化
int basic_init(basic_state_t* state) {
    if (!state) return -1;
    
    // 構造体をゼロクリア
    memset(state, 0, sizeof(basic_state_t));
    
    // 初期値設定
    state->linwid = MAX_LINE_LENGTH;
    state->rnd_seed = (uint32_t)time(NULL);
    state->immediate_mode = true;
    
    return 0;
}

// クリーンアップ
void basic_cleanup(basic_state_t* state) {
    if (!state) return;
    
    // プログラム行の解放
    program_line_t* line = state->program_start;
    while (line) {
        program_line_t* next = line->next;
        if (line->text) free(line->text);
        free(line);
        line = next;
    }
    
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
    
    // スタックの解放
    for_stack_entry_t* for_entry = state->for_stack;
    while (for_entry) {
        for_stack_entry_t* next = for_entry->next;
        free(for_entry);
        for_entry = next;
    }
    
    gosub_stack_entry_t* gosub_entry = state->gosub_stack;
    while (gosub_entry) {
        gosub_stack_entry_t* next = gosub_entry->next;
        free(gosub_entry);
        gosub_entry = next;
    }
}

// エラー設定
void set_error(basic_state_t* state, error_code_t code, const char* msg) {
    if (!state) return;
    
    state->error_code = code;
    if (msg) {
        strncpy(state->error_msg, msg, sizeof(state->error_msg) - 1);
        state->error_msg[sizeof(state->error_msg) - 1] = '\0';
    } else {
        // デフォルトエラーメッセージ
        switch (code) {
            case ERR_SYNTAX:
                strcpy(state->error_msg, "SYNTAX ERROR");
                break;
            case ERR_ILLEGAL_QUANTITY:
                strcpy(state->error_msg, "ILLEGAL QUANTITY ERROR");
                break;
            case ERR_OUT_OF_MEMORY:
                strcpy(state->error_msg, "OUT OF MEMORY ERROR");
                break;
            case ERR_UNDEF_STATEMENT:
                strcpy(state->error_msg, "UNDEF'D STATEMENT ERROR");
                break;
            case ERR_UNDEF_FUNCTION:
                strcpy(state->error_msg, "UNDEF'D FUNCTION ERROR");
                break;
            case ERR_OUT_OF_DATA:
                strcpy(state->error_msg, "OUT OF DATA ERROR");
                break;
            case ERR_TYPE_MISMATCH:
                strcpy(state->error_msg, "TYPE MISMATCH ERROR");
                break;
            case ERR_STRING_TOO_LONG:
                strcpy(state->error_msg, "STRING TOO LONG ERROR");
                break;
            case ERR_FORMULA_TOO_COMPLEX:
                strcpy(state->error_msg, "FORMULA TOO COMPLEX ERROR");
                break;
            case ERR_CANT_CONTINUE:
                strcpy(state->error_msg, "CAN'T CONTINUE ERROR");
                break;
            case ERR_DIVISION_BY_ZERO:
                strcpy(state->error_msg, "DIVISION BY ZERO ERROR");
                break;
            case ERR_SUBSCRIPT_OUT_OF_RANGE:
                strcpy(state->error_msg, "SUBSCRIPT OUT OF RANGE ERROR");
                break;
            case ERR_REDIMENSIONED_ARRAY:
                strcpy(state->error_msg, "REDIMENSIONED ARRAY ERROR");
                break;
            case ERR_RETURN_WITHOUT_GOSUB:
                strcpy(state->error_msg, "RETURN WITHOUT GOSUB ERROR");
                break;
            case ERR_NEXT_WITHOUT_FOR:
                strcpy(state->error_msg, "NEXT WITHOUT FOR ERROR");
                break;
            default:
                strcpy(state->error_msg, "UNKNOWN ERROR");
                break;
        }
    }
}

// エラークリア
void clear_error(basic_state_t* state) {
    if (!state) return;
    state->error_code = ERR_NONE;
    state->error_msg[0] = '\0';
}

// エラーチェック
bool has_error(basic_state_t* state) {
    return state && state->error_code != ERR_NONE;
}

// エラー表示
void print_error(basic_state_t* state) {
    if (!state || state->error_code == ERR_NONE) return;
    
    printf("?%s", state->error_msg);
    if (state->current_line && state->current_line->line_number > 0) {
        printf(" IN %d", state->current_line->line_number);
    }
    printf("\n");
}

// 数値変換ユーティリティ
// numeric conversion helpers are implemented in utility_functions.c

// 変数検索
variable_t* find_variable(basic_state_t* state, const char* name) {
    if (!state || !name) return NULL;
    
    variable_t* var = state->variables;
    while (var) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }
    return NULL;
}

// 変数作成
variable_t* create_variable(basic_state_t* state, const char* name, variable_type_t type) {
    if (!state || !name) return NULL;
    
    // 既存変数チェック
    variable_t* existing = find_variable(state, name);
    if (existing) return existing;
    
    // 新規変数作成
    variable_t* var = (variable_t*)malloc(sizeof(variable_t));
    if (!var) {
        set_error(state, ERR_OUT_OF_MEMORY, NULL);
        return NULL;
    }
    
    memset(var, 0, sizeof(variable_t));
    strncpy(var->name, name, sizeof(var->name) - 1);
    var->type = type;
    
    // リストに追加
    var->next = state->variables;
    state->variables = var;
    
    return var;
}

// プログラム行検索
program_line_t* find_line(basic_state_t* state, uint16_t line_number) {
    if (!state) return NULL;
    
    program_line_t* line = state->program_start;
    while (line) {
        if (line->line_number == line_number) {
            return line;
        }
        if (line->line_number > line_number) {
            break; // 行番号順にソートされている前提
        }
        line = line->next;
    }
    return NULL;
}

// プログラム行追加/更新
int add_program_line(basic_state_t* state, uint16_t line_number, const char* text) {
    if (!state || !text) return -1;
    
    // 既存行の削除（行番号のみの場合）
    if (strlen(text) == 0) {
        program_line_t* prev = NULL;
        program_line_t* line = state->program_start;
        
        while (line) {
            if (line->line_number == line_number) {
                if (prev) {
                    prev->next = line->next;
                } else {
                    state->program_start = line->next;
                }
                if (line->text) free(line->text);
                free(line);
                return 0;
            }
            prev = line;
            line = line->next;
        }
        return 0; // 行が見つからなくてもエラーではない
    }
    
    // 新規行作成
    program_line_t* new_line = (program_line_t*)malloc(sizeof(program_line_t));
    if (!new_line) {
        set_error(state, ERR_OUT_OF_MEMORY, NULL);
        return -1;
    }
    
    new_line->line_number = line_number;
    new_line->length = strlen(text);
    new_line->text = (char*)malloc(new_line->length + 1);
    if (!new_line->text) {
        free(new_line);
        set_error(state, ERR_OUT_OF_MEMORY, NULL);
        return -1;
    }
    strcpy(new_line->text, text);
    
    // 適切な位置に挿入（行番号順）
    program_line_t* prev = NULL;
    program_line_t* current = state->program_start;
    
    while (current && current->line_number < line_number) {
        prev = current;
        current = current->next;
    }
    
    // 同じ行番号の既存行を置換
    if (current && current->line_number == line_number) {
        new_line->next = current->next;
        if (prev) {
            prev->next = new_line;
        } else {
            state->program_start = new_line;
        }
        if (current->text) free(current->text);
        free(current);
    } else {
        // 新規挿入
        new_line->next = current;
        if (prev) {
            prev->next = new_line;
        } else {
            state->program_start = new_line;
        }
    }
    
    return 0;
}

// プログラムリスト表示
void basic_list_program(basic_state_t* state) {
    if (!state) return;
    
    program_line_t* line = state->program_start;
    while (line) {
        printf("%d %s\n", line->line_number, line->text);
        line = line->next;
    }
}

// プログラムクリア
void basic_new_program(basic_state_t* state) {
    if (!state) return;
    
    // プログラム行の解放
    program_line_t* line = state->program_start;
    while (line) {
        program_line_t* next = line->next;
        if (line->text) free(line->text);
        free(line);
        line = next;
    }
    state->program_start = NULL;
    
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
    
    // 実行状態リセット
    state->current_line = NULL;
    state->current_position = 0;
    state->running = false;
    
    clear_error(state);
}
