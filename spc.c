#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      // For open()
#include <sys/mman.h>   // For mmap(), munmap()
#include <sys/stat.h>   // For fstat()
#include <unistd.h>     // For close()
#include <errno.h>      // For errno
#include <string.h>     // For strerror()
#include <assert.h>     // For assert()

#define MAX_IDENT_SIZE 256

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

struct Token {
    enum TOKEN kind;
    size_t start_src_pos;
    size_t end_src_pos;
};

#define TOKEN_BUF_SIZE 2

struct Lexer {
    char *src;
    size_t src_len;
    size_t src_pos;
    // Token ring buffers
    size_t token_offset;
    size_t token_count;
    size_t token_start_src_pos[TOKEN_BUF_SIZE];
    size_t token_end_src_pos[TOKEN_BUF_SIZE];
    enum TOKEN token_kinds[TOKEN_BUF_SIZE];
};

static void
Lexer_init(struct Lexer *lexer, char *src, size_t src_len) {
    lexer->src = src;
    lexer->src_len = src_len;
    lexer->src_pos = 0;
    lexer->token_offset = 0;
    lexer->token_count = 0;
}

static void
Lexer_push_back(struct Lexer *lexer, enum TOKEN token_kind, size_t token_start_src_pos, size_t token_end_src_pos) {
    assert(lexer->token_count < TOKEN_BUF_SIZE);
    size_t i = (lexer->token_offset + lexer->token_count) % TOKEN_BUF_SIZE;
    lexer->token_kinds[i] = token_kind;
    lexer->token_start_src_pos[i] = token_start_src_pos;
    lexer->token_end_src_pos[i] = token_end_src_pos;
    lexer->token_count += 1;
}

static void
Lexer_fill(struct Lexer *lexer) {
    char *src = lexer->src;
    size_t src_len = lexer->src_len;
    size_t i = lexer->src_pos;
    size_t start;
    while (lexer->token_count < TOKEN_BUF_SIZE) {
        if (i >= src_len) {
            Lexer_push_back(lexer, TOKEN_EOF, i, i);
            continue;
        }
        start = i;
        switch (src[i]) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            i += 1;
            whitespace_loop:
            while (i < src_len) {
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
            Lexer_push_back(lexer, TOKEN_LEFT_PAREN, start, i);
            break;
        case ')':
            i += 1;
            Lexer_push_back(lexer, TOKEN_RIGHT_PAREN, start, i);
            break;
        case '{':
            i += 1;
            Lexer_push_back(lexer, TOKEN_LEFT_BRACE, start, i);
            break;
        case '}':
            i += 1;
            Lexer_push_back(lexer, TOKEN_RIGHT_BRACE, start, i);
            break;
        case '-':
            i += 1;
            if (i < src_len && src[i] == '>') {
                i += 1;
                Lexer_push_back(lexer, TOKEN_RIGHT_ARROW, start, i);
                break;
            }
            Lexer_push_back(lexer, TOKEN_UNKNOWN, start, i);
            break;
        case '_':
        case 'A' ... 'Z':
        case 'a' ... 'z':
            i += 1;
            ident_loop:
            while (i < src_len) {
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
                    Lexer_push_back(lexer, TOKEN_KEYWORD_FN, start, i);
                    break;
                }
                // fallthrough
            default:
                Lexer_push_back(lexer, TOKEN_IDENT, start, i);
                break;
            }
            break;
        case '0' ... '9':
            i += 1;
            literal_loop:
            while (i < src_len) {
                switch (src[i]) {
                case '_':
                case '0' ... '9':
                    i += 1;
                    goto literal_loop;
                }
                break;
            }
            Lexer_push_back(lexer, TOKEN_LITERAL, start, i);
            break;
        default:
            i += 1;
            unknown_loop:
            while (i < src_len) {
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
            Lexer_push_back(lexer, TOKEN_UNKNOWN, start, i);
            break;
        }
    }
    lexer->src_pos = i;
}

static void
Lexer_take(struct Lexer *lexer, struct Token* token) {
    if (lexer->token_count == 0) { Lexer_fill(lexer); }
    size_t i = lexer->token_offset;
    token->kind = lexer->token_kinds[i];
    token->start_src_pos = lexer->token_start_src_pos[i];
    token->end_src_pos = lexer->token_end_src_pos[i];
    lexer->token_offset = (i + 1) % TOKEN_BUF_SIZE;
    lexer->token_count -= 1;
}

// static enum TOKEN
// Lexer_peek_at(struct Lexer *lexer, size_t i) {
//     assert(i < TOKEN_BUF_SIZE);
//     if (i >= lexer->token_count) { Lexer_fill(lexer); }
//     return lexer->token_kinds[(lexer->token_offset + i) % TOKEN_BUF_SIZE];
// }

