#include "basic.h"

// 外部関数の宣言
extern numeric_value_t double_to_numeric(double d);
extern double numeric_to_double(numeric_value_t n);
extern variable_t* find_variable(basic_state_t* state, const char* name);
extern variable_t* create_variable(basic_state_t* state, const char* name, variable_type_t type);
extern token_t get_next_token(basic_state_t* state, parser_state_t* parser);
extern eval_result_t evaluate_expression(basic_state_t* state, parser_state_t* parser);

// データ文管理
typedef struct data_entry {
    char* value;
    struct data_entry* next;
} data_entry_t;

typedef struct {
    data_entry_t* data_list;
    data_entry_t* current_data;
} data_state_t;

static data_state_t g_data_state = {NULL, NULL};

// 配列の次元計算
uint16_t calculate_array_size(uint16_t* dimensions, uint8_t dim_count) {
    uint16_t total = 1;
    for (uint8_t i = 0; i < dim_count; i++) {
        total *= (dimensions[i] + 1); // BASICは0ベースなので+1
    }
    return total;
}

// 配列インデックスの計算
uint16_t calculate_array_index(uint16_t* dimensions, uint8_t dim_count, uint16_t* indices) {
    uint16_t index = 0;
    uint16_t multiplier = 1;
    
    for (int i = dim_count - 1; i >= 0; i--) {
        if (indices[i] > dimensions[i]) {
            return 0xFFFF; // エラー：範囲外
        }
        index += indices[i] * multiplier;
        multiplier *= (dimensions[i] + 1);
    }
    
    return index;
}

// DIM文の実装
int cmd_dim(basic_state_t* state, parser_state_t* parser_ptr) {
    // DIM var(dim1, dim2, ...), var2(dim1, dim2, ...), ...
    
    while (true) {
        // 変数名の取得
        token_t var_token = get_next_token(state, parser_ptr);
        if (var_token.type != TOKEN_VARIABLE) {
            set_error(state, ERR_SYNTAX, "Variable name expected in DIM");
            return -1;
        }
        
        // 既存変数チェック
        // 既存変数チェック（同名が存在すればエラー）
        variable_t* existing_var = find_variable(state, var_token.value.string); if (existing_var) { set_error(state, ERR_REDIMENSIONED_ARRAY, NULL); free(var_token.value.string); return -1; }
        token_t open_paren = get_next_token(state, parser_ptr);
        if (open_paren.type != TOKEN_DELIMITER || open_paren.value.operator != '(') {
            set_error(state, ERR_SYNTAX, "( expected in DIM");
            free(var_token.value.string);
            return -1;
        }
        
        // 次元の読み取り
        uint16_t dimensions[MAX_ARRAY_DIMENSIONS];
        uint8_t dim_count = 0;
        
        while (dim_count < MAX_ARRAY_DIMENSIONS) {
            eval_result_t dim_result = evaluate_expression(state, parser_ptr);
            if (has_error(state) || dim_result.type != 0) {
                set_error(state, ERR_TYPE_MISMATCH, "Numeric dimension expected");
                free(var_token.value.string);
                return -1;
            }
            
            int dim_val = (int)numeric_to_double(dim_result.value.num);
            if (dim_val < 0) {
                set_error(state, ERR_ILLEGAL_QUANTITY, "Negative dimension");
                free(var_token.value.string);
                return -1;
            }
            
            dimensions[dim_count++] = (uint16_t)dim_val;
            
            token_t next_token = get_next_token(state, parser_ptr);
            if (next_token.type == TOKEN_DELIMITER && next_token.value.operator == ',') {
                continue; // 次の次元へ
            } else if (next_token.type == TOKEN_DELIMITER && next_token.value.operator == ')') {
                break; // 次元定義終了
            } else {
                set_error(state, ERR_SYNTAX, ", or ) expected in DIM");
                free(var_token.value.string);
                return -1;
            }
        }
        
        if (dim_count == 0) {
            set_error(state, ERR_SYNTAX, "At least one dimension required");
            free(var_token.value.string);
            return -1;
        }
        
        // 配列変数の作成
        bool is_string = strchr(var_token.value.string, '$') != NULL;
        variable_type_t var_type = is_string ? VAR_ARRAY_STRING : VAR_ARRAY_NUMERIC;
        
        variable_t* array_var = create_variable(state, var_token.value.string, var_type);
        if (!array_var) {
            free(var_token.value.string);
            return -1;
        }
        
        // 配列データの割り当て
        uint16_t total_elements = calculate_array_size(dimensions, dim_count);
        
        array_var->value.array.dimensions = (uint16_t*)malloc(dim_count * sizeof(uint16_t));
        if (!array_var->value.array.dimensions) {
            set_error(state, ERR_OUT_OF_MEMORY, NULL);
            free(var_token.value.string);
            return -1;
        }
        
        memcpy(array_var->value.array.dimensions, dimensions, dim_count * sizeof(uint16_t));
        array_var->value.array.dim_count = dim_count;
        array_var->value.array.total_elements = total_elements;
        
        if (is_string) {
            // 文字列配列
            char** string_array = (char**)malloc(total_elements * sizeof(char*));
            if (!string_array) {
                set_error(state, ERR_OUT_OF_MEMORY, NULL);
                free(array_var->value.array.dimensions);
                free(var_token.value.string);
                return -1;
            }
            
            // 空文字列で初期化
            for (uint16_t i = 0; i < total_elements; i++) {
                string_array[i] = (char*)malloc(1);
                if (string_array[i]) {
                    string_array[i][0] = '\0';
                }
            }
            
            array_var->value.array.data = string_array;
        } else {
            // 数値配列
            numeric_value_t* numeric_array = (numeric_value_t*)malloc(total_elements * sizeof(numeric_value_t));
            if (!numeric_array) {
                set_error(state, ERR_OUT_OF_MEMORY, NULL);
                free(array_var->value.array.dimensions);
                free(var_token.value.string);
                return -1;
            }
            
            // 0で初期化
            for (uint16_t i = 0; i < total_elements; i++) {
                numeric_array[i] = double_to_numeric(0.0);
            }
            
            array_var->value.array.data = numeric_array;
        }
        
        free(var_token.value.string);
        
        // 次の変数があるかチェック
        token_t next_token = get_next_token(state, parser_ptr);
        if (next_token.type == TOKEN_DELIMITER && next_token.value.operator == ',') {
            continue; // 次の変数へ
        } else {
            break; // DIM文終了
        }
    }
    
    return 0;
}

