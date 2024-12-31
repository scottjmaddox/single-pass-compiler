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
#define MAX_TOKEN_LOOKAHEAD 2

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

static char *TOKEN_NAMES[] = {
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

struct token {
    enum TOKEN kind;
    size_t start_src_pos;
    size_t end_src_pos;
};

struct token_diagnostics {
    size_t start_src_line;
    size_t start_src_col;
    char *line;
    size_t line_len;
};

struct context {
    char *file_path;
    char *src;
    size_t src_len;
    size_t src_pos;
    // Token ring buffers
    size_t token_offset;
    size_t token_count;
    size_t token_start_src_pos[MAX_TOKEN_LOOKAHEAD];
    size_t token_end_src_pos[MAX_TOKEN_LOOKAHEAD];
    enum TOKEN token_kinds[MAX_TOKEN_LOOKAHEAD];
};

static void
init_context(struct context *ctx, char *file_path, char *src, size_t src_len) {
    ctx->file_path = file_path;
    ctx->src = src;
    ctx->src_len = src_len;
    ctx->src_pos = 0;
    ctx->token_offset = 0;
    ctx->token_count = 0;
}

static void
push_token(struct context *ctx, enum TOKEN token_kind, size_t token_start_src_pos, size_t token_end_src_pos) {
    assert(ctx->token_count < MAX_TOKEN_LOOKAHEAD);
    size_t i = (ctx->token_offset + ctx->token_count) % MAX_TOKEN_LOOKAHEAD;
    ctx->token_kinds[i] = token_kind;
    ctx->token_start_src_pos[i] = token_start_src_pos;
    ctx->token_end_src_pos[i] = token_end_src_pos;
    ctx->token_count += 1;
}

static void
fill_tokens(struct context *ctx) {
    char *src = ctx->src;
    size_t src_len = ctx->src_len;
    size_t i = ctx->src_pos;
    size_t start;
    while (ctx->token_count < MAX_TOKEN_LOOKAHEAD) {
        if (i >= src_len) {
            push_token(ctx, TOKEN_EOF, i, i);
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
            push_token(ctx, TOKEN_LEFT_PAREN, start, i);
            break;
        case ')':
            i += 1;
            push_token(ctx, TOKEN_RIGHT_PAREN, start, i);
            break;
        case '{':
            i += 1;
            push_token(ctx, TOKEN_LEFT_BRACE, start, i);
            break;
        case '}':
            i += 1;
            push_token(ctx, TOKEN_RIGHT_BRACE, start, i);
            break;
        case '-':
            i += 1;
            if (i < src_len && src[i] == '>') {
                i += 1;
                push_token(ctx, TOKEN_RIGHT_ARROW, start, i);
                break;
            }
            push_token(ctx, TOKEN_UNKNOWN, start, i);
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
                    push_token(ctx, TOKEN_KEYWORD_FN, start, i);
                    break;
                }
                // fallthrough
            default:
                push_token(ctx, TOKEN_IDENT, start, i);
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
            push_token(ctx, TOKEN_LITERAL, start, i);
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
                        break;
                    default:
                        i += 1;
                        goto unknown_loop;
                }
                break;
            }
            push_token(ctx, TOKEN_UNKNOWN, start, i);
            break;
        }
    }
    ctx->src_pos = i;
}

static void
take_token(struct context *ctx, struct token* token) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    size_t i = ctx->token_offset;
    token->kind = ctx->token_kinds[i];
    token->start_src_pos = ctx->token_start_src_pos[i];
    token->end_src_pos = ctx->token_end_src_pos[i];
    ctx->token_offset = (i + 1) % MAX_TOKEN_LOOKAHEAD;
    ctx->token_count -= 1;
}

// static enum TOKEN
// peek_token_kind_at(struct context *ctx, size_t i) {
//     assert(i < MAX_TOKEN_LOOKAHEAD);
//     if (i >= ctx->token_count) { fill_tokens(ctx); }
//     return ctx->token_kinds[(ctx->token_offset + i) % MAX_TOKEN_LOOKAHEAD];
// }

static enum TOKEN
peek_token_kind(struct context *ctx) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    return ctx->token_kinds[ctx->token_offset];
}

static struct token_diagnostics
get_token_diagnostics(struct context *ctx, struct token tok) {
    struct token_diagnostics diag = {
        .start_src_line = 1,
        .start_src_col = 1,
        .line = NULL,
        .line_len = 0,
    };
    size_t line_start_pos = 0;
    for (size_t i = 0; i < tok.start_src_pos; i++) {
        switch (ctx->src[i]) {
        case '\n':
            diag.start_src_line += 1;
            diag.start_src_col = 1;
            line_start_pos = i + 1;
            break;
        default:
            diag.start_src_col += 1;
            break;
        }
    }

    size_t line_end_pos = line_start_pos;
    while (line_end_pos < ctx->src_len && ctx->src[line_end_pos] != '\n') {
        line_end_pos += 1;
    }
    diag.line = ctx->src + line_start_pos;
    diag.line_len = line_end_pos - line_start_pos;
    return diag;
}

