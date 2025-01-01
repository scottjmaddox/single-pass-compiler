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
#define MAX_INT_LITERAL_LEN 64

enum TOKEN {
    TOKEN_EOF = 0,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_SEMICOLON,
    TOKEN_EQUAL,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_RIGHT_ARROW,
    TOKEN_KEYWORD_FN,
    TOKEN_KEYWORD_LET,
    TOKEN_IDENT,
    TOKEN_LITERAL,
    TOKEN_UNKNOWN,
};

static char *TOKEN_NAMES[] = {
    "EOF",
    "LEFT_PAREN",
    "RIGHT_PAREN",
    "SEMICOLON",
    "EQUAL",
    "LEFT_BRACE",
    "RIGHT_BRACE",
    "RIGHT_ARROW",
    "KEYWORD_FN",
    "KEYWORD_LET",
    "IDENT",
    "LITERAL",
    "UNKNOWN",
};

struct str {
    size_t len;
    char *ptr;
};

struct location {
    size_t idx;
    size_t line;
    size_t col;
};

struct token {
    enum TOKEN kind;
    struct location loc;
    size_t len;
};

struct context {
    FILE *output_file;
    char *input_file_path;
    char *src;
    size_t src_len;
    struct location src_loc;
    // Token ring buffer
    size_t token_offset;
    size_t token_count;
    struct token tokens[MAX_TOKEN_LOOKAHEAD];
};

//////////////////////
// Lexical analysis //
//////////////////////

