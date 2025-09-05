#include "basic.h"
#include <ctype.h>

// External helpers declared in headers
extern program_line_t* find_line(basic_state_t* state, uint16_t line_number);
extern eval_result_t evaluate_expression(basic_state_t* state, parser_state_t* parser);
extern token_t get_next_token(basic_state_t* state, parser_state_t* parser);
extern variable_t* find_variable(basic_state_t* state, const char* name);
extern variable_t* create_variable(basic_state_t* state, const char* name, variable_type_t type);
extern double numeric_to_double(numeric_value_t n);
extern numeric_value_t double_to_numeric(double d);

static void parser_skip_spaces(parser_state_t* p) {
    while (p->current_char == ' ' || p->current_char == '\t') {
        p->position++;
        p->current_char = (p->position < p->length) ? p->text[p->position] : '\0';
    }
}

int cmd_goto_line(basic_state_t* state, uint16_t line_number) {
    if (!state) return -1;
    program_line_t* target = find_line(state, line_number);
    if (!target) {
        set_error(state, ERR_UNDEF_STATEMENT, "Line not found");
        return -1;
    }
    state->current_line = target;
    state->current_position = 0;
    return 0;
}

int cmd_goto(basic_state_t* state, parser_state_t* parser_ptr) {
    eval_result_t val = evaluate_expression(state, parser_ptr);
    if (has_error(state) || val.type != 0) {
        set_error(state, ERR_TYPE_MISMATCH, "Numeric line expected");
        return -1;
    }
    uint16_t line = (uint16_t)numeric_to_double(val.value.num);
    return cmd_goto_line(state, line);
}

