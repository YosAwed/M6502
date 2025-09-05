#include "basic.h"
#include <ctype.h>

// 外部関数の宣言
numeric_value_t double_to_numeric(double d);
double numeric_to_double(numeric_value_t n);
variable_t* create_variable(basic_state_t* state, const char* name, variable_type_t type);
int add_program_line(basic_state_t* state, uint16_t line_number, const char* text);

// BASICキーワード定義
typedef struct {
    const char* name;
    uint8_t id;
} keyword_t;

// キーワードテーブル
static const keyword_t keywords[] = {
    {"END", 0x80}, {"FOR", 0x81}, {"NEXT", 0x82}, {"DATA", 0x83},
    {"INPUT", 0x84}, {"DIM", 0x85}, {"READ", 0x86}, {"LET", 0x87},
    {"GOTO", 0x88}, {"RUN", 0x89}, {"IF", 0x8A}, {"RESTORE", 0x8B},
    {"GOSUB", 0x8C}, {"RETURN", 0x8D}, {"REM", 0x8E}, {"STOP", 0x8F},
    {"ON", 0x90}, {"NULL", 0x91}, {"WAIT", 0x92}, {"LOAD", 0x93},
    {"SAVE", 0x94}, {"DEF", 0x95}, {"POKE", 0x96}, {"PRINT", 0x97},
    {"CONT", 0x98}, {"LIST", 0x99}, {"CLEAR", 0x9A}, {"GET", 0x9B},
    {"NEW", 0x9C}, {"TAB", 0x9D}, {"TO", 0x9E}, {"FN", 0x9F},
    {"SPC", 0xA0}, {"THEN", 0xA1}, {"NOT", 0xA2}, {"STEP", 0xA3},
    {"+", 0xA4}, {"-", 0xA5}, {"*", 0xA6}, {"/", 0xA7},
    {"^", 0xA8}, {"AND", 0xA9}, {"OR", 0xAA}, {">", 0xAB},
    {"=", 0xAC}, {"<", 0xAD}, {"SGN", 0xAE}, {"INT", 0xAF},
    {"ABS", 0xB0}, {"USR", 0xB1}, {"FRE", 0xB2}, {"POS", 0xB3},
    {"SQR", 0xB4}, {"RND", 0xB5}, {"LOG", 0xB6}, {"EXP", 0xB7},
    {"COS", 0xB8}, {"SIN", 0xB9}, {"TAN", 0xBA}, {"ATN", 0xBB},
    {"PEEK", 0xBC}, {"LEN", 0xBD}, {"STR$", 0xBE}, {"VAL", 0xBF},
    {"ASC", 0xC0}, {"CHR$", 0xC1}, {"LEFT$", 0xC2}, {"RIGHT$", 0xC3},
    {"MID$", 0xC4}, {NULL, 0}
};

// パーサー状態
// Disabled duplicate typedef; using basic.h's parser_state_t instead
/*
typedef struct {
    const char* text;
    uint16_t position;
    uint16_t length;
    char current_char;
} parser_state_t;
*/

// パーサー初期化
static void init_parser(parser_state_t* parser, const char* text) {
    parser->text = text;
    parser->position = 0;
    parser->length = strlen(text);
    parser->current_char = parser->length > 0 ? text[0] : '\0';
}

// 次の文字を取得
static void advance_parser(parser_state_t* parser) {
    parser->position++;
    if (parser->position >= parser->length) {
        parser->current_char = '\0';
    } else {
        parser->current_char = parser->text[parser->position];
    }
}

// 空白をスキップ
static void skip_whitespace(parser_state_t* parser) {
    while (parser->current_char == ' ' || parser->current_char == '\t') {
        advance_parser(parser);
    }
}

