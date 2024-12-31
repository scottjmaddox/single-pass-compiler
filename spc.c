#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      // For open()
#include <sys/mman.h>   // For mmap(), munmap()
#include <sys/stat.h>   // For fstat()
#include <unistd.h>     // For getopt(), close()
#include <errno.h>      // For errno
#include <string.h>     // For strerror()
#include <assert.h>     // For assert()

#define MAX_TOKEN_LOOKAHEAD 2
#define MAX_LITERAL_LEN 256

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

struct str {
    size_t len;
    char *ptr;
};

struct token {
    enum TOKEN kind;
    size_t idx;
    size_t len;
    size_t line;
    size_t col;
};

struct context {
    FILE *output_file;
    char *input_file_path;
    char *src;
    size_t src_len;
    size_t src_idx;
    // Token ring buffer
    size_t token_offset;
    size_t token_count;
    struct token tokens[MAX_TOKEN_LOOKAHEAD];
};

static struct token
lex(char *src, size_t src_len, size_t from_idx) {
    struct token tok = { .kind = TOKEN_UNKNOWN, .idx = from_idx, .len = 1, .line = 1, .col = 1 };
    size_t i;
    while (tok.idx < src_len) {
        switch (src[tok.idx]) {
        case ' ':
        case '\t':
            tok.idx += 1;
            tok.col += 1;
            continue;
        case '\n':
            tok.idx += 1;
            tok.line += 1;
            tok.col = 1;
            continue;
        case '(':
            tok.kind = TOKEN_LEFT_PAREN;
            return tok;
        case ')':
            tok.kind = TOKEN_RIGHT_PAREN;
            return tok;
        case '{':
            tok.kind = TOKEN_LEFT_BRACE;
            return tok;
        case '}':
            tok.kind = TOKEN_RIGHT_BRACE;
            return tok;
        case '-':
            if (tok.idx + 1 < src_len) {
                switch (src[tok.idx + 1]) {
                case '>':
                    tok.kind = TOKEN_RIGHT_ARROW;
                    tok.len = 2;
                    return tok;
                }
            }
            tok.kind = TOKEN_UNKNOWN;
            return tok;
        case '_':
        case 'A' ... 'Z':
        case 'a' ... 'z':
            i = tok.idx + 1;
            while (i < src_len) {
                switch (src[i]) {
                case '_':
                case 'A' ... 'Z':
                case 'a' ... 'z':
                case '0' ... '9':
                    i += 1;
                    continue;
                }
                break;
            }
            tok.len = i - tok.idx;
            switch (tok.len) {
            case 2:
                if (src[tok.idx] == 'f' && src[tok.idx + 1] == 'n') {
                    tok.kind = TOKEN_KEYWORD_FN;
                    return tok;
                }
                break;
            }
            tok.kind = TOKEN_IDENT;
            return tok;
        case '0' ... '9':
            i = tok.idx + 1;
            while (i < src_len) {
                switch (src[i]) {
                case '_':
                case '0' ... '9':
                    i += 1;
                    continue;
                }
                break;
            }
            tok.kind = TOKEN_LITERAL;
            tok.len = i - tok.idx;
            return tok;
        }
        tok.kind = TOKEN_UNKNOWN;
        return tok;
    }
    tok.kind = TOKEN_EOF;
    tok.len = 0;
    return tok;
}

static void
push_token(struct context *ctx, struct token tok) {
    assert(ctx->token_count < MAX_TOKEN_LOOKAHEAD);
    ctx->tokens[(ctx->token_offset + ctx->token_count) % MAX_TOKEN_LOOKAHEAD] = tok;
    ctx->token_count += 1;
    ctx->src_idx = tok.idx + tok.len;
}

static void
fill_tokens(struct context *ctx) {
    while (ctx->token_count < MAX_TOKEN_LOOKAHEAD) {
        struct token tok = lex(ctx->src, ctx->src_len, ctx->src_idx);
        push_token(ctx, tok);
    }
}

