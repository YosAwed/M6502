#include "basic.h"
#include <ctype.h>

// externs
extern token_t get_next_token(basic_state_t* state, parser_state_t* parser);
extern variable_t* create_variable(basic_state_t* state, const char* name, variable_type_t type);
extern numeric_value_t double_to_numeric(double d);

// helper: parse next field from input line (handles quoted strings and commas)
static char* parse_field_quoted(char** pcur) {
    if (!pcur || !*pcur) return NULL;
    char* s = *pcur;
    while (*s == ' ' || *s == '\t') s++;
    char* out = NULL;
    if (*s == '"') {
        s++;
        size_t cap = strlen(s) + 1;
        out = (char*)malloc(cap);
        if (!out) return NULL;
        size_t oi = 0;
        while (*s) {
            if (*s == '"') {
                if (*(s+1) == '"') { out[oi++]='"'; s+=2; continue; }
                s++; // closing quote
                break;
            }
            out[oi++] = *s++;
        }
        out[oi] = '\0';
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ',') s++;
    } else {
        char* start = s;
        while (*s && *s != ',') s++;
        size_t len = (size_t)(s - start);
        while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t')) len--;
        out = (char*)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, start, len);
        out[len] = '\0';
        if (*s == ',') s++;
    }
    *pcur = s;
    return out;
}

int cmd_input_ex(basic_state_t* state, parser_state_t* parser_ptr) {
    bool have_prompt = false;
    bool prompt_with_question = false;
    char* prompt_text = NULL;

    // detect optional prompt; rewind if not a prompt
    uint16_t save_pos = parser_ptr->position;
    token_t tok = get_next_token(state, parser_ptr);
    if (tok.type == TOKEN_STRING) {
        prompt_text = tok.value.string;
        token_t sep = get_next_token(state, parser_ptr);
        if (sep.type == TOKEN_DELIMITER && (sep.value.operator == ';' || sep.value.operator == ',')) {
            have_prompt = true;
            if (sep.value.operator == ',') prompt_with_question = true;
        } else {
            parser_ptr->position = save_pos;
            parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
            if (prompt_text) { free(prompt_text); prompt_text = NULL; }
        }
    } else {
        parser_ptr->position = save_pos;
        parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
    }

    for (;;) {
        if (have_prompt && prompt_text) {
            if (prompt_with_question) printf("%s? ", prompt_text); else printf("%s", prompt_text);
        } else {
            printf("? ");
        }
        fflush(stdout);

        if (!fgets(state->input_buffer, sizeof(state->input_buffer), stdin)) {
            set_error(state, ERR_SYNTAX, "Input error");
            if (prompt_text) free(prompt_text);
            return -1;
        }
        size_t blen = strlen(state->input_buffer);
        if (blen > 0 && state->input_buffer[blen-1] == '\n') state->input_buffer[blen-1] = '\0';

        parser_state_t pv = *parser_ptr;
        char* cur = state->input_buffer;
        bool ok = true;
        while (ok) {
            token_t v = get_next_token(state, &pv);
            if (v.type != TOKEN_VARIABLE) { set_error(state, ERR_SYNTAX, "Variable expected in INPUT"); ok=false; break; }
            char* field = parse_field_quoted(&cur);
            if (!field) { set_error(state, ERR_SYNTAX, "Input parse error"); ok=false; free(v.value.string); break; }
            bool is_str = strchr(v.value.string, '$') != NULL;
            variable_type_t vt = is_str ? VAR_STRING : VAR_NUMERIC;
            variable_t* var = create_variable(state, v.value.string, vt);
            if (!var) { ok=false; free(field); free(v.value.string); break; }
            if (is_str) {
                if (var->value.str.data) free(var->value.str.data);
                var->value.str.data = field;
                var->value.str.length = (uint16_t)strlen(field);
            } else {
                char* endp=NULL; double val = strtod(field,&endp);
                while (endp && *endp && isspace((unsigned char)*endp)) endp++;
                if (!endp || *endp != '\0' || strlen(field)==0) { ok=false; }
                else { var->value.num = double_to_numeric(val); }
                free(field);
            }
            free(v.value.string);
            if (!ok) break;
            uint16_t sp = pv.position; token_t d = get_next_token(state, &pv);
            if (d.type == TOKEN_DELIMITER && d.value.operator == ',') continue;
            pv.position = sp; pv.current_char = (pv.position < pv.length) ? pv.text[pv.position] : '\0';
            break;
        }

        if (ok) {
            if (prompt_text) free(prompt_text);
            parser_ptr->position = pv.position;
            parser_ptr->current_char = (parser_ptr->position < parser_ptr->length) ? parser_ptr->text[parser_ptr->position] : '\0';
            return 0;
        }
        printf("?Redo from start\n");
    }
}

