#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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
    TOKEN_PERCENT,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_ASTERISK,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_SLASH,
    TOKEN_SEMICOLON,
    TOKEN_EQUAL,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_RIGHT_ARROW,
    TOKEN_KEYWORD_FN,
    TOKEN_KEYWORD_LET,
    TOKEN_IDENT,
    TOKEN_INT_LITERAL,
    TOKEN_UNKNOWN,
};

static char *TOKEN_NAMES[] = {
    "EOF",
    "PERCENT",
    "LEFT_PAREN",
    "RIGHT_PAREN",
    "ASTERISK",
    "PLUS",
    "MINUS",
    "SLASH",
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

static size_t
scan_int_literal_rest(char *src, size_t src_len, size_t idx) {
    while (idx < src_len) {
        switch (src[idx]) {
        case '_':
        case '0' ... '9':
            idx += 1;
            continue;
        }
        break;
    }
    return idx;
}

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
        case '%': tok.kind = TOKEN_PERCENT; return tok;
        case '(': tok.kind = TOKEN_LEFT_PAREN; return tok;
        case ')': tok.kind = TOKEN_RIGHT_PAREN; return tok;
        case '+': tok.kind = TOKEN_PLUS; return tok;
        case '*': tok.kind = TOKEN_ASTERISK; return tok;
        case '-':
            if (tok.loc.idx + 1 < src_len) {
                switch (src[tok.loc.idx + 1]) {
                case '>':
                    tok.kind = TOKEN_RIGHT_ARROW;
                    tok.len = 2;
                    return tok;
                case '0' ... '9':
                    i = scan_int_literal_rest(src, src_len, tok.loc.idx + 2);
                    tok.kind = TOKEN_INT_LITERAL;
                    tok.len = i - tok.loc.idx;
                    return tok;
                }
            }
            tok.kind = TOKEN_MINUS;
            return tok;
        case '/':
            if (tok.loc.idx + 1 < src_len) {
                switch (src[tok.loc.idx + 1]) {
                case '/':
                    // line comment
                    i = tok.loc.idx + 2;
                    while (i < src_len && src[i] != '\n') { i += 1; }
                    tok.loc.col += i - tok.loc.idx;
                    tok.loc.idx = i;
                    continue;
                }
            }
            tok.kind = TOKEN_SLASH;
            return tok;
        case ';': tok.kind = TOKEN_SEMICOLON; return tok;
        case '=': tok.kind = TOKEN_EQUAL; return tok;
        case '{': tok.kind = TOKEN_LEFT_BRACE; return tok;
        case '}': tok.kind = TOKEN_RIGHT_BRACE; return tok;
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
            i = scan_int_literal_rest(src, src_len, tok.loc.idx + 1);
            tok.kind = TOKEN_INT_LITERAL;
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
//     return ctx->tokens[(ctx->token_offset + i) % MAX_TOKEN_LOOKAHEAD].kind;
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
take_token_expect_kind(struct context *ctx, struct token *tok_out, enum TOKEN token_kind) {
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
emit_program_epilogue(struct context *ctx) {
    fprintf(ctx->output_file, "\n.subsections_via_symbols\n");
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
emit_push(struct context *ctx, char *reg) {
    // NOTE: using 16-byte slot, for now, to comply with stack pointer alignment restrictions
    fprintf(ctx->output_file, "\tstr\t%s, [sp, #-16]!\n", reg);
}

static void
emit_pop(struct context *ctx, char *reg) {
    // NOTE: using 16-byte slot, for now, to comply with stack pointer alignment restrictions
    fprintf(ctx->output_file, "\tldr\t%s, [sp], #16\n", reg);
}

static void
emit_fn_epilogue(struct context *ctx, bool has_return_value) {
    if (has_return_value) { emit_pop(ctx, "w0"); }
    fprintf(ctx->output_file,
        "\n"
        "\tldp\tx29, x30, [sp], #16\n"
        "\tret\n"
        "\t.cfi_endproc\n"
    );
}

static void
emit_fn_call(struct context *ctx, char *name, size_t name_len, bool has_return_value) {
    fprintf(ctx->output_file, "\tbl\t_%.*s\n", (int)name_len, name);
    if (has_return_value) { emit_push(ctx, "w0"); }
}

static void
emit_negate(struct context *ctx) {
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tneg\tw0, w0\n");
    emit_push(ctx, "w0");
}

static void
emit_add(struct context *ctx) {
    emit_pop(ctx, "w1");
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tadd\tw0, w0, w1\n");
    emit_push(ctx, "w0");
}

static void
emit_sub(struct context *ctx) {
    emit_pop(ctx, "w1");
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tsub\tw0, w0, w1\n");
    emit_push(ctx, "w0");
}

static void
emit_mul(struct context *ctx) {
    emit_pop(ctx, "w1");
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tmul\tw0, w0, w1\n");
    emit_push(ctx, "w0");
}

static void
emit_div(struct context *ctx) {
    emit_pop(ctx, "w1");
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tsdiv\tw0, w0, w1\n");
    emit_push(ctx, "w0");
}

static void
emit_rem(struct context *ctx) {
    emit_pop(ctx, "w1");
    emit_pop(ctx, "w0");
	fprintf(ctx->output_file, "\tsdiv\tw2, w0, w1\n");
	fprintf(ctx->output_file, "\tmul\tw2, w2, w1\n");
	fprintf(ctx->output_file, "\tsub\tw0, w0, w2\n");
    emit_push(ctx, "w0");
}

static void
emit_int_literal(struct context *ctx, char *lit, size_t lit_len) {
    // push the literal onto the stack
    fprintf(ctx->output_file, "\tldr\tw0, =%.*s\n", (int)lit_len, lit);
    emit_push(ctx, "w0");
}

static void compile_program(struct context *ctx);
static void compile_stmnt(struct context *ctx);
static void compile_let_stmnt(struct context *ctx);
static void compile_fn_def(struct context *ctx, struct token *name_tok);
static void compile_expr(struct context *ctx, int min_binding_power);
static void compile_neg_expr(struct context *ctx);
static void compile_fn_call(struct context *ctx);
static void compile_int_literal(struct context *ctx, bool negate);

static void
compile_program(struct context *ctx) {
    // EBNF: program = { stmnt } ;
    emit_program_prologue(ctx);
    while (peek_token_kind(ctx) != TOKEN_EOF) {
        compile_stmnt(ctx);
    }
    emit_program_epilogue(ctx);
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
    take_token_expect_kind(ctx, NULL, TOKEN_KEYWORD_LET);
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    take_token_expect_kind(ctx, NULL, TOKEN_EQUAL);
    compile_fn_def(ctx, &name_tok);
    take_token_expect_kind(ctx, NULL, TOKEN_SEMICOLON);
}

static void
compile_fn_def(struct context *ctx, struct token *name_tok) {
    // EBNF: fn_def = "fn" "(" ")" "->" type_expr "{" expr "}" ;
    take_token_expect_kind(ctx, NULL, TOKEN_KEYWORD_FN);
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
    take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_PAREN);
    take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_ARROW);
    // TODO: support arbitrary type expressions
    struct token type_tok;
    take_token_expect_kind(ctx, &type_tok, TOKEN_IDENT);
    // TODO: intern the type, and type check
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_BRACE);
    emit_fn_prologue(ctx, ctx->src + name_tok->loc.idx, name_tok->len);
    compile_expr(ctx, 0);
    // TODO: support optional return value
    bool has_return_value = true;
    emit_fn_epilogue(ctx, has_return_value);
    take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_BRACE);
}

