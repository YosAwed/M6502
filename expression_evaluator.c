#include "basic.h"

// 外部関数の宣言
extern numeric_value_t double_to_numeric(double d);
extern double numeric_to_double(numeric_value_t n);
extern variable_t* find_variable(basic_state_t* state, const char* name);
extern variable_t* create_variable(basic_state_t* state, const char* name, variable_type_t type);
extern token_t get_next_token(basic_state_t* state, parser_state_t* parser);

// 数学関数の宣言
extern numeric_value_t func_sgn(numeric_value_t x);
extern numeric_value_t func_int(numeric_value_t x);
extern numeric_value_t func_abs(numeric_value_t x);
extern numeric_value_t func_sqr(numeric_value_t x);
extern numeric_value_t func_exp(numeric_value_t x);
extern numeric_value_t func_log(numeric_value_t x);
extern numeric_value_t func_sin(numeric_value_t x);
extern numeric_value_t func_cos(numeric_value_t x);
extern numeric_value_t func_tan(numeric_value_t x);
extern numeric_value_t func_atn(numeric_value_t x);
extern numeric_value_t func_rnd(basic_state_t* state, numeric_value_t x);
extern numeric_value_t func_fre(numeric_value_t x);
extern numeric_value_t func_pos(numeric_value_t x);
extern numeric_value_t func_peek(uint16_t address);

// 文字列関数の宣言
extern eval_result_t func_len(const char* str);
extern eval_result_t func_asc(const char* str);
extern eval_result_t func_chr(int ascii_code);
extern eval_result_t func_str(numeric_value_t num);
extern eval_result_t func_val(const char* str);
extern eval_result_t func_left(const char* str, int n);
extern eval_result_t func_right(const char* str, int n);
extern eval_result_t func_mid(const char* str, int start, int len);

// 算術演算の宣言
extern numeric_value_t math_add(numeric_value_t a, numeric_value_t b);
extern numeric_value_t math_subtract(numeric_value_t a, numeric_value_t b);
extern numeric_value_t math_multiply(numeric_value_t a, numeric_value_t b);
extern numeric_value_t math_divide(numeric_value_t a, numeric_value_t b);
extern numeric_value_t math_power(numeric_value_t base, numeric_value_t exponent);
extern numeric_value_t math_negate(numeric_value_t a);
extern numeric_value_t math_and(numeric_value_t a, numeric_value_t b);
extern numeric_value_t math_or(numeric_value_t a, numeric_value_t b);
extern numeric_value_t math_not(numeric_value_t a);

// 比較演算の宣言
extern int math_equal(numeric_value_t a, numeric_value_t b);
extern int math_less_than(numeric_value_t a, numeric_value_t b);
extern int math_greater_than(numeric_value_t a, numeric_value_t b);
extern int math_less_equal(numeric_value_t a, numeric_value_t b);
extern int math_greater_equal(numeric_value_t a, numeric_value_t b);
extern int math_not_equal(numeric_value_t a, numeric_value_t b);

// 文字列比較の宣言
extern int string_equal(const char* str1, const char* str2);
extern int string_less_than(const char* str1, const char* str2);
extern int string_greater_than(const char* str1, const char* str2);
extern int string_less_equal(const char* str1, const char* str2);
extern int string_greater_equal(const char* str1, const char* str2);
extern int string_not_equal(const char* str1, const char* str2);

// 文字列連結の宣言
extern eval_result_t string_concatenate(const char* str1, const char* str2);

// 演算子優先度テーブル
typedef struct {
    char operator;
    uint8_t precedence;
    bool right_associative;
} operator_info_t;

static const operator_info_t operators[] = {
    {'^', 127, true},   // べき乗（右結合）
    {'*', 123, false},  // 乗算
    {'/', 123, false},  // 除算
    {'+', 121, false},  // 加算
    {'-', 121, false},  // 減算
    {'=', 100, false},  // 等しい
    {'<', 100, false},  // より小さい
    {'>', 100, false},  // より大きい
    {'&', 80, false},   // AND
    {'|', 70, false},   // OR
    {0, 0, false}       // 終端
};