// 数値の解析
static bool parse_number(parser_state_t* parser, numeric_value_t* result) {
    if (!isdigit(parser->current_char) && parser->current_char != '.') {
        return false;
    }
    
    double value = 0.0;
    bool has_decimal = false;
    double decimal_factor = 0.1;
    
    // 整数部
    while (isdigit(parser->current_char)) {
        value = value * 10.0 + (parser->current_char - '0');
        advance_parser(parser);
    }
    
    // 小数部
    if (parser->current_char == '.') {
        has_decimal = true;
        advance_parser(parser);
        while (isdigit(parser->current_char)) {
            value += (parser->current_char - '0') * decimal_factor;
            decimal_factor *= 0.1;
            advance_parser(parser);
        }
    }
    
    // 指数部
    if (parser->current_char == 'E' || parser->current_char == 'e') {
        advance_parser(parser);
        bool negative_exp = false;
        if (parser->current_char == '-') {
            negative_exp = true;
            advance_parser(parser);
        } else if (parser->current_char == '+') {
            advance_parser(parser);
        }
        
        int exponent = 0;
        while (isdigit(parser->current_char)) {
            exponent = exponent * 10 + (parser->current_char - '0');
            advance_parser(parser);
        }
        
        if (negative_exp) exponent = -exponent;
        value *= pow(10.0, exponent);
    }
    
    *result = double_to_numeric(value);
    return true;
}

// 文字列の解析
static bool parse_string(parser_state_t* parser, char** result) {
    if (parser->current_char != '"') {
        return false;
    }
    
    advance_parser(parser); // 開始の"をスキップ
    
    uint16_t start_pos = parser->position;
    uint16_t length = 0;
    
    // 終了の"を探す
    while (parser->current_char != '\0' && parser->current_char != '"') {
        length++;
        advance_parser(parser);
    }
    
    if (parser->current_char != '"') {
        return false; // 閉じられていない文字列
    }
    
    // 文字列をコピー
    *result = (char*)malloc(length + 1);
    if (!*result) return false;
    
    strncpy(*result, parser->text + start_pos, length);
    (*result)[length] = '\0';
    
    advance_parser(parser); // 終了の"をスキップ
    return true;
}

// 変数名の解析
static bool parse_variable(parser_state_t* parser, char* result) {
    if (!isalpha(parser->current_char)) {
        return false;
    }
    
    result[0] = toupper(parser->current_char);
    advance_parser(parser);
    
    if (isalnum(parser->current_char)) {
        result[1] = toupper(parser->current_char);
        result[2] = '\0';
        advance_parser(parser);
    } else {
        result[1] = '\0';
    }
    
    // 文字列変数の$記号
    if (parser->current_char == '$') {
        if (result[1] == '\0') {
            result[1] = '$';
            result[2] = '\0';
        } else {
            result[2] = '$';
            result[3] = '\0';
        }
        advance_parser(parser);
    }
    
    return true;
}

// キーワードの検索
static uint8_t find_keyword(const char* word) {
    for (int i = 0; keywords[i].name; i++) {
        if (strcmp(word, keywords[i].name) == 0) {
            return keywords[i].id;
        }
    }
    return 0;
}

// 次のトークンを取得
token_t get_next_token(basic_state_t* state, parser_state_t* parser) {
    token_t token = {0};
    
    skip_whitespace(parser);
    
    if (parser->current_char == '\0') {
        token.type = TOKEN_EOF;
        return token;
    }
    
    token.position = parser->position;
    
    // 数値
    if (isdigit(parser->current_char) || parser->current_char == '.') {
        token.type = TOKEN_NUMBER;
        if (!parse_number(parser, &token.value.number)) {
            set_error(state, ERR_SYNTAX, "Invalid number format");
        }
        return token;
    }
    
    // 文字列
    if (parser->current_char == '"') {
        token.type = TOKEN_STRING;
        if (!parse_string(parser, &token.value.string)) {
            set_error(state, ERR_SYNTAX, "Unterminated string");
        }
        return token;
    }
    
    // 変数またはキーワード
    if (isalpha(parser->current_char)) {
        char word[32];
        uint16_t word_len = 0;
        uint16_t start_pos = parser->position;
        
        // 単語を読み取り
        while ((isalnum(parser->current_char) || parser->current_char == '$') && 
               word_len < sizeof(word) - 1) {
            word[word_len++] = toupper(parser->current_char);
            advance_parser(parser);
        }
        word[word_len] = '\0';
        
        // キーワードチェック
        uint8_t keyword_id = find_keyword(word);
        if (keyword_id) {
            token.type = TOKEN_KEYWORD;
            token.value.keyword_id = keyword_id;
        } else {
            token.type = TOKEN_VARIABLE;
            token.value.string = (char*)malloc(strlen(word) + 1);
            if (token.value.string) {
                strcpy(token.value.string, word);
            }
        }
        return token;
    }
    
    // 演算子と区切り文字
    char ch = parser->current_char;
    advance_parser(parser);
    
    switch (ch) {
        case '+': case '-': case '*': case '/': case '^':
        case '=': case '<': case '>':
            token.type = TOKEN_OPERATOR;
            token.value.operator = ch;
            
            // 複合演算子のチェック
            if ((ch == '<' || ch == '>') && parser->current_char == '=') {
                advance_parser(parser);
                // <= または >= として処理
            } else if (ch == '<' && parser->current_char == '>') {
                advance_parser(parser);
                // <> として処理
            }
            break;
            
        case '(': case ')': case ',': case ';': case ':':
            token.type = TOKEN_DELIMITER;
            token.value.operator = ch;
            break;
            
        case '\n': case '\r':
            token.type = TOKEN_EOL;
            break;
            
        default:
            set_error(state, ERR_SYNTAX, "Unexpected character");
            token.type = TOKEN_EOF;
            break;
    }
    
    return token;
}