int cmd_if(basic_state_t* state, parser_state_t* parser_ptr) {
    // Find THEN and evaluate a minimal condition before it
    uint16_t expr_start = parser_ptr->position;
    const char* txt = parser_ptr->text;
    uint16_t len = parser_ptr->length;
    uint16_t i = parser_ptr->position;
    bool found_then = false;
    while (i + 3 < len) {
        if (toupper((unsigned char)txt[i])=='T' && toupper((unsigned char)txt[i+1])=='H' &&
            toupper((unsigned char)txt[i+2])=='E' && toupper((unsigned char)txt[i+3])=='N') {
            bool left_ok = (i == 0) || !isalnum((unsigned char)txt[i-1]);
            bool right_ok = (i + 4 >= len) || !isalnum((unsigned char)txt[i+4]);
            if (left_ok && right_ok) { found_then = true; break; }
        }
        i++;
    }
    if (!found_then) { set_error(state, ERR_SYNTAX, "THEN expected in IF statement"); return -1; }

    // Simple condition parser within [expr_start, i)
    parser_state_t p = *parser_ptr;
    p.position = expr_start;
    p.length = i;
    p.current_char = (p.position < p.length) ? p.text[p.position] : '\0';

    // Read LHS
    token_t lhs = get_next_token(state, &p);
    bool lhs_is_num = false, lhs_is_str = false;
    double lhs_num = 0.0; const char* lhs_str = NULL; char* lhs_str_owned = NULL;
    if (lhs.type == TOKEN_NUMBER) { lhs_is_num = true; lhs_num = numeric_to_double(lhs.value.number); }
    else if (lhs.type == TOKEN_STRING) { lhs_is_str = true; lhs_str = lhs.value.string; lhs_str_owned = lhs.value.string; }
    else if (lhs.type == TOKEN_VARIABLE) {
        variable_t* v = find_variable(state, lhs.value.string);
        if (v && v->type == VAR_NUMERIC) { lhs_is_num = true; lhs_num = numeric_to_double(v->value.num); }
        else if (v && v->type == VAR_STRING) { lhs_is_str = true; lhs_str = v->value.str.data ? v->value.str.data : ""; }
        else { lhs_is_num = true; lhs_num = 0.0; }
        free(lhs.value.string);
    } else {
        set_error(state, ERR_SYNTAX, "Invalid IF condition");
        if (lhs.type == TOKEN_STRING && lhs_str_owned) free(lhs_str_owned);
        return -1;
    }

    // Peek operator
    token_t op = get_next_token(state, &p);
    bool has_op = (op.type == TOKEN_OPERATOR && (op.value.operator=='=' || op.value.operator=='<' || op.value.operator=='>' ));
    bool truthy = false;
    if (!has_op) {
        // No operator: truthiness of LHS
        if (lhs_is_num) truthy = (lhs_num != 0.0); else truthy = (lhs_str && lhs_str[0] != '\0');
    } else {
        // RHS
        token_t rhs = get_next_token(state, &p);
        bool rhs_is_num = false, rhs_is_str = false; double rhs_num = 0.0; const char* rhs_str = NULL; char* rhs_str_owned = NULL;
        if (rhs.type == TOKEN_NUMBER) { rhs_is_num = true; rhs_num = numeric_to_double(rhs.value.number); }
        else if (rhs.type == TOKEN_STRING) { rhs_is_str = true; rhs_str = rhs.value.string; rhs_str_owned = rhs.value.string; }
        else if (rhs.type == TOKEN_VARIABLE) {
            variable_t* v2 = find_variable(state, rhs.value.string);
            if (v2 && v2->type == VAR_NUMERIC) { rhs_is_num = true; rhs_num = numeric_to_double(v2->value.num); }
            else if (v2 && v2->type == VAR_STRING) { rhs_is_str = true; rhs_str = v2->value.str.data ? v2->value.str.data : ""; }
            else { rhs_is_num = true; rhs_num = 0.0; }
            free(rhs.value.string);
        } else {
            set_error(state, ERR_SYNTAX, "Invalid IF condition RHS");
            if (lhs_str_owned) free(lhs_str_owned);
            return -1;
        }
        if (lhs_is_num && rhs_is_num) {
            if (op.value.operator=='=') truthy = (lhs_num == rhs_num);
            else if (op.value.operator=='<') truthy = (lhs_num < rhs_num);
            else if (op.value.operator=='>') truthy = (lhs_num > rhs_num);
        } else if (lhs_is_str && rhs_is_str) {
            int cmp = strcmp(lhs_str ? lhs_str : "", rhs_str ? rhs_str : "");
            if (op.value.operator=='=') truthy = (cmp == 0);
            else if (op.value.operator=='<') truthy = (cmp < 0);
            else if (op.value.operator=='>') truthy = (cmp > 0);
        } else {
            set_error(state, ERR_TYPE_MISMATCH, "Type mismatch in IF");
            if (lhs_str_owned) free(lhs_str_owned);
            if (rhs_str_owned) free(rhs_str_owned);
            return -1;
        }
        if (rhs_str_owned) free(rhs_str_owned);
    }
    if (lhs_str_owned) free(lhs_str_owned);

    // Advance real parser to after THEN
    parser_ptr->position = i + 4;
    parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
    parser_skip_spaces(parser_ptr);

    if (!truthy) return 0;

    token_t next = get_next_token(state, parser_ptr);
    if (next.type == TOKEN_NUMBER) {
        uint16_t line = (uint16_t)numeric_to_double(next.value.number);
        return cmd_goto_line(state, line);
    }
    set_error(state, ERR_UNDEF_STATEMENT, "Statement after THEN not implemented");
    return -1;
}

int cmd_gosub(basic_state_t* state, parser_state_t* parser_ptr) {
    // Evaluate target line
    eval_result_t val = evaluate_expression(state, parser_ptr);
    if (has_error(state) || val.type != 0) {
        set_error(state, ERR_TYPE_MISMATCH, "Numeric line expected");
        return -1;
    }

    // Push return address
    gosub_stack_entry_t* entry = (gosub_stack_entry_t*)malloc(sizeof(gosub_stack_entry_t));
    if (!entry) { set_error(state, ERR_OUT_OF_MEMORY, NULL); return -1; }
    entry->line = state->current_line;
    entry->position = parser_ptr->position; // resume after GOSUB args
    entry->next = state->gosub_stack;
    state->gosub_stack = entry;

    uint16_t line = (uint16_t)numeric_to_double(val.value.num);
    return cmd_goto_line(state, line);
}

