#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

enum ERROR {
    ERROR_OK = 0,
    ERROR_INVALID_ARG,
    ERROR_MALLOC,
    ERROR_FILE_OPEN,
    ERROR_FILE_SEEK,
    ERROR_FILE_TELL,
    ERROR_FILE_READ,
};

static char *ERROR_MESSAGE[] = {
    "No error",
    "Invalid argument",
    "Memory allocation failed",
    "Failed to open file",
    "Failed to seek file",
    "Failed to tell file",
    "Failed to read file",
};

enum TOKEN {
    TOKEN_EOF = 0,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_RIGHT_ARROW,
    TOKEN_KEYWORD_FN,
    TOKEN_IDENT,
    TOKEN_LITERAL,
    TOKEN_UNKNOWN,
};

static char *TOKEN_STRING[] = {
    "EOF",
    "LEFT_PAREN",
    "RIGHT_PAREN",
    "LEFT_BRACE",
    "RIGHT_BRACE",
    "RIGHT_ARROW",
    "KEYWORD_FN",
    "IDENT",
    "LITERAL",
    "UNKNOWN",
};

struct Tokens {
    size_t len;
    size_t cap;
    enum TOKEN *token_kinds;
    size_t *token_starts;
    size_t *token_ends;
};

static enum ERROR
Tokens_init(struct Tokens *tokens) {
    size_t new_cap = 1024;
    void *mem;

    mem = malloc(new_cap * sizeof(enum TOKEN));
    if (mem == NULL) { goto error1; }
    tokens->token_kinds = (enum TOKEN *)mem;

    mem = malloc(new_cap * sizeof(size_t));
    if (mem == NULL) { goto error2; }
    tokens->token_starts = (size_t *)mem;

    mem = malloc(new_cap * sizeof(size_t));
    if (mem == NULL) { goto error3; }
    tokens->token_ends = (size_t *)mem;

    tokens->len = 0;
    tokens->cap = new_cap;
    return ERROR_OK;

error3:
    free(tokens->token_starts);
    tokens->token_starts = NULL;
error2:
    free(tokens->token_kinds);
    tokens->token_kinds = NULL;
error1:
    return ERROR_MALLOC;
}

static void
Tokens_free(struct Tokens *tokens) {
    free(tokens->token_kinds);
    free(tokens->token_starts);
    free(tokens->token_ends);
}

static enum ERROR
Tokens_push(struct Tokens *tokens, enum TOKEN token_kind, size_t token_start, size_t token_end) {
    size_t new_cap;
    void *mem;
    if (tokens->len == tokens->cap) {
        new_cap = tokens->cap * 2;

        mem = realloc(tokens->token_kinds, new_cap * sizeof(enum TOKEN));
        if (mem == NULL) { return ERROR_MALLOC; }
        tokens->token_kinds = (enum TOKEN *)mem;

        mem = realloc(tokens->token_starts, new_cap * sizeof(size_t));
        if (mem == NULL) { return ERROR_MALLOC; }
        tokens->token_starts = (size_t *)mem;

        mem = realloc(tokens->token_ends, new_cap * sizeof(size_t));
        if (mem == NULL) { return ERROR_MALLOC; }
        tokens->token_ends = (size_t *)mem;

        tokens->cap = new_cap;
    }
    tokens->token_kinds[tokens->len] = token_kind;
    tokens->token_starts[tokens->len] = token_start;
    tokens->token_ends[tokens->len] = token_end;
    tokens->len += 1;
    return ERROR_OK;
}

static void
Tokens_fprint(struct Tokens *tokens, FILE *file) {
    size_t i;
    for (i = 0; i < tokens->len; i++) {
        fprintf(file, "%s %zu-%zu\n",
            TOKEN_STRING[tokens->token_kinds[i]],
            tokens->token_starts[i],
            tokens->token_ends[i]);
    }
}

