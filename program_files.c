#include "basic.h"

#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/* These commands use literal paths, so backslashes in Windows paths remain
 * literal and a colon inside quotes cannot become a statement separator. */
static int parse_file_argument(basic_state_t* state, parser_state_t* parser,
                               bool optional, char** path) {
    size_t pos = parser->position;
    *path = NULL;
    while (pos < parser->length &&
           (parser->text[pos] == ' ' || parser->text[pos] == '\t')) pos++;
    if (pos == parser->length || parser->text[pos] == ':' ||
        parser->text[pos] == '\r' || parser->text[pos] == '\n') {
        if (!optional) {
            set_error(state, ERR_SYNTAX, "Quoted filename expected");
            return -1;
        }
    } else {
        if (parser->text[pos] != '"') {
            set_error(state, ERR_SYNTAX, "Quoted filename expected");
            return -1;
        }
        size_t start = ++pos;
        while (pos < parser->length && parser->text[pos] != '"' &&
               parser->text[pos] != '\r' && parser->text[pos] != '\n') pos++;
        if (pos == parser->length || parser->text[pos] != '"') {
            set_error(state, ERR_SYNTAX, "Unterminated filename");
            return -1;
        }
        size_t length = pos - start;
        if (length == 0) {
            set_error(state, ERR_SYNTAX, "Filename must not be empty");
            return -1;
        }
        *path = malloc(length + 1);
        if (!*path) {
            set_error(state, ERR_OUT_OF_MEMORY, NULL);
            return -1;
        }
        memcpy(*path, parser->text + start, length);
        (*path)[length] = '\0';
        pos++;
        while (pos < parser->length &&
               (parser->text[pos] == ' ' || parser->text[pos] == '\t')) pos++;
        if (pos < parser->length && parser->text[pos] != ':' &&
            parser->text[pos] != '\r' && parser->text[pos] != '\n') {
            free(*path);
            *path = NULL;
            set_error(state, ERR_SYNTAX, "Unexpected argument after filename");
            return -1;
        }
    }
    parser->position = (uint16_t)pos;
    parser->current_char = pos < parser->length ? parser->text[pos] : '\0';
    return 0;
}

static int file_error(basic_state_t* state, const char* operation, int error) {
    char message[128];
    snprintf(message, sizeof(message), "%s: %s", operation,
             error ? strerror(error) : "File operation failed");
    set_error(state, ERR_FILE_IO, message);
    return -1;
}

static int load_error(basic_state_t* state, size_t physical_line,
                      error_code_t code, const char* detail) {
    char message[128];
    snprintf(message, sizeof(message), "LOAD line %zu: %s", physical_line, detail);
    set_error(state, code, message);
    return -1;
}

static void free_loaded_lines(program_line_t* line) {
    while (line) {
        program_line_t* next = line->next;
        free(line->text);
        free(line);
        line = next;
    }
}

/* Parse one physical source line without allocating space for arbitrarily
 * long indentation or numeric prefixes. The 72-byte limit applies to text
 * after the line number and separating whitespace. */
