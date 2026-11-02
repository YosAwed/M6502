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

// 配列要素数を16ビットで表せる場合のみ返す。0はサイズ超過を示す。
uint16_t calculate_array_size(uint16_t* dimensions, uint8_t dim_count) {
    size_t total = 1;
    for (uint8_t i = 0; i < dim_count; i++) {
        size_t extent = (size_t)dimensions[i] + 1;
        if (total > UINT16_MAX / extent) return 0;
        total *= extent;
    }
    return (uint16_t)total;
}

// 配列の線形添字を計算する。UINT16_MAXは範囲外を示す。
uint16_t calculate_array_index(uint16_t* dimensions, uint8_t dim_count, uint16_t* indices) {
    size_t index = 0;
    size_t multiplier = 1;
    for (int i = (int)dim_count - 1; i >= 0; i--) {
        if (indices[i] > dimensions[i]) return UINT16_MAX;
        index += (size_t)indices[i] * multiplier;
        multiplier *= (size_t)dimensions[i] + 1;
    }
    return index >= UINT16_MAX ? UINT16_MAX : (uint16_t)index;
}

// DIMで確保した一時データを登録前に解放する。
static void free_array_storage(void* data, bool is_string, uint16_t count) {
    if (!data) return;
    if (is_string) {
        char** values = (char**)data;
        for (uint16_t i = 0; i < count; i++) free(values[i]);
    }
    free(data);
}

// DIM文の実装
int cmd_dim(basic_state_t* state, parser_state_t* parser_ptr) {
    while (true) {
        token_t var_token = get_next_token(state, parser_ptr);
        if (var_token.type != TOKEN_VARIABLE) {
            set_error(state, ERR_SYNTAX, "Variable name expected in DIM");
            return -1;
        }
        if (find_variable(state, var_token.value.string)) {
            set_error(state, ERR_REDIMENSIONED_ARRAY, NULL);
            free(var_token.value.string);
            return -1;
        }
        token_t open_paren = get_next_token(state, parser_ptr);
        if (open_paren.type != TOKEN_DELIMITER || open_paren.value.operator != '(') {
            set_error(state, ERR_SYNTAX, "( expected in DIM");
            free(var_token.value.string);
            return -1;
        }

        uint16_t dimensions[MAX_ARRAY_DIMENSIONS];
        uint8_t dim_count = 0;
        bool closed = false;
        while (dim_count < MAX_ARRAY_DIMENSIONS) {
            eval_result_t dim_result = evaluate_expression(state, parser_ptr);
            if (has_error(state) || dim_result.type != 0) {
                set_error(state, ERR_TYPE_MISMATCH, "Numeric dimension expected");
                free(var_token.value.string);
                return -1;
            }
            double dimension = numeric_to_double(dim_result.value.num);
            if (!isfinite(dimension) || dimension < 0.0 ||
                dimension >= UINT16_MAX) {
                set_error(state, ERR_ILLEGAL_QUANTITY, "Invalid array dimension");
                free(var_token.value.string);
                return -1;
            }
            dimensions[dim_count++] = (uint16_t)dimension;

            token_t next_token = get_next_token(state, parser_ptr);
            if (next_token.type == TOKEN_DELIMITER && next_token.value.operator == ')') {
                closed = true;
                break;
            }
            if (next_token.type != TOKEN_DELIMITER || next_token.value.operator != ',') {
                set_error(state, ERR_SYNTAX, ", or ) expected in DIM");
                free(var_token.value.string);
                return -1;
            }
        }
        if (!closed) {
            set_error(state, ERR_SYNTAX, "Too many array dimensions");
            free(var_token.value.string);
            return -1;
        }

        uint16_t total_elements = calculate_array_size(dimensions, dim_count);
        if (total_elements == 0) {
            set_error(state, ERR_ILLEGAL_QUANTITY, "Array too large");
            free(var_token.value.string);
            return -1;
        }

        bool is_string = strchr(var_token.value.string, '$') != NULL;
        variable_type_t var_type = is_string ? VAR_ARRAY_STRING : VAR_ARRAY_NUMERIC;
        uint16_t* dims_copy = malloc(dim_count * sizeof(*dims_copy));
        if (!dims_copy) {
            set_error(state, ERR_OUT_OF_MEMORY, NULL);
            free(var_token.value.string);
            return -1;
        }
        memcpy(dims_copy, dimensions, dim_count * sizeof(*dims_copy));

        void* data = NULL;
        if (is_string) {
            char** values = calloc(total_elements, sizeof(*values));
            if (values) {
                data = values;
                for (uint16_t i = 0; i < total_elements; i++) {
                    values[i] = malloc(1);
                    if (!values[i]) break;
                    values[i][0] = '\0';
                }
                for (uint16_t i = 0; i < total_elements; i++) {
                    if (!values[i]) {
                        free_array_storage(data, true, total_elements);
                        data = NULL;
                        break;
                    }
                }
            }
        } else {
            numeric_value_t* values = malloc((size_t)total_elements * sizeof(*values));
            if (values) {
                data = values;
                for (uint16_t i = 0; i < total_elements; i++)
                    values[i] = double_to_numeric(0.0);
            }
        }
        if (!data) {
            free(dims_copy);
            free(var_token.value.string);
            set_error(state, ERR_OUT_OF_MEMORY, NULL);
            return -1;
        }

        // すべての確保に成功してから変数リストに登録する。
        variable_t* array_var = create_variable(state, var_token.value.string, var_type);
        if (!array_var) {
            free_array_storage(data, is_string, total_elements);
            free(dims_copy);
            free(var_token.value.string);
            return -1;
        }
        array_var->value.array.dimensions = dims_copy;
        array_var->value.array.dim_count = dim_count;
        array_var->value.array.total_elements = total_elements;
        array_var->value.array.data = data;
        free(var_token.value.string);

        token_t next_token = get_next_token(state, parser_ptr);
        if (next_token.type != TOKEN_DELIMITER || next_token.value.operator != ',')
            break;
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
    if (array_index == UINT16_MAX || array_index >= var->value.array.total_elements) {
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
    if (array_index == UINT16_MAX || array_index >= var->value.array.total_elements) {
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