static struct Tokens
lex(size_t len, char *src) {
    struct Tokens tokens;
    size_t i = 0;
    size_t start = 0;
    Tokens_init(&tokens);
    while (i < len) {
        start = i;
        switch (src[i]) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            i += 1;
            whitespace_loop:
            while (i < len) {
                switch (src[i]) {
                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    i += 1;
                    goto whitespace_loop;
                }
                break;
            }
            break;
        case '(':
            i += 1;
            Tokens_push(&tokens, TOKEN_LEFT_PAREN, start, i);
            break;
        case ')':
            i += 1;
            Tokens_push(&tokens, TOKEN_RIGHT_PAREN, start, i);
            break;
        case '{':
            i += 1;
            Tokens_push(&tokens, TOKEN_LEFT_BRACE, start, i);
            break;
        case '}':
            i += 1;
            Tokens_push(&tokens, TOKEN_RIGHT_BRACE, start, i);
            break;
        case '-':
            i += 1;
            if (i < len && src[i] == '>') {
                i += 1;
                Tokens_push(&tokens, TOKEN_RIGHT_ARROW, start, i);
                break;
            }
            Tokens_push(&tokens, TOKEN_UNKNOWN, start, i);
            break;
        case '_':
        case 'A' ... 'Z':
        case 'a' ... 'z':
            i += 1;
            ident_loop:
            while (i < len) {
                switch (src[i]) {
                case '_':
                case 'A' ... 'Z':
                case 'a' ... 'z':
                case '0' ... '9':
                    i += 1;
                    goto ident_loop;
                }
                break;
            }
            switch (i - start) {
            case 2:
                if (src[start] == 'f' && src[start + 1] == 'n') {
                    Tokens_push(&tokens, TOKEN_KEYWORD_FN, start, i);
                    break;
                }
                // fallthrough
            default:
                Tokens_push(&tokens, TOKEN_IDENT, start, i);
                break;
            }
            break;
        case '0' ... '9':
            i += 1;
            literal_loop:
            while (i < len) {
                switch (src[i]) {
                case '_':
                case '0' ... '9':
                    i += 1;
                    goto literal_loop;
                }
                break;
            }
            Tokens_push(&tokens, TOKEN_LITERAL, start, i);
            break;
        default:
            i += 1;
            unknown_loop:
            while (i < len) {
                switch (src[i]) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '(':
                    case ')':
                    case '{':
                    case '}':
                    case '-':
                    case '>':
                    case '_':
                    case 'A' ... 'Z':
                    case 'a' ... 'z':
                    case '0' ... '9':
                        i += 1;
                        goto unknown_loop;
                }
                break;
            }
            Tokens_push(&tokens, TOKEN_UNKNOWN, start, i);
            break;
        }
    }
    Tokens_push(&tokens, TOKEN_EOF, i, i);
    return tokens;
}

static enum ERROR
read_file(const char *path, size_t *len, char **src) {
    FILE *fp;
    long file_len;
    size_t file_size;
    char *buffer;
    size_t read_size;

    assert(path != NULL);
    assert(len != NULL);
    assert(src != NULL);
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return ERROR_FILE_OPEN;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return ERROR_FILE_SEEK;
    }
    file_len = ftell(fp);
    if (file_len == -1L) {
        fclose(fp);
        return ERROR_FILE_TELL;
    }
    file_size = (size_t)file_len;
    rewind(fp);
    buffer = (char *)malloc(file_size);
    if (buffer == NULL) {
        fclose(fp);
        return ERROR_MALLOC;
    }
    read_size = fread(buffer, 1, file_size, fp);
    if (read_size != file_size) {
        fclose(fp);
        free(buffer);
        return ERROR_FILE_READ;
    }
    fclose(fp);
    *len = read_size;
    *src = buffer;
    return ERROR_OK;
}

int
main(int argc, char *argv[]) {
    char *file_path;
    size_t len;
    char *src;
    enum ERROR err;
    struct Tokens tokens;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return ERROR_INVALID_ARG;
    }
    file_path = argv[1];

    err = read_file(file_path, &len, &src);
    if (err != ERROR_OK) {
        fprintf(stderr, "Error: %s\n", ERROR_MESSAGE[err]);
        return (int)err;
    }
    tokens = lex(len, src);
    Tokens_fprint(&tokens, stdout);

    Tokens_free(&tokens);
    free(src);
    return 0;
}