static int read_source_line(FILE* file, basic_state_t* state, size_t physical_line,
                            uint16_t* number, char text[MAX_LINE_LENGTH + 1],
                            bool* numbered) {
    unsigned int value = 0;
    size_t length = 0;
    int phase = 0; /* leading whitespace, number, separating whitespace, text */
    bool received = false;
    *numbered = false;
    for (;;) {
        int ch = fgetc(file);
        if (ch == EOF) {
            if (ferror(file))
                return load_error(state, physical_line, ERR_FILE_IO, "Read failed");
            if (!received) return 0;
            break;
        }
        received = true;
        if (ch == '\n') break;
        if (ch == '\r') {
            int next = fgetc(file);
            if (next != '\n' && next != EOF)
                return load_error(state, physical_line, ERR_SYNTAX,
                                  "Unexpected carriage return");
            if (ferror(file))
                return load_error(state, physical_line, ERR_FILE_IO, "Read failed");
            break;
        }
        if (ch == 0)
            return load_error(state, physical_line, ERR_SYNTAX, "NUL byte in source");
        if (phase == 0) {
            if (ch == ' ' || ch == '\t') continue;
            if (ch < '0' || ch > '9')
                return load_error(state, physical_line, ERR_SYNTAX,
                                  "Line number expected");
            *numbered = true;
            phase = 1;
        }
        if (phase == 1) {
            if (ch >= '0' && ch <= '9') {
                unsigned int digit = (unsigned int)(ch - '0');
                if (value > (UINT16_MAX - digit) / 10)
                    return load_error(state, physical_line, ERR_SYNTAX,
                                      "Line number must be 0..65535");
                value = value * 10 + digit;
                continue;
            }
            if (ch != ' ' && ch != '\t')
                return load_error(state, physical_line, ERR_SYNTAX,
                                  "Space expected after line number");
            phase = 2;
        }
        if (phase == 2) {
            if (ch == ' ' || ch == '\t') continue;
            phase = 3;
        }
        if (length == MAX_LINE_LENGTH)
            return load_error(state, physical_line, ERR_SYNTAX, "Program line too long");
        text[length++] = (char)ch;
    }
    *number = (uint16_t)value;
    text[length] = '\0';
    return 1;
}

int basic_load_program(basic_state_t* state, const char* filename) {
    if (!state || !filename) return -1;
    if (state->running || !state->immediate_mode) {
        set_error(state, ERR_ILLEGAL_QUANTITY, "LOAD is only allowed in direct mode");
        return -1;
    }
    FILE* file = fopen(filename, "rb");
    if (!file) return file_error(state, "LOAD", errno);

    basic_state_t pending;
    basic_init(&pending);
    int result = 0;
    size_t physical_line = 1;
    size_t line_count = 0;
    /* Only an initial UTF-8 BOM is stripped. */
    int first = fgetc(file);
    if (first == 0xEF) {
        int second = fgetc(file);
        int third = fgetc(file);
        if (second != 0xBB || third != 0xBF)
            result = load_error(state, 1, ERR_SYNTAX, "Line number expected");
    } else if (first != EOF) {
        ungetc(first, file);
    }
    if (ferror(file)) result = load_error(state, 1, ERR_FILE_IO, "Read failed");

    while (result == 0) {
        uint16_t number = 0;
        char text[MAX_LINE_LENGTH + 1];
        bool numbered = false;
        int status = read_source_line(file, state, physical_line,
                                      &number, text, &numbered);
        if (status <= 0) {
            result = status;
            break;
        }
        if (numbered) {
            bool exists = find_line(&pending, number) != NULL;
            if (text[0] && !exists && line_count == MAX_PROGRAM_LINES) {
                result = load_error(state, physical_line, ERR_OUT_OF_MEMORY,
                                    "Too many program lines");
                break;
            }
            if (add_program_line(&pending, number, text) != 0) {
                result = load_error(state, physical_line, pending.error_code,
                                    pending.error_msg);
                break;
            }
            if (text[0] && !exists) line_count++;
            else if (!text[0] && exists) line_count--;
        }
        physical_line++;
    }
    if (fclose(file) != 0 && result == 0)
        result = load_error(state, physical_line, ERR_FILE_IO, "Close failed");
    if (result != 0) {
        free_loaded_lines(pending.program_start);
        return -1;
    }
    basic_new_program(state);
    state->program_start = pending.program_start;
    return 0;
}

/* Create beside the destination so the final replacement stays on the same
 * filesystem. Exclusive creation protects an existing temporary file. */