static void
take_token(struct context *ctx, struct token* token) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    *token = ctx->tokens[ctx->token_offset];
    ctx->token_offset = (ctx->token_offset + 1) % MAX_TOKEN_LOOKAHEAD;
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
    return ctx->tokens[ctx->token_offset].kind;
}

static struct str
get_token_line(struct context *ctx, struct token tok) {
    size_t idx = 0;
    size_t line = 1;
    while (line < tok.line) {
        switch (ctx->src[idx]) {
        case '\n':
            idx += 1;
            line += 1;
            break;
        default:
            idx += 1;
            break;
        }
    }
    size_t len = 0;
    while (idx + len < ctx->src_len && ctx->src[idx + len] != '\n') {
        len += 1;
    }
    return (struct str){ .len = len, .ptr = ctx->src + idx };;
}

static void
expected(struct context *ctx, enum TOKEN *token_kinds, size_t token_kinds_len) {
    struct token tok;
    take_token(ctx, &tok);
    struct str token_line = get_token_line(ctx, tok);
    fprintf(stderr,
        "error: unexpected token %s\n"
        " --> %s:%zu:%zu\n"
        "  |\n"
        "  | %.*s\n",
        TOKEN_NAMES[tok.kind], ctx->input_file_path, tok.line, tok.col,
        (int)token_line.len, token_line.ptr
    );
    fprintf(stderr, "  | ");
    for (size_t i = 0; i < tok.col - 1; i++) {
        if (token_line.ptr[i] == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }
    for (size_t i = 0; i < tok.len; i++) { fputc('^', stderr); }
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

size_t // out_len
remove_underscores(char *in, size_t in_len, char *out, size_t out_cap) {
    size_t j = 0;
    for (size_t i = 0; i < in_len && j < out_cap; i++) {
        if (in[i] != '_') {
            out[j++] = in[i];
        }
    }
    return j;
}

static void
compile_literal(struct context *ctx) {
    struct token tok;
    expect(ctx, TOKEN_LITERAL, &tok);
    char lit[MAX_LITERAL_LEN];
    size_t lit_len = remove_underscores(ctx->src + tok.idx, tok.len, lit, MAX_LITERAL_LEN);
    fprintf(ctx->output_file, "\tmov\tw0, #%.*s\n", (int)lit_len, lit);
}

static void
compile_fn_call(struct context *ctx) {
    // EBNF: fn_call = ident "(" ")" ;
    struct token tok;
    expect(ctx, TOKEN_IDENT, &tok);
    char *name = ctx->src + tok.idx;
    size_t name_len = tok.len;
    fprintf(ctx->output_file, "\tbl\t_%.*s\n", (int)name_len, name);
    expect(ctx, TOKEN_LEFT_PAREN, NULL);
    expect(ctx, TOKEN_RIGHT_PAREN, NULL);
}

static void
compile_expr(struct context *ctx) {
    // EBNF: expr = fn_call | literal ;
    switch (peek_token_kind(ctx)) {
    case TOKEN_IDENT: compile_fn_call(ctx); break;
    case TOKEN_LITERAL: compile_literal(ctx); break;
    default: expected(ctx, (enum TOKEN[]){TOKEN_IDENT, TOKEN_LITERAL}, 2);
    }
}

static void
compile_block(struct context *ctx) {
    // EBNF: block = "{" expr "}" ;
    expect(ctx, TOKEN_LEFT_BRACE, NULL);
    compile_expr(ctx);
    expect(ctx, TOKEN_RIGHT_BRACE, NULL);
}

static void
compile_fn_def(struct context *ctx) {
    // EBNF: fn_def = "fn" ident "(" ")" "->" ident block ;
    struct token tok;
    expect(ctx, TOKEN_KEYWORD_FN, NULL);
    expect(ctx, TOKEN_IDENT, &tok);
    char *name = ctx->src + tok.idx;
    size_t name_len = tok.len;
    fprintf(ctx->output_file,
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
    // TODO: intern the type, and pass it into compile_block to type check
    compile_block(ctx);
    fprintf(ctx->output_file,
        "\n"
        "\tldp\tx29, x30, [sp], #16\n"
        "\tret\n"
        "\t.cfi_endproc\n"
    );
}

static void
compile_program(struct context *ctx) {
    // EBNF: program = { fn_def } EOF ;
    fprintf(ctx->output_file,
        "\t.section\t__TEXT,__text,regular,pure_instructions\n"
    );
    for (;;) {
        switch (peek_token_kind(ctx)) {
        case TOKEN_KEYWORD_FN: compile_fn_def(ctx); break;
        case TOKEN_EOF: return;
        default: expected(ctx, (enum TOKEN[]){TOKEN_KEYWORD_FN, TOKEN_EOF}, 2);
        }
    }
}

char *
get_default_output_file_path(const char *input_file_path) {
    // Find the last dot in input_file_path
    char *dot = strrchr(input_file_path, '.');
    // Calculate the length of the base name
    size_t base_len = dot ? (size_t)(dot - input_file_path) : strlen(input_file_path);
    // Allocate memory for the new file path
    char *output_file_path = malloc(base_len + 3); // +3 for `.s\0`
    if (!output_file_path) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    // Copy the base name
    strncpy(output_file_path, input_file_path, base_len);
    output_file_path[base_len] = '\0';
    // Append the new extension
    strcat(output_file_path, ".s");
    return output_file_path;
}

int
main(int argc, char *argv[]) {
    // Parse command line options
    int opt;
    char *output_file_path = NULL;
    while ((opt = getopt(argc, argv, "o:")) != -1) {
        switch(opt) {
            case 'o':
                output_file_path = optarg;
                break;
            case '?':
                // getopt already prints an error message for unknown options
                fprintf(stderr, "Usage: %s [-o OUT_FILE] FILE\n", argv[0]);
                exit(EXIT_FAILURE);
            default:
                assert(!"unreachable");
        }
    }
    // After option parsing, optind is the index of the first non-option argument
    if (optind >= argc) {
        fprintf(stderr, "Expected FILE argument after options\n");
        fprintf(stderr, "Usage: %s [-o OUT_FILE] FILE\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *input_file_path = argv[optind];
    if (output_file_path == NULL) {
        output_file_path = get_default_output_file_path(input_file_path);
    }

    // Open the input file
    int fd = open(input_file_path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file '%s': %s\n", input_file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Get the input file size
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "Error getting file size for '%s': %s\n", input_file_path, strerror(errno));
        if (close(fd) == -1) { fprintf(stderr, "Error closing file '%s': %s\n", input_file_path, strerror(errno)); }
        exit(EXIT_FAILURE);
    }
    if (sb.st_size == 0) {
        fprintf(stderr, "Error: file '%s' is empty.\n", input_file_path);
        if (close(fd) == -1) { fprintf(stderr, "Error closing file '%s': %s\n", input_file_path, strerror(errno)); }
        exit(EXIT_FAILURE);
    }
    size_t file_size = sb.st_size;

    // Memory-map the input file
    void *mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        fprintf(stderr, "Error mapping file '%s': %s\n", input_file_path, strerror(errno));
        if (close(fd) == -1) { fprintf(stderr, "Error closing file '%s': %s\n", input_file_path, strerror(errno)); }
        exit(EXIT_FAILURE);
    }
    if (close(fd) == -1) {
        fprintf(stderr, "Error closing file '%s': %s\n", input_file_path, strerror(errno));
        // Continue even if close fails
    }

    // Open the output file
    FILE *output_file = fopen(output_file_path, "w");

    struct context ctx = {
        .output_file = output_file,
        .input_file_path = input_file_path,
        .src = (char *)mapped,
        .src_len = file_size,
    };
    compile_program(&ctx);

    // Close the output file
    if (fclose(output_file) == EOF) {
        fprintf(stderr, "Error closing file '%s': %s\n", output_file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Unmap the input file
    if (munmap(mapped, file_size) == -1) {
        fprintf(stderr, "Error unmapping file '%s': %s\n", input_file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return 0;
}