// 配列要素のアクセス
eval_result_t access_array_element(basic_state_t* state, const char* var_name, 
                                  uint16_t* indices, uint8_t index_count) {
    eval_result_t result = {0};
    
    variable_t* var = find_variable(state, var_name);
    if (!var || (var->type != VAR_ARRAY_NUMERIC && var->type != VAR_ARRAY_STRING)) {
        set_error(state, ERR_UNDEF_STATEMENT, "Array not found");
        return result;
    }
    
    if (index_count != var->value.array.dim_count) {
        set_error(state, ERR_SYNTAX, "Wrong number of dimensions");
        return result;
    }
    
    uint16_t array_index = calculate_array_index(var->value.array.dimensions, 
                                                var->value.array.dim_count, indices);
    if (array_index == 0xFFFF) {
        set_error(state, ERR_SUBSCRIPT_OUT_OF_RANGE, NULL);
        return result;
    }
    
    if (var->type == VAR_ARRAY_NUMERIC) {
        result.type = 0; // 数値
        numeric_value_t* numeric_array = (numeric_value_t*)var->value.array.data;
        result.value.num = numeric_array[array_index];
    } else {
        result.type = 1; // 文字列
        char** string_array = (char**)var->value.array.data;
        result.value.str.data = safe_string_dup(string_array[array_index], MAX_STRING_LENGTH);
        result.value.str.length = result.value.str.data ? strlen(result.value.str.data) : 0;
    }
    
    return result;
}

// 配列要素への代入
int assign_array_element(basic_state_t* state, const char* var_name, 
                        uint16_t* indices, uint8_t index_count, eval_result_t value) {
    variable_t* var = find_variable(state, var_name);
    if (!var || (var->type != VAR_ARRAY_NUMERIC && var->type != VAR_ARRAY_STRING)) {
        set_error(state, ERR_UNDEF_STATEMENT, "Array not found");
        return -1;
    }
    
    if (index_count != var->value.array.dim_count) {
        set_error(state, ERR_SYNTAX, "Wrong number of dimensions");
        return -1;
    }
    
    uint16_t array_index = calculate_array_index(var->value.array.dimensions, 
                                                var->value.array.dim_count, indices);
    if (array_index == 0xFFFF) {
        set_error(state, ERR_SUBSCRIPT_OUT_OF_RANGE, NULL);
        return -1;
    }
    
    if (var->type == VAR_ARRAY_NUMERIC) {
        if (value.type != 0) {
            set_error(state, ERR_TYPE_MISMATCH, NULL);
            return -1;
        }
        numeric_value_t* numeric_array = (numeric_value_t*)var->value.array.data;
        numeric_array[array_index] = value.value.num;
    } else {
        if (value.type != 1) {
            set_error(state, ERR_TYPE_MISMATCH, NULL);
            return -1;
        }
        char** string_array = (char**)var->value.array.data;
        if (string_array[array_index]) {
            free(string_array[array_index]);
        }
        string_array[array_index] = value.value.str.data;
    }
    
    return 0;
}