static FILE* create_save_temporary(const char* filename, char** temporary) {
    static unsigned long sequence = 0;
    size_t length = strlen(filename);
    if (length > SIZE_MAX - 80) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    *temporary = malloc(length + 80);
    if (!*temporary) {
        errno = ENOMEM;
        return NULL;
    }
    for (unsigned int attempt = 0; attempt < 100; attempt++) {
#ifdef _WIN32
        unsigned long process = (unsigned long)_getpid();
#else
        unsigned long process = (unsigned long)getpid();
#endif
        snprintf(*temporary, length + 80, "%s.tmp.%lu.%lu",
                 filename, process, ++sequence);
#ifdef _WIN32
        int fd = _open(*temporary, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                       _S_IREAD | _S_IWRITE);
#else
        int fd = open(*temporary, O_WRONLY | O_CREAT | O_EXCL, 0666);
#endif
        if (fd < 0) {
            if (errno == EEXIST) continue;
            break;
        }
#ifdef _WIN32
        FILE* file = _fdopen(fd, "wb");
#else
        FILE* file = fdopen(fd, "wb");
#endif
        if (file) return file;
        int error = errno;
#ifdef _WIN32
        _close(fd);
#else
        close(fd);
#endif
        remove(*temporary);
        errno = error;
        break;
    }
    free(*temporary);
    *temporary = NULL;
    return NULL;
}

int basic_save_program(basic_state_t* state, const char* filename) {
    if (!state || !filename) return -1;
    char* temporary = NULL;
    FILE* file = create_save_temporary(filename, &temporary);
    if (!file) return file_error(state, "SAVE", errno);
    int error = 0;
    for (program_line_t* line = state->program_start; line; line = line->next) {
        if (fprintf(file, "%u %s\n", (unsigned int)line->line_number, line->text) < 0) {
            error = errno ? errno : EIO;
            break;
        }
    }
    if (fclose(file) != 0 && !error) error = errno ? errno : EIO;
    if (error) {
        remove(temporary);
        free(temporary);
        return file_error(state, "SAVE", error);
    }
#ifdef _WIN32
    if (!MoveFileExA(temporary, filename,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        char message[128];
        snprintf(message, sizeof(message), "SAVE: Cannot replace file (Windows error %lu)",
                 (unsigned long)GetLastError());
        remove(temporary);
        free(temporary);
        set_error(state, ERR_FILE_IO, message);
        return -1;
    }
#else
    if (rename(temporary, filename) != 0) {
        error = errno;
        remove(temporary);
        free(temporary);
        return file_error(state, "SAVE", error);
    }
#endif
    free(temporary);
    return 0;
}

int cmd_load(basic_state_t* state, parser_state_t* parser) {
    char* path;
    if (parse_file_argument(state, parser, false, &path) != 0) return -1;
    int result = basic_load_program(state, path);
    free(path);
    return result;
}

int cmd_save(basic_state_t* state, parser_state_t* parser) {
    char* path;
    if (parse_file_argument(state, parser, false, &path) != 0) return -1;
    int result = basic_save_program(state, path);
    free(path);
    return result;
}

static char* copy_text(const char* text) {
    size_t length = strlen(text) + 1;
    char* copy = malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

static char* join_path(const char* directory, const char* name) {
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    if (directory_length > SIZE_MAX - name_length - 2) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    char* path = malloc(directory_length + name_length + 2);
    if (!path) {
        errno = ENOMEM;
        return NULL;
    }
    memcpy(path, directory, directory_length);
    size_t offset = directory_length;
#ifdef _WIN32
    if (!offset || (directory[offset - 1] != '/' && directory[offset - 1] != '\\'))
        path[offset++] = '\\';
#else
    if (!offset || directory[offset - 1] != '/') path[offset++] = '/';
#endif
    memcpy(path + offset, name, name_length + 1);
    return path;
}

static char* absolute_directory(const char* directory) {
#ifdef _WIN32
    DWORD length = GetFullPathNameA(directory, 0, NULL, NULL);
    if (!length) {
        errno = EINVAL;
        return NULL;
    }
    char* absolute = malloc(length);
    if (!absolute) {
        errno = ENOMEM;
        return NULL;
    }
    DWORD written = GetFullPathNameA(directory, length, absolute, NULL);
    if (!written || written >= length) {
        free(absolute);
        errno = EINVAL;
        return NULL;
    }
    return absolute;
#else
    if (directory[0] == '/') {
        char* copy = copy_text(directory);
        if (!copy) errno = ENOMEM;
        return copy;
    }
    size_t capacity = 256;
    for (;;) {
        char* current = malloc(capacity);
        if (!current) {
            errno = ENOMEM;
            return NULL;
        }
        if (getcwd(current, capacity)) {
            if (strcmp(directory, ".") == 0) return current;
            char* path = join_path(current, directory);
            free(current);
            return path;
        }
        int error = errno;
        free(current);
        if (error != ERANGE || capacity > SIZE_MAX / 2) {
            errno = error;
            return NULL;
        }
        capacity *= 2;
    }
#endif
}

static bool is_basic_filename(const char* name) {
    size_t length = strlen(name);
    if (length < 4) return false;
    const char* suffix = name + length - 4;
    return suffix[0] == '.' && (suffix[1] == 'b' || suffix[1] == 'B') &&
           (suffix[2] == 'a' || suffix[2] == 'A') &&
           (suffix[3] == 's' || suffix[3] == 'S');
}

typedef struct {
    char** names;
    size_t count;
    size_t capacity;
} file_list_t;

static int append_filename(file_list_t* list, const char* name) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        if (capacity < list->capacity || capacity > SIZE_MAX / sizeof(char*)) {
            errno = ENOMEM;
            return -1;
        }
        char** names = realloc(list->names, capacity * sizeof(char*));
        if (!names) {
            errno = ENOMEM;
            return -1;
        }
        list->names = names;
        list->capacity = capacity;
    }
    char* copy = copy_text(name);
    if (!copy) {
        errno = ENOMEM;
        return -1;
    }
    list->names[list->count++] = copy;
    return 0;
}

