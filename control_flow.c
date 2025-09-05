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
    // Evaluate full expression for condition
    eval_result_t cond = evaluate_expression(state, parser_ptr);
    if (has_error(state)) return -1;

    // Expect THEN keyword
    token_t then_tok = get_next_token(state, parser_ptr);
    if (!(then_tok.type == TOKEN_KEYWORD && then_tok.value.keyword_id == 0xA1)) { // THEN
        set_error(state, ERR_SYNTAX, "THEN expected in IF statement");
        return -1;
    }

    // Truthiness
    bool truthy = false;
    if (cond.type == 0) {
        truthy = (numeric_to_double(cond.value.num) != 0.0);
    } else if (cond.type == 1) {
        truthy = (cond.value.str.data && cond.value.str.data[0] != '\0');
        if (cond.value.str.data) free(cond.value.str.data);
    }

    // Helper: for false branch, skip only the immediate statement after THEN up to ':' or EOL
    if (!truthy) {
        while (1) {
            uint16_t save = parser_ptr->position;
            token_t t = get_next_token(state, parser_ptr);
            if (t.type == TOKEN_EOF || t.type == TOKEN_EOL) break;
            if (t.type == TOKEN_DELIMITER && t.value.operator == ':') {
                // leave ':' for outer loop to consume
                parser_ptr->position = save;
                parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
                break;
            }
            if (save == parser_ptr->position) break;
        }
        return 0;
    }

    // True: execute exactly one immediate statement following THEN
    uint16_t stmt_start = parser_ptr->position;
    token_t t = get_next_token(state, parser_ptr);
    if (t.type == TOKEN_NUMBER) {
        uint16_t line = (uint16_t)numeric_to_double(t.value.number);
        return cmd_goto_line(state, line);
    }
    int rc = 0;
    if (t.type == TOKEN_KEYWORD) {
        switch (t.value.keyword_id) {
            case 0x97: rc = cmd_print(state, parser_ptr); break;          // PRINT
            case 0x87: rc = cmd_let(state, parser_ptr); break;            // LET
            case 0x81: rc = cmd_for(state, parser_ptr); break;            // FOR
            case 0x82: rc = cmd_next(state, parser_ptr); break;           // NEXT
            case 0x8A: rc = cmd_if(state, parser_ptr); break;             // nested IF
            case 0x88: rc = cmd_goto(state, parser_ptr); break;           // GOTO
            case 0x8C: rc = cmd_gosub(state, parser_ptr); break;          // GOSUB
            case 0x8D: rc = cmd_return(state, parser_ptr); break;         // RETURN
            case 0x90: rc = cmd_on_goto(state, parser_ptr); break;        // ON ... GOTO/GOSUB
            case 0x85: rc = cmd_dim(state, parser_ptr); break;            // DIM
            case 0x83: rc = cmd_data(state, parser_ptr); break;           // DATA
            case 0x86: rc = cmd_read(state, parser_ptr); break;           // READ
            case 0x8B: rc = cmd_restore(state, parser_ptr); break;        // RESTORE
            case 0x84: rc = cmd_input_ex(state, parser_ptr); break;       // INPUT
            case 0x9A: rc = cmd_clear(state, parser_ptr); break;          // CLEAR
            case 0x8F: rc = cmd_stop(state, parser_ptr); break;           // STOP
            case 0x80: rc = cmd_end(state, parser_ptr); break;            // END
            case 0x96: rc = cmd_poke(state, parser_ptr); break;           // POKE
            case 0x9B: rc = cmd_get(state, parser_ptr); break;            // GET
            case 0x92: rc = cmd_wait(state, parser_ptr); break;           // WAIT
            case 0x91: rc = cmd_null(state, parser_ptr); break;           // NULL
            case 0x95: rc = cmd_def(state, parser_ptr); break;            // DEF
            case 0x98: rc = cmd_cont(state, parser_ptr); break;           // CONT
            case 0x99: basic_list_program(state); rc = 0; break;          // LIST
            case 0x9C: basic_new_program(state); rc = 0; break;           // NEW
            case 0x9D: set_error(state, ERR_UNDEF_STATEMENT, "TAB not supported as statement"); rc = -1; break;
            default:
                set_error(state, ERR_UNDEF_STATEMENT, "Command not implemented after THEN");
                rc = -1;
        }
    } else if (t.type == TOKEN_VARIABLE) {
        // LET omitted
        parser_ptr->position = stmt_start;
        parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
        rc = cmd_let(state, parser_ptr);
    } else if (t.type == TOKEN_EOF || t.type == TOKEN_EOL) {
        rc = 0;
    } else {
        set_error(state, ERR_SYNTAX, "Invalid statement after THEN");
        rc = -1;
    }
    return rc;
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
    // ON expr GOTO/GOSUB line1, line2, ... (full expression supported)
    eval_result_t idxr = evaluate_expression(state, parser_ptr);
    if (has_error(state) || idxr.type != 0) { set_error(state, ERR_TYPE_MISMATCH, "Numeric expression expected"); return -1; }
    int index = (int)numeric_to_double(idxr.value.num);

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