static enum TOKEN
Lexer_peek(struct Lexer *lexer) {
    if (lexer->token_count == 0) { Lexer_fill(lexer); }
    return lexer->token_kinds[lexer->token_offset];
}

static void
expected(struct Lexer *lexer, enum TOKEN *token_kinds, size_t token_kinds_len) {
    struct Token token;
    Lexer_take(lexer, &token);
    // TODO: improve error message
    fprintf(stderr,
        "Syntax Error: unexpected token %s at %zu-%zu\n"
        "Expected one of: ",
        TOKEN_STRING[token.kind], token.start_src_pos, token.end_src_pos);
    for (size_t i = 0; i < token_kinds_len; i++) {
        fprintf(stderr, "%s", TOKEN_STRING[token_kinds[i]]);
        if (i < token_kinds_len - 1) {
            fprintf(stderr, ", ");
        }
    }
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static void
expect(struct Lexer *lexer, enum TOKEN token_kind, struct Token *token_out) {
    struct Token token;
    Lexer_take(lexer, &token);
    if (token.kind != token_kind) {
        expected(lexer, &token_kind, 1);
    }
    if (token_out != NULL) {
        *token_out = token;
    }
}

static void
do_literal(struct Lexer *lexer) {
    struct Token token;
    expect(lexer, TOKEN_LITERAL, &token);
    char *lit = lexer->src + token.start_src_pos;
    size_t lit_len = token.end_src_pos - token.start_src_pos;
    printf("\tmov\tw0, #%.*s\n", (int)lit_len, lit);
}

static void
do_expr(struct Lexer *lexer) {
    // EBNF: expr = literal ;
   do_literal(lexer);
}

static void
do_block(struct Lexer *lexer) {
    // EBNF: block = "{" expr "}" ;
    expect(lexer, TOKEN_LEFT_BRACE, NULL);
    do_expr(lexer);
    expect(lexer, TOKEN_RIGHT_BRACE, NULL);
}

static void
do_fn_def(struct Lexer *lexer) {
    // EBNF: fn_def = "fn" ident "(" ")" "->" ident block ;
    struct Token token;
    expect(lexer, TOKEN_KEYWORD_FN, NULL);
    expect(lexer, TOKEN_IDENT, &token);
    char *name = lexer->src + token.start_src_pos;
    size_t name_len = token.end_src_pos - token.start_src_pos;
    printf(
        "\t.section\t__TEXT,__text,regular,pure_instructions\n"
        "\t.globl\t_%.*s\n"
        "\t.p2align\t2\n"
        "_%.*s:\n"
        "\t.cfi_startproc\n",
        (int)name_len, name,
        (int)name_len, name
    );
    expect(lexer, TOKEN_LEFT_PAREN, NULL);
    expect(lexer, TOKEN_RIGHT_PAREN, NULL);
    expect(lexer, TOKEN_RIGHT_ARROW, NULL);
    expect(lexer, TOKEN_IDENT, &token);
    // TODO: intern the type, and pass it into do_block to type check
    do_block(lexer);
    printf(
        "\tret\n"
        "\t.cfi_endproc\n"
    );
}

static void
do_program(struct Lexer *lexer) {
    // EBNF: program = { fn_def } EOF ;
    for (;;) {
        switch (Lexer_peek(lexer)) {
        case TOKEN_KEYWORD_FN: do_fn_def(lexer); break;
        case TOKEN_EOF: return;
        default: expected(lexer, (enum TOKEN[]){TOKEN_KEYWORD_FN, TOKEN_EOF}, 2);
        }
    }
}

int
main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *file_path = argv[1];

    // Open the file
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file '%s': %s\n", file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Get the file size
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "Error getting file size for '%s': %s\n", file_path, strerror(errno));
        if (close(fd) == -1) { fprintf(stderr, "Error closing file '%s': %s\n", file_path, strerror(errno)); }
        exit(EXIT_FAILURE);
    }
    if (sb.st_size == 0) {
        fprintf(stderr, "Error: file '%s' is empty.\n", file_path);
        if (close(fd) == -1) { fprintf(stderr, "Error closing file '%s': %s\n", file_path, strerror(errno)); }
        exit(EXIT_FAILURE);
    }
    size_t file_size = sb.st_size;

    // Memory-map the file
    void *mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        fprintf(stderr, "Error mapping file '%s': %s\n", file_path, strerror(errno));
        if (close(fd) == -1) { fprintf(stderr, "Error closing file '%s': %s\n", file_path, strerror(errno)); }
        exit(EXIT_FAILURE);
    }
    if (close(fd) == -1) {
        fprintf(stderr, "Error closing file '%s': %s\n", file_path, strerror(errno));
        // Continue even if close fails
    }

    struct Lexer lexer;
    Lexer_init(&lexer, (char *)mapped, file_size);
    do_program(&lexer);

    if (munmap(mapped, file_size) == -1) {
        fprintf(stderr, "Error unmapping file '%s': %s\n", file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return 0;
}