static int compare_filenames(const void* left, const void* right) {
    return strcmp(*(const char* const*)left, *(const char* const*)right);
}

int cmd_files(basic_state_t* state, parser_state_t* parser) {
    char* argument;
    if (parse_file_argument(state, parser, true, &argument) != 0) return -1;
    char* directory = absolute_directory(argument ? argument : ".");
    free(argument);
    if (!directory) return file_error(state, "FILES", errno);
    file_list_t list = {0};
    int error = 0;
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(directory);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        free(directory);
        return file_error(state, "FILES", ENOENT);
    }
    char* pattern = join_path(directory, "*");
    if (!pattern) {
        error = errno;
    } else {
        WIN32_FIND_DATAA entry;
        HANDLE search = FindFirstFileA(pattern, &entry);
        free(pattern);
        if (search == INVALID_HANDLE_VALUE) {
            if (GetLastError() != ERROR_FILE_NOT_FOUND) error = EIO;
        } else {
            do {
                if (!(entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) &&
                    is_basic_filename(entry.cFileName) &&
                    append_filename(&list, entry.cFileName) != 0) {
                    error = errno;
                    break;
                }
            } while (FindNextFileA(search, &entry));
            if (!error && GetLastError() != ERROR_NO_MORE_FILES) error = EIO;
            if (!FindClose(search) && !error) error = EIO;
        }
    }
#else
    DIR* handle = opendir(directory);
    if (!handle) {
        error = errno;
    } else {
        for (;;) {
            errno = 0;
            struct dirent* entry = readdir(handle);
            if (!entry) {
                error = errno;
                break;
            }
            if (!is_basic_filename(entry->d_name)) continue;
            char* path = join_path(directory, entry->d_name);
            if (!path) {
                error = errno;
                break;
            }
            struct stat info;
            int status = stat(path, &info);
            free(path);
            if (status != 0 || !S_ISREG(info.st_mode)) continue;
            if (append_filename(&list, entry->d_name) != 0) {
                error = errno;
                break;
            }
        }
        if (closedir(handle) != 0 && !error) error = errno;
    }
#endif
    if (!error) {
        if (list.count > 1)
            qsort(list.names, list.count, sizeof(char*), compare_filenames);
        printf("FILES %s\n", directory);
        for (size_t i = 0; i < list.count; i++) printf("%s\n", list.names[i]);
        if (list.count == 0) printf("No BASIC files found.\n");
        state->trmpos = 0;
    }
    for (size_t i = 0; i < list.count; i++) free(list.names[i]);
    free(list.names);
    free(directory);
    return error ? file_error(state, "FILES", error) : 0;
}