// 演算子情報の取得
const operator_info_t* get_operator_info(char op) {
    for (int i = 0; operators[i].operator != 0; i++) {
        if (operators[i].operator == op) {
            return &operators[i];
        }
    }
    return NULL;
}

// Rewind parser to a prior token start position
static void parser_rewind(parser_state_t* parser, uint16_t pos) {
    if (!parser) return;
    if (pos > parser->length) pos = parser->length;
    parser->position = pos;
    parser->current_char = (parser->position < parser->length)
        ? parser->text[parser->position]
        : '\0';
}

// 前方宣言
eval_result_t evaluate_expression_with_precedence(basic_state_t* state, parser_state_t* parser_ptr, uint8_t min_precedence);
eval_result_t evaluate_primary(basic_state_t* state, parser_state_t* parser_ptr);

// 式評価のメイン関数
eval_result_t evaluate_expression(basic_state_t* state, parser_state_t* parser_ptr) {
    return evaluate_expression_with_precedence(state, parser_ptr, 0);
}

// 優先度を考慮した式評価（演算子優先順位解析）
eval_result_t evaluate_expression_with_precedence(basic_state_t* state, parser_state_t* parser_ptr, uint8_t min_precedence) {
    eval_result_t left = evaluate_primary(state, parser_ptr);
    if (has_error(state)) return left;

    while (true) {
        uint16_t save_pos = parser_ptr->position;
        token_t op_token = get_next_token(state, parser_ptr);

        // Determine operator kind (supports combined <= >= <>, AND/OR keywords)
        bool is_combined = false; // <=, >=, <>
        enum { OP_NONE, OP_LE, OP_GE, OP_NE } comb = OP_NONE;
        char effective_op = 0; // '+', '-', '*', '/', '^', '=', '<', '>', '&', '|'
        uint8_t op_prec = 0;
        bool right_assoc = false;

        if (op_token.type == TOKEN_OPERATOR) {
            const char* txt = parser_ptr->text;
            uint16_t pos0 = save_pos;
            char c1 = (pos0 < parser_ptr->length) ? txt[pos0] : '\0';
            char c2 = (pos0 + 1 < parser_ptr->length) ? txt[pos0 + 1] : '\0';
            if (c1 == '<' && c2 == '=') { is_combined = true; comb = OP_LE; op_prec = 100; }
            else if (c1 == '>' && c2 == '=') { is_combined = true; comb = OP_GE; op_prec = 100; }
            else if (c1 == '<' && c2 == '>') { is_combined = true; comb = OP_NE; op_prec = 100; }
            else {
                effective_op = op_token.value.operator;
                const operator_info_t* op_info = get_operator_info(effective_op);
                if (!op_info) { parser_ptr->position = save_pos; parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0'; break; }
                op_prec = op_info->precedence;
                right_assoc = op_info->right_associative;
            }
        } else if (op_token.type == TOKEN_KEYWORD && (op_token.value.keyword_id == 0xA9 || op_token.value.keyword_id == 0xAA)) {
            // AND / OR
            effective_op = (op_token.value.keyword_id == 0xA9) ? '&' : '|';
            const operator_info_t* op_info = get_operator_info(effective_op);
            if (!op_info) { parser_ptr->position = save_pos; parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0'; break; }
            op_prec = op_info->precedence;
            right_assoc = op_info->right_associative;
        } else {
            parser_ptr->position = save_pos;
            parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
            break;
        }

        if (op_prec < min_precedence) {
            parser_ptr->position = save_pos;
            parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
            break;
        }

        uint8_t next_min_precedence = op_prec;
        if (!right_assoc) {
            next_min_precedence++;
        }

        eval_result_t right = evaluate_expression_with_precedence(state, parser_ptr, next_min_precedence);
        if (has_error(state)) return right;

        if (is_combined) {
            eval_result_t res = {0};
            res.type = 0;
            if (left.type == 0 && right.type == 0) {
                switch (comb) {
                    case OP_LE: res.value.num = double_to_numeric(math_less_equal(left.value.num, right.value.num)); break;
                    case OP_GE: res.value.num = double_to_numeric(math_greater_equal(left.value.num, right.value.num)); break;
                    case OP_NE: res.value.num = double_to_numeric(math_not_equal(left.value.num, right.value.num)); break;
                    default: break;
                }
            } else if (left.type == 1 && right.type == 1) {
                switch (comb) {
                    case OP_LE: res.value.num = double_to_numeric(string_less_equal(left.value.str.data, right.value.str.data)); break;
                    case OP_GE: res.value.num = double_to_numeric(string_greater_equal(left.value.str.data, right.value.str.data)); break;
                    case OP_NE: res.value.num = double_to_numeric(string_not_equal(left.value.str.data, right.value.str.data)); break;
                    default: break;
                }
            } else {
                set_error(state, ERR_TYPE_MISMATCH, "Type mismatch in comparison");
                // free any strings
                if (left.type == 1 && left.value.str.data) free(left.value.str.data);
                if (right.type == 1 && right.value.str.data) free(right.value.str.data);
                return res;
            }
            // clean up any string operands as perform_operation would
            if (left.type == 1 && left.value.str.data) free(left.value.str.data);
            if (right.type == 1 && right.value.str.data) free(right.value.str.data);
            left = res;
            if (has_error(state)) return left;
        } else if (effective_op == '&' || effective_op == '|') {
            // Bitwise AND/OR (numeric only)
            if (left.type != 0 || right.type != 0) {
                set_error(state, ERR_TYPE_MISMATCH, "AND/OR require numeric operands");
                // free any strings
                if (left.type == 1 && left.value.str.data) free(left.value.str.data);
                if (right.type == 1 && right.value.str.data) free(right.value.str.data);
                return left;
            }
            eval_result_t res = {0};
            res.type = 0;
            if (effective_op == '&') res.value.num = math_and(left.value.num, right.value.num);
            else res.value.num = math_or(left.value.num, right.value.num);
            left = res;
        } else {
            left = perform_operation(state, left, effective_op, right);
            if (has_error(state)) return left;
        }
    }

    return left;
}

// 基本項の評価
eval_result_t evaluate_primary(basic_state_t* state, parser_state_t* parser_ptr) {
    eval_result_t result = {0};
    
    token_t token = get_next_token(state, parser_ptr);
    
    switch (token.type) {
        case TOKEN_NUMBER:
            result.type = 0; // 数値
            result.value.num = token.value.number;
            break;
            
        case TOKEN_STRING:
            result.type = 1; // 文字列
            result.value.str.data = token.value.string;
            result.value.str.length = strlen(token.value.string);
            break;
            
        case TOKEN_VARIABLE:
            result = evaluate_variable(state, parser_ptr, token.value.string);
            free(token.value.string);
            break;
            
        case TOKEN_KEYWORD:
            if (token.value.keyword_id == 0xA2) { // NOT (prefix, precedence ~90)
                eval_result_t rhs = evaluate_expression_with_precedence(state, parser_ptr, 90);
                if (has_error(state)) return rhs;
                if (rhs.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "NOT requires numeric operand"); return rhs; }
                result.type = 0;
                result.value.num = math_not(rhs.value.num);
            } else {
                result = evaluate_function(state, parser_ptr, token.value.keyword_id);
            }
            break;
            
        case TOKEN_OPERATOR:
            if (token.value.operator == '-') {
                // 単項マイナス
                eval_result_t operand = evaluate_primary(state, parser_ptr);
                if (has_error(state)) return operand;
                
                if (operand.type == 0) {
                    result.type = 0;
                    result.value.num = math_negate(operand.value.num);
                } else {
                    set_error(state, ERR_TYPE_MISMATCH, "Cannot negate string");
                }
            } else if (token.value.operator == '+') {
                // 単項プラス
                result = evaluate_primary(state, parser_ptr);
            } else {
                set_error(state, ERR_SYNTAX, "Unexpected operator");
            }
            break;
            
        case TOKEN_DELIMITER:
            if (token.value.operator == '(') {
                // 括弧で囲まれた式
                result = evaluate_expression(state, parser_ptr);
                if (has_error(state)) return result;
                
                token_t close_paren = get_next_token(state, parser_ptr);
                if (close_paren.type != TOKEN_DELIMITER || close_paren.value.operator != ')') {
                    set_error(state, ERR_SYNTAX, ") expected");
                }
            } else {
                set_error(state, ERR_SYNTAX, "Unexpected delimiter");
            }
            break;
            
        default:
            set_error(state, ERR_SYNTAX, "Unexpected token in expression");
            break;
    }
    
    return result;
}

// 変数の評価
eval_result_t evaluate_variable(basic_state_t* state, parser_state_t* parser_ptr, const char* var_name) {
    eval_result_t result = {0};
    
        uint16_t save_pos = parser_ptr->position;
    token_t next_token = get_next_token(state, parser_ptr);
    if (next_token.type == TOKEN_DELIMITER && next_token.value.operator == '(') {
        // 配列アクセス
        uint16_t indices[MAX_ARRAY_DIMENSIONS];
        uint8_t index_count = 0;
        
        while (index_count < MAX_ARRAY_DIMENSIONS) {
            eval_result_t index_result = evaluate_expression(state, parser_ptr);
            if (has_error(state) || index_result.type != 0) {
                set_error(state, ERR_TYPE_MISMATCH, "Numeric index expected");
                return result;
            }
            
            indices[index_count++] = (uint16_t)numeric_to_double(index_result.value.num);
            
            token_t sep_token = get_next_token(state, parser_ptr);
            if (sep_token.type == TOKEN_DELIMITER && sep_token.value.operator == ',') {
                continue; // 次のインデックスへ
            } else if (sep_token.type == TOKEN_DELIMITER && sep_token.value.operator == ')') {
                break; // インデックス終了
            } else {
                set_error(state, ERR_SYNTAX, ", or ) expected in array access");
                return result;
            }
        }
        
        result = access_array_element(state, var_name, indices, index_count);
    } else {
        // not an array access; rewind so caller sees following token
        parser_ptr->position = save_pos;
        parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
        // \P+
        variable_t* var = find_variable(state, var_name);
        if (!var) {
            // 未定義変数は0または空文字列として扱う
            bool is_string = strchr(var_name, '$') != NULL;
            if (is_string) {
                result.type = 1;
                result.value.str.data = (char*)malloc(1);
                if (result.value.str.data) {
                    result.value.str.data[0] = '\0';
                    result.value.str.length = 0;
                }
            } else {
                result.type = 0;
                result.value.num = double_to_numeric(0.0);
            }
        } else {
            if (var->type == VAR_NUMERIC) {
                result.type = 0;
                result.value.num = var->value.num;
            } else if (var->type == VAR_STRING) {
                result.type = 1;
                result.value.str.data = safe_string_dup(var->value.str.data, MAX_STRING_LENGTH);
                result.value.str.length = result.value.str.data ? strlen(result.value.str.data) : 0;
            } else {
                set_error(state, ERR_TYPE_MISMATCH, "Invalid variable type");
            }
        }
    }
    
    return result;
}

// 関数の評価
eval_result_t evaluate_function(basic_state_t* state, parser_state_t* parser_ptr, uint8_t function_id) {
    eval_result_t result = {0};
    
    // 開き括弧
    token_t open_paren = get_next_token(state, parser_ptr);
    if (open_paren.type != TOKEN_DELIMITER || open_paren.value.operator != '(') {
        set_error(state, ERR_SYNTAX, "( expected after function name");
        return result;
    }
    
    switch (function_id) {
        case 0xAE: // SGN
        case 0xAF: // INT
        case 0xB0: // ABS
        case 0xB4: // SQR
        case 0xB6: // LOG
        case 0xB7: // EXP
        case 0xB8: // COS
        case 0xB9: // SIN
        case 0xBA: // TAN
        case 0xBB: // ATN
        case 0xB5: // RND
        {
            eval_result_t arg = evaluate_expression(state, parser_ptr);
            if (has_error(state) || arg.type != 0) {
                set_error(state, ERR_TYPE_MISMATCH, "Numeric argument expected");
                return result;
            }
            
            result.type = 0;
            switch (function_id) {
                case 0xAE: result.value.num = func_sgn(arg.value.num); break;
                case 0xAF: result.value.num = func_int(arg.value.num); break;
                case 0xB0: result.value.num = func_abs(arg.value.num); break;
                case 0xB4: result.value.num = func_sqr(arg.value.num); break;
                case 0xB6: result.value.num = func_log(arg.value.num); break;
                case 0xB7: result.value.num = func_exp(arg.value.num); break;
                case 0xB8: result.value.num = func_cos(arg.value.num); break;
                case 0xB9: result.value.num = func_sin(arg.value.num); break;
                case 0xBA: result.value.num = func_tan(arg.value.num); break;
                case 0xBB: result.value.num = func_atn(arg.value.num); break;
                case 0xB5: result.value.num = func_rnd(state, arg.value.num); break;
            }
            break;
        }
        
        case 0xBD: // LEN
        case 0xC0: // ASC
        case 0xBF: // VAL
        {
            eval_result_t arg = evaluate_expression(state, parser_ptr);
            if (has_error(state) || arg.type != 1) {
                set_error(state, ERR_TYPE_MISMATCH, "String argument expected");
                return result;
            }
            
            switch (function_id) {
                case 0xBD: result = func_len(arg.value.str.data); break;
                case 0xC0: result = func_asc(arg.value.str.data); break;
                case 0xBF: result = func_val(arg.value.str.data); break;
            }
            
            if (arg.value.str.data) free(arg.value.str.data);
            break;
        }
        
        case 0xC1: // CHR$
        case 0xBE: // STR$
        {
            eval_result_t arg = evaluate_expression(state, parser_ptr);
            if (has_error(state) || arg.type != 0) {
                set_error(state, ERR_TYPE_MISMATCH, "Numeric argument expected");
                return result;
            }
            
            switch (function_id) {
                case 0xC1: result = func_chr((int)numeric_to_double(arg.value.num)); break;
                case 0xBE: result = func_str(arg.value.num); break;
            }
            break;
        }

        case 0xC2: // LEFT$
        case 0xC3: // RIGHT$
        case 0xC4: // MID$
        {
            // String first argument
            eval_result_t s = evaluate_expression(state, parser_ptr);
            if (has_error(state) || s.type != 1) { set_error(state, ERR_TYPE_MISMATCH, "String argument expected"); return result; }
            // Comma
            token_t comma = get_next_token(state, parser_ptr);
            if (comma.type != TOKEN_DELIMITER || comma.value.operator != ',') { set_error(state, ERR_SYNTAX, ", expected"); if (s.value.str.data) free(s.value.str.data); return result; }
            // Numeric parameter(s)
            eval_result_t p1 = evaluate_expression(state, parser_ptr);
            if (has_error(state) || p1.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric argument expected"); if (s.value.str.data) free(s.value.str.data); return result; }
            if (function_id == 0xC4) {
                // MID$(s$, start, len)
                token_t comma2 = get_next_token(state, parser_ptr);
                if (comma2.type != TOKEN_DELIMITER || comma2.value.operator != ',') { set_error(state, ERR_SYNTAX, ", expected"); if (s.value.str.data) free(s.value.str.data); return result; }
                eval_result_t p2 = evaluate_expression(state, parser_ptr);
                if (has_error(state) || p2.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric argument expected"); if (s.value.str.data) free(s.value.str.data); return result; }
                int start = (int)numeric_to_double(p1.value.num);
                int len = (int)numeric_to_double(p2.value.num);
                result = func_mid(s.value.str.data, start, len);
            } else if (function_id == 0xC2) {
                int n = (int)numeric_to_double(p1.value.num);
                result = func_left(s.value.str.data, n);
            } else { // RIGHT$
                int n = (int)numeric_to_double(p1.value.num);
                result = func_right(s.value.str.data, n);
            }
            if (s.value.str.data) free(s.value.str.data);
            break;
        }

        case 0xBC: // PEEK
        case 0xB2: // FRE
        case 0xB3: // POS
        {
            eval_result_t arg = evaluate_expression(state, parser_ptr);
            if (has_error(state) || arg.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric argument expected"); return result; }
            result.type = 0;
            switch (function_id) {
                case 0xBC: {
                    uint16_t addr = (uint16_t)numeric_to_double(arg.value.num);
                    result.value.num = func_peek(addr);
                    break;
                }
                case 0xB2: result.value.num = func_fre(arg.value.num); break;
                case 0xB3: result.value.num = func_pos(arg.value.num); break;
            }
            break;
        }
        
        // 複数引数の文字列関数は別途実装
        default:
            set_error(state, ERR_UNDEF_FUNCTION, "Function not implemented");
            return result;
    }
    
    // 閉じ括弧
    token_t close_paren = get_next_token(state, parser_ptr);
    if (close_paren.type != TOKEN_DELIMITER || close_paren.value.operator != ')') {
        set_error(state, ERR_SYNTAX, ") expected after function arguments");
    }
    
    return result;
}

// 演算の実行
eval_result_t perform_operation(basic_state_t* state, eval_result_t left, char operator, eval_result_t right) {
    eval_result_t result = {0};
    
    // 型チェックと演算
    if (operator == '+' && (left.type == 1 || right.type == 1)) {
        // 文字列連結
        const char* left_str = (left.type == 1) ? left.value.str.data : "";
        const char* right_str = (right.type == 1) ? right.value.str.data : "";
        
        result = string_concatenate(left_str, right_str);
    } else if (left.type == 0 && right.type == 0) {
        // 数値演算
        result.type = 0;
        
        switch (operator) {
            case '+':
                result.value.num = math_add(left.value.num, right.value.num);
                break;
            case '-':
                result.value.num = math_subtract(left.value.num, right.value.num);
                break;
            case '*':
                result.value.num = math_multiply(left.value.num, right.value.num);
                break;
            case '/':
                if (numeric_to_double(right.value.num) == 0.0) {
                    set_error(state, ERR_DIVISION_BY_ZERO, NULL);
                    return result;
                }
                result.value.num = math_divide(left.value.num, right.value.num);
                break;
            case '^':
                result.value.num = math_power(left.value.num, right.value.num);
                break;
            case '=':
                result.value.num = double_to_numeric(math_equal(left.value.num, right.value.num));
                break;
            case '<':
                result.value.num = double_to_numeric(math_less_than(left.value.num, right.value.num));
                break;
            case '>':
                result.value.num = double_to_numeric(math_greater_than(left.value.num, right.value.num));
                break;
            default:
                set_error(state, ERR_SYNTAX, "Unknown operator");
                break;
        }
    } else if (left.type == 1 && right.type == 1) {
        // 文字列比較
        result.type = 0;
        
        switch (operator) {
            case '=':
                result.value.num = double_to_numeric(string_equal(left.value.str.data, right.value.str.data));
                break;
            case '<':
                result.value.num = double_to_numeric(string_less_than(left.value.str.data, right.value.str.data));
                break;
            case '>':
                result.value.num = double_to_numeric(string_greater_than(left.value.str.data, right.value.str.data));
                break;
            default:
                set_error(state, ERR_TYPE_MISMATCH, "Invalid string operation");
                break;
        }
    } else {
        set_error(state, ERR_TYPE_MISMATCH, "Type mismatch in operation");
    }
    
    // メモリクリーンアップ
    if (left.type == 1 && left.value.str.data) free(left.value.str.data);
    if (right.type == 1 && right.value.str.data) free(right.value.str.data);
    
    return result;
}