static void
expected(struct context *ctx, enum TOKEN *token_kinds, size_t token_kinds_len) {
    struct token tok;
    take_token(ctx, &tok);
    struct token_diagnostics diag = get_token_diagnostics(ctx, tok);
    fprintf(stderr,
        "error: unexpected token %s\n"
        " --> %s:%zu:%zu\n"
        "  |\n"
        "  | %.*s\n",
        TOKEN_NAMES[tok.kind], ctx->file_path, diag.start_src_line, diag.start_src_col,
        (int)diag.line_len, diag.line
    );
    fprintf(stderr, "  | ");
    for (size_t i = 0; i < diag.start_src_col - 1; i++) {
        if (diag.line[i] == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }
    for (size_t i = 0; i < tok.end_src_pos - tok.start_src_pos; i++) { fputc('^', stderr); }
    fprintf(stderr, "\n");
    fprintf(stderr, "Expected one of: ");
    for (size_t i = 0; i < token_kinds_len; i++) {
        fprintf(stderr, "%s", TOKEN_NAMES[token_kinds[i]]);
        if (i < token_kinds_len - 1) {
            fprintf(stderr, ", ");
        }
    }
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static void
expect(struct context *ctx, enum TOKEN token_kind, struct token *tok_out) {
    struct token tok;
    take_token(ctx, &tok);
    if (tok.kind != token_kind) {
        expected(ctx, &token_kind, 1);
    }
    if (tok_out != NULL) {
        *tok_out = tok;
    }
}

static void
do_literal(struct context *ctx) {
    struct token tok;
    expect(ctx, TOKEN_LITERAL, &tok);
    char *lit = ctx->src + tok.start_src_pos;
    size_t lit_len = tok.end_src_pos - tok.start_src_pos;
    printf("\tmov\tw0, #%.*s\n", (int)lit_len, lit);
}

static void
do_fn_call(struct context *ctx) {
    // EBNF: fn_call = ident "(" ")" ;
    struct token tok;
    expect(ctx, TOKEN_IDENT, &tok);
    char *name = ctx->src + tok.start_src_pos;
    size_t name_len = tok.end_src_pos - tok.start_src_pos;
    printf("\tbl\t_%.*s\n", (int)name_len, name);
    expect(ctx, TOKEN_LEFT_PAREN, NULL);
    expect(ctx, TOKEN_RIGHT_PAREN, NULL);
}

static void
do_expr(struct context *ctx) {
    // EBNF: expr = fn_call | literal ;
    switch (peek_token_kind(ctx)) {
    case TOKEN_IDENT: do_fn_call(ctx); break;
    case TOKEN_LITERAL: do_literal(ctx); break;
    default: expected(ctx, (enum TOKEN[]){TOKEN_IDENT, TOKEN_LITERAL}, 2);
    }
}

static void
do_block(struct context *ctx) {
    // EBNF: block = "{" expr "}" ;
    expect(ctx, TOKEN_LEFT_BRACE, NULL);
    do_expr(ctx);
    expect(ctx, TOKEN_RIGHT_BRACE, NULL);
}

static void
do_fn_def(struct context *ctx) {
    // EBNF: fn_def = "fn" ident "(" ")" "->" ident block ;
    struct token tok;
    expect(ctx, TOKEN_KEYWORD_FN, NULL);
    expect(ctx, TOKEN_IDENT, &tok);
    char *name = ctx->src + tok.start_src_pos;
    size_t name_len = tok.end_src_pos - tok.start_src_pos;
    printf(
        "\n"
        "\t.globl\t_%.*s\n"
        "\t.p2align\t2\n"
        "_%.*s:\n"
        "\t.cfi_startproc\n"
        "\tstp\tx29, x30, [sp, #-16]!\n"
        "\n",
        (int)name_len, name,
        (int)name_len, name
    );
    expect(ctx, TOKEN_LEFT_PAREN, NULL);
    expect(ctx, TOKEN_RIGHT_PAREN, NULL);
    expect(ctx, TOKEN_RIGHT_ARROW, NULL);
    expect(ctx, TOKEN_IDENT, &tok);
    // TODO: intern the type, and pass it into do_block to type check
    do_block(ctx);
    printf(
        "\n"
        "\tldp\tx29, x30, [sp], #16\n"
        "\tret\n"
        "\t.cfi_endproc\n"
    );
}

static void
do_program(struct context *ctx) {
    printf(
        "\t.section\t__TEXT,__text,regular,pure_instructions\n"
    );
    // EBNF: program = { fn_def } EOF ;
    for (;;) {
        switch (peek_token_kind(ctx)) {
        case TOKEN_KEYWORD_FN: do_fn_def(ctx); break;
        case TOKEN_EOF: return;
        default: expected(ctx, (enum TOKEN[]){TOKEN_KEYWORD_FN, TOKEN_EOF}, 2);
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

    struct context ctx;
    init_context(&ctx, file_path, (char *)mapped, file_size);
    do_program(&ctx);

    if (munmap(mapped, file_size) == -1) {
        fprintf(stderr, "Error unmapping file '%s': %s\n", file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return 0;
}