// 行の解析（行番号の有無をチェック）
int parse_line(basic_state_t* state, const char* line) {
    if (!state || !line) return -1;
    
    parser_state_t parser;
    init_parser(&parser, line);
    
    skip_whitespace(&parser);
    
    // 空行チェック
    if (parser.current_char == '\0') {
        return 0;
    }
    
    // 行番号チェック
    if (isdigit(parser.current_char)) {
        numeric_value_t line_num;
        if (parse_number(&parser, &line_num)) {
            uint16_t line_number = (uint16_t)numeric_to_double(line_num);
            
            skip_whitespace(&parser);
            
            // 残りのテキストを取得
            const char* remaining = parser.text + parser.position;
            
            // プログラム行として追加
            return add_program_line(state, line_number, remaining);
        }
    }
    
    // 即座実行モード
    state->immediate_mode = true;
    return basic_execute_line(state, line);
}

// 行の実行（基本的な実装）
int basic_execute_line(basic_state_t* state, const char* line) {
    if (!state || !line) return -1;
    
    parser_state_t parser;
    init_parser(&parser, line);
    // If resuming mid-line (e.g., FOR/NEXT single-line), honor saved position
    if (state->current_position > 0) {
        if (state->current_position < parser.length) {
            parser.position = state->current_position;
            parser.current_char = parser.text[parser.position];
        } else {
            parser.position = parser.length;
            parser.current_char = '\0';
        }
        // clear resume position after applying
        state->current_position = 0;
    }
    
    while (true) {
        token_t token = get_next_token(state, &parser);
        // If resuming mid-line, we may start on a ':'; skip leading ':' delimiters
        while (token.type == TOKEN_DELIMITER && token.value.operator == ':') {
            token = get_next_token(state, &parser);
        }
        if (has_error(state)) return -1;

        int rc = 0;
        if (token.type == TOKEN_KEYWORD) {
            switch (token.value.keyword_id) {
                case 0x97: rc = cmd_print(state, &parser); break;          // PRINT
                case 0x87: rc = cmd_let(state, &parser); break;            // LET
                case 0x81: rc = cmd_for(state, &parser); break;            // FOR
                case 0x82: rc = cmd_next(state, &parser); break;           // NEXT
                case 0x8A: rc = cmd_if(state, &parser); break;             // IF
                case 0x88: rc = cmd_goto(state, &parser); break;           // GOTO
                case 0x8C: rc = cmd_gosub(state, &parser); break;          // GOSUB
                case 0x8D: rc = cmd_return(state, &parser); break;         // RETURN
                case 0x90: rc = cmd_on_goto(state, &parser); break;        // ON ... GOTO/GOSUB
                case 0x85: rc = cmd_dim(state, &parser); break;            // DIM
                case 0x83: rc = cmd_data(state, &parser); break;           // DATA
                case 0x86: rc = cmd_read(state, &parser); break;           // READ
                case 0x8B: rc = cmd_restore(state, &parser); break;        // RESTORE
                case 0x84: rc = cmd_input(state, &parser); break;          // INPUT
                case 0x9A: rc = cmd_clear(state, &parser); break;          // CLEAR
                case 0x8F: rc = cmd_stop(state, &parser); break;           // STOP
                case 0x80: rc = cmd_end(state, &parser); break;            // END
                case 0x96: rc = cmd_poke(state, &parser); break;           // POKE
                case 0x9B: rc = cmd_get(state, &parser); break;            // GET
                case 0x92: rc = cmd_wait(state, &parser); break;           // WAIT
                case 0x91: rc = cmd_null(state, &parser); break;           // NULL
                case 0x95: rc = cmd_def(state, &parser); break;            // DEF
                case 0x98: rc = cmd_cont(state, &parser); break;           // CONT
                case 0x9D: set_error(state, ERR_UNDEF_STATEMENT, "TAB not supported as statement"); rc = -1; break;
                case 0x9E: case 0xA3: case 0xA1:
                    set_error(state, ERR_SYNTAX, "Misplaced keyword"); rc = -1; break;
                case 0x99: basic_list_program(state); rc = 0; break;       // LIST
                case 0x9C: basic_new_program(state); rc = 0; break;        // NEW
                case 0x89: rc = basic_run_program(state); break;           // RUN
                case 0x8E: rc = cmd_rem(state, &parser); break;            // REM
                default:
                    set_error(state, ERR_UNDEF_STATEMENT, "Command not implemented");
                    rc = -1;
            }
        } else if (token.type == TOKEN_EOF || token.type == TOKEN_EOL) {
            break;
        } else {
            set_error(state, ERR_SYNTAX, "Invalid statement");
            rc = -1;
        }

        if (rc != 0 || has_error(state)) return rc;

        // After a statement, optionally consume ':' and continue; otherwise stop at EOL/EOF
        uint16_t save_pos = parser.position;
        token_t sep = get_next_token(state, &parser);
        if (sep.type == TOKEN_DELIMITER && sep.value.operator == ':') {
            continue; // next statement on the same line
        }
        // put back non-separator token
        parser.position = save_pos;
        parser.current_char = (parser.position < parser.length) ? parser.text[parser.position] : '\0';
        break;
    }

    return 0;
}