// DATA文の実装
int cmd_data(basic_state_t* state, parser_state_t* parser_ptr) {
    // DATA value1, value2, value3, ...
    
    while (true) {
        token_t value_token = get_next_token(state, parser_ptr);
        
        char* data_value = NULL;
        if (value_token.type == TOKEN_STRING) {
            data_value = value_token.value.string;
        } else if (value_token.type == TOKEN_NUMBER) {
            data_value = number_to_string(value_token.value.number);
        } else if (value_token.type == TOKEN_VARIABLE) {
            data_value = value_token.value.string;
        } else {
            break; // DATA文終了
        }
        
        // データエントリを作成
        data_entry_t* entry = (data_entry_t*)malloc(sizeof(data_entry_t));
        if (!entry) {
            set_error(state, ERR_OUT_OF_MEMORY, NULL);
            if (data_value) free(data_value);
            return -1;
        }
        
        entry->value = data_value;
        entry->next = NULL;
        
        // データリストに追加
        if (!g_data_state.data_list) {
            g_data_state.data_list = entry;
            g_data_state.current_data = entry;
        } else {
            data_entry_t* last = g_data_state.data_list;
            while (last->next) {
                last = last->next;
            }
            last->next = entry;
        }
        
        // 次のトークンをチェック
        token_t next_token = get_next_token(state, parser_ptr);
        if (next_token.type == TOKEN_DELIMITER && next_token.value.operator == ',') {
            continue; // 次のデータへ
        } else {
            break; // DATA文終了
        }
    }
    
    return 0;
}

// READ文の実装
int cmd_read(basic_state_t* state, parser_state_t* parser_ptr) {
    // READ var1, var2, var3, ...
    
    while (true) {
        token_t var_token = get_next_token(state, parser_ptr);
        if (var_token.type != TOKEN_VARIABLE) {
            set_error(state, ERR_SYNTAX, "Variable expected in READ");
            return -1;
        }
        
        // データの取得
        if (!g_data_state.current_data) {
            set_error(state, ERR_OUT_OF_DATA, NULL);
            free(var_token.value.string);
            return -1;
        }
        
        // 変数の作成または取得
        bool is_string = strchr(var_token.value.string, '$') != NULL;
        variable_type_t var_type = is_string ? VAR_STRING : VAR_NUMERIC;
        
        variable_t* var = create_variable(state, var_token.value.string, var_type);
        if (!var) {
            free(var_token.value.string);
            return -1;
        }
        
        // データの代入
        if (is_string) {
            if (var->value.str.data) {
                free(var->value.str.data);
            }
            var->value.str.data = safe_string_dup(g_data_state.current_data->value, MAX_STRING_LENGTH);
            var->value.str.length = var->value.str.data ? strlen(var->value.str.data) : 0;
        } else {
            var->value.num = string_to_number(g_data_state.current_data->value);
        }
        
        // 次のデータに進む
        g_data_state.current_data = g_data_state.current_data->next;
        
        free(var_token.value.string);
        
        // 次の変数があるかチェック
        token_t next_token = get_next_token(state, parser_ptr);
        if (next_token.type == TOKEN_DELIMITER && next_token.value.operator == ',') {
            continue; // 次の変数へ
        } else {
            break; // READ文終了
        }
    }
    
    return 0;
}

// RESTORE文の実装
int cmd_restore(basic_state_t* state, parser_state_t* parser_ptr) {
    (void)parser_ptr; // 未使用パラメータ
    (void)state;      // 未使用パラメータ
    
    // データポインターをリセット
    g_data_state.current_data = g_data_state.data_list;
    
    return 0;
}

// データ状態のクリーンアップ
void cleanup_data_state(void) {
    data_entry_t* current = g_data_state.data_list;
    while (current) {
        data_entry_t* next = current->next;
        if (current->value) {
            free(current->value);
        }
        free(current);
        current = next;
    }
    
    g_data_state.data_list = NULL;
    g_data_state.current_data = NULL;
}



