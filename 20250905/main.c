#include "basic.h"

// 関数プロトタイプ（parser.cから）
int parse_line(basic_state_t* state, const char* line);
int cmd_print(basic_state_t* state, parser_state_t* parser);
int cmd_let(basic_state_t* state, parser_state_t* parser);

void print_banner() {
    printf("%s\n", BASIC_VERSION_STRING);
    printf("READY\n");
}

void print_prompt() {
    printf("] ");
    fflush(stdout);
}

int main() {
    basic_state_t state;
    char input_line[MAX_LINE_LENGTH + 1];
    
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
        if (!fgets(input_line, sizeof(input_line), stdin)) {
            break; // EOF
        }
        
        // 改行文字を削除
        size_t len = strlen(input_line);
        if (len > 0 && input_line[len - 1] == '\n') {
            input_line[len - 1] = '\0';
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
    printf("BYE\n");
    
    return 0;
}