int cmd_print(basic_state_t* state, parser_state_t* parser) {
    bool trailing_semicolon = false;
    bool first = true;
    while (1) {
        // Allow bare EOL
        uint16_t pos0 = parser->position;
        token_t peek = get_next_token(state, parser);
        if (peek.type == TOKEN_EOF || peek.type == TOKEN_EOL) {
            break;
        }
        // handle separators directly
        if (peek.type == TOKEN_DELIMITER && (peek.value.operator == ',' || peek.value.operator == ';')) {
            if (peek.value.operator == ',') { printf("\t"); }
            else { trailing_semicolon = true; }
            first = false;
            continue;
        }
        // TAB(n)
        if (peek.type == TOKEN_KEYWORD && peek.value.keyword_id == 0x9D) {
            token_t open = get_next_token(state, parser);
            if (open.type != TOKEN_DELIMITER || open.value.operator != '(') { set_error(state, ERR_SYNTAX, "( expected after TAB"); return -1; }
            eval_result_t n = evaluate_expression(state, parser);
            if (has_error(state) || n.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric expected in TAB"); return -1; }
            token_t close = get_next_token(state, parser);
            if (close.type != TOKEN_DELIMITER || close.value.operator != ')') { set_error(state, ERR_SYNTAX, ") expected after TAB"); return -1; }
            int count = (int)numeric_to_double(n.value.num);
            for (int i=0;i<count;i++) putchar(' ');
            first = false; trailing_semicolon = false; continue;
        }
        // Not a simple separator: rewind and evaluate expression
        parser->position = pos0;
        parser->current_char = (parser->position < parser->length) ? parser->text[parser->position] : '\0';
        eval_result_t val = evaluate_expression(state, parser);
        if (has_error(state)) return -1;
        if (val.type == 1) {
            printf("%s", val.value.str.data ? val.value.str.data : "");
            if (val.value.str.data) free(val.value.str.data);
        } else {
            printf("%g", numeric_to_double(val.value.num));
        }
        first = false; trailing_semicolon = false;
        // Check for separator
        uint16_t save = parser->position;
        token_t sep = get_next_token(state, parser);
        if (sep.type == TOKEN_DELIMITER && (sep.value.operator == ',' || sep.value.operator == ';')) {
            if (sep.value.operator == ';') trailing_semicolon = true; else trailing_semicolon = false;
            continue;
        }
        parser->position = save;
        parser->current_char = (parser->position < parser->length) ? parser->text[parser->position] : '\0';
        break;
    }
    if (!trailing_semicolon) putchar('\n');
    return 0;
}


// 基本的なLETコマンド実装
int cmd_let(basic_state_t* state, parser_state_t* parser) {
    token_t var_token = get_next_token(state, parser);
    
    if (var_token.type != TOKEN_VARIABLE) {
        set_error(state, ERR_SYNTAX, "Variable name expected");
        return -1;
    }
    
    // Check for array element assignment: VAR(...)=...
    uint16_t save_pos = parser->position;
    token_t next = get_next_token(state, parser);
    if (next.type == TOKEN_DELIMITER && next.value.operator == '(') {
        uint16_t indices[MAX_ARRAY_DIMENSIONS];
        uint8_t index_count = 0;
        while (index_count < MAX_ARRAY_DIMENSIONS) {
            eval_result_t idx = evaluate_expression(state, parser);
            if (has_error(state) || idx.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric index expected"); free(var_token.value.string); return -1; }
            indices[index_count++] = (uint16_t)numeric_to_double(idx.value.num);
            token_t sep = get_next_token(state, parser);
            if (sep.type == TOKEN_DELIMITER && sep.value.operator == ',') {
                continue;
            } else if (sep.type == TOKEN_DELIMITER && sep.value.operator == ')') {
                break;
            } else {
                set_error(state, ERR_SYNTAX, ", or ) expected in array assignment");
                free(var_token.value.string);
                return -1;
            }
        }
        token_t eq2 = get_next_token(state, parser);
        if (eq2.type != TOKEN_OPERATOR || eq2.value.operator != '=') { set_error(state, ERR_SYNTAX, "= expected"); free(var_token.value.string); return -1; }
        eval_result_t value = evaluate_expression(state, parser);
        if (has_error(state)) { free(var_token.value.string); return -1; }
        int rc = assign_array_element(state, var_token.value.string, indices, index_count, value);
        free(var_token.value.string);
        return rc;
    }
    // Not an array: rewind and parse '=' and expression
    parser->position = save_pos;
    parser->current_char = (parser->position < parser->length) ? parser->text[parser->position] : '\0';
    
    token_t eq_token = get_next_token(state, parser);
    if (eq_token.type != TOKEN_OPERATOR || eq_token.value.operator != '=') {
        set_error(state, ERR_SYNTAX, "= expected");
        free(var_token.value.string);
        return -1;
    }
    
    eval_result_t value = evaluate_expression(state, parser);
    if (has_error(state)) { free(var_token.value.string); return -1; }
    
    bool is_string = strchr(var_token.value.string, '$') != NULL;
    variable_type_t var_type = is_string ? VAR_STRING : VAR_NUMERIC;
    variable_t* var = create_variable(state, var_token.value.string, var_type);
    if (!var) { if (value.type==1 && value.value.str.data) free(value.value.str.data); free(var_token.value.string); return -1; }
    
    if (is_string) {
        if (value.type != 1) { set_error(state, ERR_TYPE_MISMATCH, NULL); if (value.value.str.data) free(value.value.str.data); free(var_token.value.string); return -1; }
        if (var->value.str.data) free(var->value.str.data);
        var->value.str.data = value.value.str.data; // take ownership
        var->value.str.length = var->value.str.data ? (uint16_t)strlen(var->value.str.data) : 0;
    } else {
        if (value.type != 0) { set_error(state, ERR_TYPE_MISMATCH, NULL); if (value.type==1 && value.value.str.data) free(value.value.str.data); free(var_token.value.string); return -1; }
        var->value.num = value.value.num;
    }
    
    free(var_token.value.string);
    return 0;
}

// プログラム実行
int basic_run_program(basic_state_t* state) {
    if (!state) return -1;
    
    state->current_line = state->program_start;
    state->running = true;
    
    while (state->current_line && state->running && !has_error(state)) {
        program_line_t* before = state->current_line;
        int result = basic_execute_line(state, state->current_line->text);
        if (result != 0 || has_error(state) || !state->running) break;
        
        // If resuming mid-line (state->current_position set), or current_line changed, don't auto-advance
        if (state->current_line == before && state->current_position == 0) {
            state->current_line = state->current_line->next;
        }
    }
    
    state->running = false;
    return has_error(state) ? -1 : 0;
}