static void
compile_expr(struct context *ctx, int min_binding_power) {
    // EBNF:
    // expr = "(" expr ")" | prefix_op_expr | int_literal | fn_call | infix_op_expr ;
    // infix_op_expr = expr "+" expr | expr "-" expr | expr "*" expr | expr "/" expr | expr "%" expr ;
    // prefix_op_expr = "-" expr ;
    switch (peek_token_kind(ctx)) {
    case TOKEN_LEFT_PAREN: {
        take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
        compile_expr(ctx, 0);
        take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_PAREN);
        break;
    }
    case TOKEN_MINUS: compile_neg_expr(ctx); break;
    case TOKEN_INT_LITERAL: compile_int_literal(ctx, false); break;
    case TOKEN_IDENT: compile_fn_call(ctx); break;
    default: eprint_expected(ctx, (enum TOKEN[]){TOKEN_INT_LITERAL, TOKEN_IDENT, TOKEN_KEYWORD_FN}, 3);
    }
    for (;;) {
        int left_binding_power, right_binding_power;
        switch (peek_token_kind(ctx)) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            left_binding_power = 1;
            right_binding_power = 2;
            break;
        case TOKEN_ASTERISK:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            left_binding_power = 3;
            right_binding_power = 4;
            break;
        default: return;
        }
        if (left_binding_power < min_binding_power) {
            break;
        }
        struct token tok;
        take_token(ctx, &tok);
        compile_expr(ctx, right_binding_power);
        switch (tok.kind) {
        case TOKEN_PLUS: emit_add(ctx); break;
        case TOKEN_MINUS: emit_sub(ctx); break;
        case TOKEN_ASTERISK: emit_mul(ctx); break;
        case TOKEN_SLASH: emit_div(ctx); break;
        case TOKEN_PERCENT: emit_rem(ctx); break;
        default: assert(!"unreachable");
        }
    }
}

static void
compile_neg_expr(struct context *ctx) {
    take_token_expect_kind(ctx, NULL, TOKEN_MINUS);
    bool negate = true;
    while (peek_token_kind(ctx) == TOKEN_MINUS) {
        take_token_expect_kind(ctx, NULL, TOKEN_MINUS);
        negate = !negate;
    }
    int right_binding_power = 5;
    compile_expr(ctx, right_binding_power);
    if (negate) {
        emit_negate(ctx);
    }
}

static void
compile_fn_call(struct context *ctx) {
    // EBNF: fn_call = ident "(" ")" ;
    struct token tok;
    take_token_expect_kind(ctx, &tok, TOKEN_IDENT);
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
    take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_PAREN);
    // TODO: support optional return value
    bool has_return_value = true;
    emit_fn_call(ctx, ctx->src + tok.loc.idx, tok.len, has_return_value);
}

static void
compile_int_literal(struct context *ctx, bool negate) {
    struct token tok;
    take_token_expect_kind(ctx, &tok, TOKEN_INT_LITERAL);
    char lit[MAX_INT_LITERAL_LEN];
    size_t lit_len;
    char *out = lit;
    size_t out_cap = MAX_INT_LITERAL_LEN;
    if (negate) {
        lit[0] = '-';
        out += 1;
        out_cap -= 1;
    }
    if (remove_underscores(ctx->src + tok.loc.idx, tok.len, out, out_cap, &lit_len) == -1) {
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
