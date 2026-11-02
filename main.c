#include "basic.h"

void print_banner() {
    printf("%s\n", BASIC_VERSION_STRING);
    printf("READY\n");
}

void print_prompt() {
    printf("] ");
    fflush(stdout);
}

// 長い入力も物理行の末尾まで読み、残りを別コマンドとして実行しない。
static int read_command(basic_state_t* state, char* line) {
    size_t length = 0;
    bool too_long = false;
    bool has_nul = false;
    int ch;
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (ch == '\0') has_nul = true;
        if (length < MAX_INPUT_LENGTH + 1) {
            line[length++] = (char)ch;
        } else {
            too_long = true;
        }
    }
    if (ch == EOF && length == 0 && !too_long) return 0;
    if (length > 0 && line[length - 1] == '\r') length--;
    line[length] = '\0';
    if (too_long || length > MAX_INPUT_LENGTH) {
        set_error(state, ERR_SYNTAX, "Input line too long");
        return -1;
    }
    if (has_nul) {
        set_error(state, ERR_SYNTAX, "NUL byte in input");
        return -1;
    }
    return 1;
}

int main() {
    basic_state_t state;
    char input_line[MAX_INPUT_LENGTH + 2];
    
    // 初期化
    if (basic_init(&state) != 0) {
        fprintf(stderr, "Failed to initialize BASIC interpreter\n");
        return 1;
    }
    
    print_banner();
    
    // メインループ
    while (1) {
        print_prompt();
        
        // 入力読み取り
        int read_status = read_command(&state, input_line);
        if (read_status == 0) {
            break; // EOF
        }
        if (read_status < 0) {
            print_error(&state);
            clear_error(&state);
            continue;
        }
        
        // 空行チェック
        if (strlen(input_line) == 0) {
            continue;
        }
        
        // 終了コマンドチェック
        if (strcmp(input_line, "QUIT") == 0 || strcmp(input_line, "EXIT") == 0) {
            break;
        }
        
        // 行を解析・実行
        parse_line(&state, input_line);
        
        // エラーチェック
        if (has_error(&state)) {
            print_error(&state);
            clear_error(&state);
        }
    }
    
    // クリーンアップ
    basic_cleanup(&state);
    cleanup_data_state();
    printf("BYE\n");
    
    return 0;
}