static struct token
lex(char *src, size_t src_len, struct location from_loc) {
    struct token tok = { .kind = TOKEN_UNKNOWN, .len = 1, .loc = from_loc };
    size_t i;
    while (tok.loc.idx < src_len) {
        switch (src[tok.loc.idx]) {
        case ' ':
        case '\t':
            tok.loc.idx += 1;
            tok.loc.col += 1;
            continue;
        case '\n':
            tok.loc.idx += 1;
            tok.loc.line += 1;
            tok.loc.col = 1;
            continue;
        case '/':
            if (tok.loc.idx + 1 < src_len) {
                switch (src[tok.loc.idx + 1]) {
                case '/':
                    i = tok.loc.idx + 2;
                    while (i < src_len && src[i] != '\n') { i += 1; }
                    tok.loc.col += i - tok.loc.idx;
                    tok.loc.idx = i;
                    continue;
                }
            }
            tok.kind = TOKEN_UNKNOWN;
            return tok;
        case '(': tok.kind = TOKEN_LEFT_PAREN; return tok;
        case ')': tok.kind = TOKEN_RIGHT_PAREN; return tok;
        case ';': tok.kind = TOKEN_SEMICOLON; return tok;
        case '=': tok.kind = TOKEN_EQUAL; return tok;
        case '{': tok.kind = TOKEN_LEFT_BRACE; return tok;
        case '}': tok.kind = TOKEN_RIGHT_BRACE; return tok;
        case '-':
            if (tok.loc.idx + 1 < src_len) {
                switch (src[tok.loc.idx + 1]) {
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
            i = tok.loc.idx + 1;
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
            tok.len = i - tok.loc.idx;
            switch (tok.len) {
            case 2:
                if (strncmp(src + tok.loc.idx, "fn", 2) == 0) {
                    tok.kind = TOKEN_KEYWORD_FN;
                    return tok;
                }
                break;
            case 3:
                if (strncmp(src + tok.loc.idx, "let", 3) == 0) {
                    tok.kind = TOKEN_KEYWORD_LET;
                    return tok;
                }
                break;
            }
            tok.kind = TOKEN_IDENT;
            return tok;
        case '0' ... '9':
            i = tok.loc.idx + 1;
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
            tok.len = i - tok.loc.idx;
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
    ctx->src_loc = (struct location){
        .idx = tok.loc.idx + tok.len,
        .line = tok.loc.line,
        .col = tok.loc.col + tok.len,
    };
}

static void
fill_tokens(struct context *ctx) {
    while (ctx->token_count < MAX_TOKEN_LOOKAHEAD) {
        struct token tok = lex(ctx->src, ctx->src_len, ctx->src_loc);
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

///////////////////////////////
// Diagnostics and Utilities //
///////////////////////////////

static struct str
get_token_line(struct context *ctx, struct token tok) {
    size_t idx = 0;
    size_t line = 1;
    while (line < tok.loc.line) {
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
eprint_token_line(struct context *ctx, struct token tok) {
    struct str token_line = get_token_line(ctx, tok);
    fprintf(stderr,
        " --> %s:%zu:%zu\n"
        "  |\n"
        "  | %.*s\n",
        ctx->input_file_path, tok.loc.line, tok.loc.col,
        (int)token_line.len, token_line.ptr
    );
    fprintf(stderr, "  | ");
    for (size_t i = 0; i < tok.loc.col - 1; i++) {
        if (token_line.ptr[i] == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }
    for (size_t i = 0; i < tok.len; i++) { fputc('^', stderr); }
    fprintf(stderr, "\n");
}

static void
eprint_expected(struct context *ctx, enum TOKEN *token_kinds, size_t token_kinds_len) {
    struct token tok;
    take_token(ctx, &tok);
    fprintf(stderr, "error: unexpected token %s\n", TOKEN_NAMES[tok.kind]);
    eprint_token_line(ctx, tok);
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
eprint_int_literal_too_long(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: integer literal too long\n");
    eprint_token_line(ctx, tok);
    fprintf(stderr, "Maximum allowed length (excluding underscores): %d\n", MAX_INT_LITERAL_LEN);
    exit(EXIT_FAILURE);
}

static void
expect(struct context *ctx, enum TOKEN token_kind, struct token *tok_out) {
    struct token tok;
    take_token(ctx, &tok);
    if (tok.kind != token_kind) {
        eprint_expected(ctx, &token_kind, 1);
    }
    if (tok_out != NULL) {
        *tok_out = tok;
    }
}

int // 0 on success, -1 on buffer overflow
remove_underscores(char *in, size_t in_len, char *out, size_t out_cap, size_t *out_len) {
    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        if (in[i] != '_') {
            if (j < out_cap) {
                out[j++] = in[i];
            } else {
                return -1;
            }
        }
    }
    *out_len = j;
    return 0;
}

/////////////////////////////
// Parsing and Compilation //
/////////////////////////////

static void
emit_program_prologue(struct context *ctx) {
    fprintf(ctx->output_file,
        "\t.section\t__TEXT,__text,regular,pure_instructions\n"
    );
}

static void
emit_fn_prologue(struct context *ctx, char *name, size_t name_len) {
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
}

static void
emit_fn_epilogue(struct context *ctx) {
    fprintf(ctx->output_file,
        "\n"
        "\tldp\tx29, x30, [sp], #16\n"
        "\tret\n"
        "\t.cfi_endproc\n"
    );
}

static void
emit_fn_call(struct context *ctx, char *name, size_t name_len) {
    fprintf(ctx->output_file, "\tbl\t_%.*s\n", (int)name_len, name);
}

static void
emit_int_literal(struct context *ctx, char *lit, size_t lit_len) {
    fprintf(ctx->output_file, "\tmov\tw0, #%.*s\n", (int)lit_len, lit);
}

static void compile_program(struct context *ctx);
static void compile_stmnt(struct context *ctx);
static void compile_let_stmnt(struct context *ctx);
static void compile_fn_def(struct context *ctx);
static void compile_expr(struct context *ctx);
static void compile_fn_call(struct context *ctx);
static void compile_literal(struct context *ctx);

static void
compile_program(struct context *ctx) {
    // EBNF: program = { stmnt } ;
    emit_program_prologue(ctx);
    while (peek_token_kind(ctx) != TOKEN_EOF) {
        compile_stmnt(ctx);
    }
}

static void
compile_stmnt(struct context *ctx) {
    // EBNF: stmnt = let_stmnt ;
    switch (peek_token_kind(ctx)) {
    case TOKEN_KEYWORD_LET: compile_let_stmnt(ctx); break;
    default: eprint_expected(ctx, (enum TOKEN[]){TOKEN_KEYWORD_LET}, 1);
    }
}

static void
compile_let_stmnt(struct context *ctx) {
    // EBNF: let_stmnt = "let" ident "=" fn_def ";" ;
    expect(ctx, TOKEN_KEYWORD_LET, NULL);
    struct token tok;
    expect(ctx, TOKEN_IDENT, &tok);
    expect(ctx, TOKEN_EQUAL, NULL);
    emit_fn_prologue(ctx, ctx->src + tok.loc.idx, tok.len);
    compile_expr(ctx);
    emit_fn_epilogue(ctx);
    expect(ctx, TOKEN_SEMICOLON, NULL);
}

static void
compile_fn_def(struct context *ctx) {
    // EBNF: fn_def = "fn" "(" ")" "->" type_expr "{" expr "}" ;
    expect(ctx, TOKEN_KEYWORD_FN, NULL);
    expect(ctx, TOKEN_LEFT_PAREN, NULL);
    expect(ctx, TOKEN_RIGHT_PAREN, NULL);
    expect(ctx, TOKEN_RIGHT_ARROW, NULL);
    // TODO: support arbitrary type expressions
    struct token tok;
    expect(ctx, TOKEN_IDENT, &tok);
    // TODO: intern the type, and type check
    expect(ctx, TOKEN_LEFT_BRACE, NULL);
    compile_expr(ctx);
    expect(ctx, TOKEN_RIGHT_BRACE, NULL);
}

static void
compile_expr(struct context *ctx) {
    // EBNF: expr = literal | fn_call ;
    switch (peek_token_kind(ctx)) {
    case TOKEN_LITERAL: compile_literal(ctx); break;
    case TOKEN_IDENT: compile_fn_call(ctx); break;
    case TOKEN_KEYWORD_FN: compile_fn_def(ctx); break;
    default: eprint_expected(ctx, (enum TOKEN[]){TOKEN_LITERAL, TOKEN_IDENT, TOKEN_KEYWORD_FN}, 3);
    }
}

static void
compile_fn_call(struct context *ctx) {
    // EBNF: fn_call = ident "(" ")" ;
    struct token tok;
    expect(ctx, TOKEN_IDENT, &tok);
    expect(ctx, TOKEN_LEFT_PAREN, NULL);
    expect(ctx, TOKEN_RIGHT_PAREN, NULL);
    emit_fn_call(ctx, ctx->src + tok.loc.idx, tok.len);
}

static void
compile_literal(struct context *ctx) {
    struct token tok;
    expect(ctx, TOKEN_LITERAL, &tok);
    char lit[MAX_INT_LITERAL_LEN];
    size_t lit_len;
    if (remove_underscores(ctx->src + tok.loc.idx, tok.len, lit, MAX_INT_LITERAL_LEN, &lit_len) == -1) {
        eprint_int_literal_too_long(ctx, tok);
    }
    emit_int_literal(ctx, lit, lit_len);
}

/////////////////
// Entry point //
/////////////////

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
        .src_loc = { .idx = 0, .line = 1, .col = 1 },
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