int cmd_return(basic_state_t* state, parser_state_t* parser_ptr) {
    (void)parser_ptr;
    if (!state->gosub_stack) {
        set_error(state, ERR_RETURN_WITHOUT_GOSUB, NULL);
        return -1;
    }
    gosub_stack_entry_t* entry = state->gosub_stack;
    state->gosub_stack = entry->next;

    state->current_line = entry->line;
    state->current_position = entry->position;
    free(entry);
    return 0;
}

int cmd_for(basic_state_t* state, parser_state_t* parser_ptr) {
    // FOR var = start TO limit [STEP step]
    token_t var_tok = get_next_token(state, parser_ptr);
    if (var_tok.type != TOKEN_VARIABLE) { set_error(state, ERR_SYNTAX, "Variable expected after FOR"); return -1; }

    // Only numeric loops supported
    bool is_string = (strchr(var_tok.value.string, '$') != NULL);
    if (is_string) { set_error(state, ERR_TYPE_MISMATCH, "FOR variable must be numeric"); free(var_tok.value.string); return -1; }

    token_t eq = get_next_token(state, parser_ptr);
    if (eq.type != TOKEN_OPERATOR || eq.value.operator != '=') { set_error(state, ERR_SYNTAX, "= expected after FOR variable"); free(var_tok.value.string); return -1; }

    eval_result_t start_val = evaluate_expression(state, parser_ptr);
    if (has_error(state) || start_val.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric start expected"); free(var_tok.value.string); return -1; }

    token_t to_kw = get_next_token(state, parser_ptr);
    if (to_kw.type != TOKEN_KEYWORD || to_kw.value.keyword_id != 0x9E) { set_error(state, ERR_SYNTAX, "TO expected"); free(var_tok.value.string); return -1; }

    eval_result_t limit_val = evaluate_expression(state, parser_ptr);
    if (has_error(state) || limit_val.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric limit expected"); free(var_tok.value.string); return -1; }

    numeric_value_t step_val = double_to_numeric(1.0);
    uint16_t save_pos = parser_ptr->position;
    token_t maybe_step = get_next_token(state, parser_ptr);
    if (maybe_step.type == TOKEN_KEYWORD && maybe_step.value.keyword_id == 0xA3) {
        eval_result_t step_expr = evaluate_expression(state, parser_ptr);
        if (has_error(state) || step_expr.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric STEP expected"); free(var_tok.value.string); return -1; }
        step_val = step_expr.value.num;
    } else {
        // rewind
        parser_ptr->position = save_pos;
        parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
    }

    // Initialize/control variable
    variable_t* var = create_variable(state, var_tok.value.string, VAR_NUMERIC);
    if (!var) { free(var_tok.value.string); return -1; }
    var->value.num = start_val.value.num;

    // Push FOR frame
    for_stack_entry_t* fe = (for_stack_entry_t*)malloc(sizeof(for_stack_entry_t));
    if (!fe) { set_error(state, ERR_OUT_OF_MEMORY, NULL); free(var_tok.value.string); return -1; }
    memset(fe, 0, sizeof(*fe));
    strncpy(fe->var_name, var_tok.value.string, sizeof(fe->var_name)-1);
    fe->limit = limit_val.value.num;
    fe->step = step_val;
    fe->line = state->current_line;
    fe->position = parser_ptr->position; // resume at first statement after FOR
    fe->next = state->for_stack;
    state->for_stack = fe;

    free(var_tok.value.string);
    return 0;
}

int cmd_next(basic_state_t* state, parser_state_t* parser_ptr) {
    // NEXT [var]
    char* var_name = NULL;
    uint16_t save_pos = parser_ptr->position;
    token_t t = get_next_token(state, parser_ptr);
    if (t.type == TOKEN_VARIABLE) {
        var_name = t.value.string;
    } else {
        // no var provided; rewind
        parser_ptr->position = save_pos;
        parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
    }

    if (!state->for_stack) { if (var_name) free(var_name); set_error(state, ERR_NEXT_WITHOUT_FOR, NULL); return -1; }

    // Find matching FOR frame (top-most, or by name)
    for_stack_entry_t* prev = NULL;
    for_stack_entry_t* cur = state->for_stack;
    if (var_name) {
        while (cur && strcmp(cur->var_name, var_name) != 0) { prev = cur; cur = cur->next; }
        if (!cur) { free(var_name); set_error(state, ERR_NEXT_WITHOUT_FOR, NULL); return -1; }
    }

    if (!cur) { cur = state->for_stack; }

    // Increment the loop variable
    variable_t* v = find_variable(state, cur->var_name);
    if (!v || v->type != VAR_NUMERIC) { if (var_name) free(var_name); set_error(state, ERR_UNDEF_STATEMENT, "FOR variable missing"); return -1; }

    double step = numeric_to_double(cur->step);
    double new_val = numeric_to_double(v->value.num) + step;
    v->value.num = double_to_numeric(new_val);

    double limit = numeric_to_double(cur->limit);
    bool continue_loop = false;
    if (step >= 0) continue_loop = (new_val <= limit); else continue_loop = (new_val >= limit);

    if (continue_loop) {
        // Jump back to after FOR statement
        state->current_line = cur->line;
        state->current_position = cur->position;
    } else {
        // Pop this frame
        if (prev) prev->next = cur->next; else state->for_stack = cur->next;
        free(cur);
    }

    if (var_name) free(var_name);
    return 0;
}

int cmd_on_goto(basic_state_t* state, parser_state_t* parser_ptr) {
    // ON expr GOTO/GOSUB line1, line2, ... (simple expr: number or numeric variable)
    int index = 0;
    token_t idx_tok = get_next_token(state, parser_ptr);
    if (idx_tok.type == TOKEN_NUMBER) {
        index = (int)numeric_to_double(idx_tok.value.number);
    } else if (idx_tok.type == TOKEN_VARIABLE) {
        variable_t* v = find_variable(state, idx_tok.value.string);
        if (v && v->type == VAR_NUMERIC) index = (int)numeric_to_double(v->value.num);
        else { set_error(state, ERR_TYPE_MISMATCH, "Numeric expression expected"); free(idx_tok.value.string); return -1; }
        free(idx_tok.value.string);
    } else {
        set_error(state, ERR_TYPE_MISMATCH, "Numeric expression expected");
        return -1;
    }

    token_t which = get_next_token(state, parser_ptr);
    bool do_gosub = false;
    if (which.type == TOKEN_KEYWORD && which.value.keyword_id == 0x88) { // GOTO
        do_gosub = false;
    } else if (which.type == TOKEN_KEYWORD && which.value.keyword_id == 0x8C) { // GOSUB
        do_gosub = true;
    } else {
        set_error(state, ERR_SYNTAX, "GOTO or GOSUB expected");
        return -1;
    }

    // Read list of line numbers
    int current = 1;
    uint16_t chosen = 0;
    while (1) {
        token_t t = get_next_token(state, parser_ptr);
        if (t.type != TOKEN_NUMBER) break;
        if (current == index) {
            chosen = (uint16_t)numeric_to_double(t.value.number);
        }
        current++;
        uint16_t save = parser_ptr->position;
        token_t comma = get_next_token(state, parser_ptr);
        if (comma.type == TOKEN_DELIMITER && comma.value.operator == ',') {
            continue;
        }
        // rewind last non-comma
        parser_ptr->position = save;
        parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
        break;
    }

    if (chosen == 0) {
        // No branch
        return 0;
    }

    if (do_gosub) {
        // Push return info
        gosub_stack_entry_t* entry = (gosub_stack_entry_t*)malloc(sizeof(gosub_stack_entry_t));
        if (!entry) { set_error(state, ERR_OUT_OF_MEMORY, NULL); return -1; }
        entry->line = state->current_line;
        entry->position = parser_ptr->position;
        entry->next = state->gosub_stack;
        state->gosub_stack = entry;
    }

    return cmd_goto_line(state, chosen);
}
