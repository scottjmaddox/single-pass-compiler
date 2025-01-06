#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>         // For open()
#include <sys/mman.h>      // For mmap()
#include <sys/stat.h>      // For fstat()
#include <sys/syslimits.h> // For PATH_MAX
#include <unistd.h>        // For getopt(), close()
#include <errno.h>         // For errno
#include <string.h>        // For strerror()
#include <assert.h>        // For assert()
#include <limits.h>        // For ULLONG_MAX, LLONG_MAX

#define MAX_TOKEN_LOOKAHEAD 2
#define HEAP_COMMIT_SIZE (1024 * 1024) // 1 MiB

enum TOKEN {
    TOKEN_EOF,
    TOKEN_EXCLAMATION,
    TOKEN_EXCLAMATION_EQUAL,
    TOKEN_PERCENT,
    TOKEN_AMPERSAND,
    TOKEN_AMPERSAND_AMPERSAND,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_ASTERISK,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_RIGHT_ARROW,
    TOKEN_SLASH,
    TOKEN_LESS_THAN,
    TOKEN_LESS_EQUAL,
    TOKEN_LESS_LESS,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER_THAN,
    TOKEN_GREATER_EQUAL,
    TOKEN_GREATER_GREATER,
    TOKEN_CARET,
    TOKEN_LEFT_BRACE,
    TOKEN_BAR,
    TOKEN_BAR_BAR,
    TOKEN_RIGHT_BRACE,
    TOKEN_TILDE,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_FN,
    TOKEN_KEYWORD_LET,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_CONST,
    TOKEN_IDENT,
    TOKEN_INT_LITERAL,
    TOKEN_RESERVED,
    TOKEN_UNKNOWN,
};

static char *TOKEN_NAMES[] = {
    "EOF",
    "EXCLAMATION",
    "EXCLAMATION_EQUAL",
    "PERCENT",
    "AMPERSAND",
    "AMPERSAND_AMPERSAND",
    "LEFT_PAREN",
    "RIGHT_PAREN",
    "ASTERISK",
    "PLUS",
    "MINUS",
    "RIGHT_ARROW",
    "SLASH",
    "LESS_THAN",
    "LESS_EQUAL",
    "LESS_LESS",
    "EQUAL",
    "EQUAL_EQUAL",
    "GREATER_THAN",
    "GREATER_EQUAL",
    "GREATER_GREATER",
    "CARET",
    "LEFT_BRACE",
    "BAR",
    "BAR_BAR",
    "RIGHT_BRACE",
    "TILDE",
    "KEYWORD_IF",
    "KEYWORD_FN",
    "KEYWORD_LET",
    "KEYWORD_ELSE",
    "KEYWORD_CONST",
    "IDENT",
    "INT_LITERAL",
    "RESERVED",
    "UNKNOWN",
};

enum UNARY_OP {
    UNARY_OP_NEG,
    UNARY_OP_BITWISE_NOT,
    UNARY_OP_LOGICAL_NOT,
};

enum BINARY_OP {
    BINARY_OP_MUL,
    BINARY_OP_DIV,
    BINARY_OP_REM,
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_SHL,
    BINARY_OP_SHR,
    BINARY_OP_LT,
    BINARY_OP_LE,
    BINARY_OP_GT,
    BINARY_OP_GE,
    BINARY_OP_EQ,
    BINARY_OP_NE,
    BINARY_OP_BITWISE_AND,
    BINARY_OP_BITWISE_XOR,
    BINARY_OP_BITWISE_OR,
    BINARY_OP_LOGICAL_AND,
    BINARY_OP_LOGICAL_OR,
};

static int UNARY_OP_RIGHT_BINDING_POWERS[] = {
    [UNARY_OP_LOGICAL_NOT]  = 110,
    [UNARY_OP_BITWISE_NOT]  = 110,
    [UNARY_OP_NEG]          = 110,
};

static int BINARY_OP_LEFT_BINDING_POWERS[] = {
    [BINARY_OP_MUL]         = 100,
    [BINARY_OP_DIV]         = 100,
    [BINARY_OP_REM]         = 100,
    [BINARY_OP_ADD]         =  90,
    [BINARY_OP_SUB]         =  90,
    [BINARY_OP_SHL]         =  80,
    [BINARY_OP_SHR]         =  80,
    [BINARY_OP_LT]          =  70,
    [BINARY_OP_LE]          =  70,
    [BINARY_OP_GT]          =  70,
    [BINARY_OP_GE]          =  70,
    [BINARY_OP_EQ]          =  60,
    [BINARY_OP_NE]          =  60,
    [BINARY_OP_BITWISE_AND] =  50,
    [BINARY_OP_BITWISE_XOR] =  40,
    [BINARY_OP_BITWISE_OR]  =  30,
    [BINARY_OP_LOGICAL_AND] =  20,
    [BINARY_OP_LOGICAL_OR]  =  10,
};

static int BINARY_OP_RIGHT_BINDING_POWERS[] = {
    // +1 for left-to-right associativity, -1 for right-to-left associativity
    [BINARY_OP_MUL]         = 100 + 1,
    [BINARY_OP_DIV]         = 100 + 1,
    [BINARY_OP_REM]         = 100 + 1,
    [BINARY_OP_ADD]         =  90 + 1,
    [BINARY_OP_SUB]         =  90 + 1,
    [BINARY_OP_SHL]         =  80 + 1,
    [BINARY_OP_SHR]         =  80 + 1,
    [BINARY_OP_LT]          =  70 + 1,
    [BINARY_OP_LE]          =  70 + 1,
    [BINARY_OP_GT]          =  70 + 1,
    [BINARY_OP_GE]          =  70 + 1,
    [BINARY_OP_EQ]          =  60 + 1,
    [BINARY_OP_NE]          =  60 + 1,
    [BINARY_OP_BITWISE_AND] =  50 + 1,
    [BINARY_OP_BITWISE_XOR] =  40 + 1,
    [BINARY_OP_BITWISE_OR]  =  30 + 1,
    [BINARY_OP_LOGICAL_AND] =  20 + 1,
    [BINARY_OP_LOGICAL_OR]  =  10 + 1,
};

enum TYPE {
    TYPE_UNIT,
    TYPE_FN,
    TYPE_I32,
};

static bool TYPE_SIGNED[] = {
    [TYPE_I32] = true,
};

static char *TYPE_NAMES[] = {
    "unit",
    "fn",
    "i32",
};

struct str {
    size_t len;
    char *ptr;
};

static struct str IF_STR = {.len = sizeof "if" - 1, .ptr = "if"};
static struct str FN_STR = {.len = sizeof "fn" - 1, .ptr = "fn"};
static struct str LET_STR = {.len = sizeof "let" - 1, .ptr = "let"};
static struct str ELSE_STR = {.len = sizeof "else" - 1, .ptr = "else"};
static struct str CONST_STR = {.len = sizeof "const" - 1, .ptr = "const"};
static struct str I32_STR = {.len = sizeof "i32" - 1, .ptr = "i32"};
static struct str BUILTIN_STR = {.len = sizeof "__builtin" - 1, .ptr = "__builtin"};
static struct str BUILTIN_TRAP_STR = {.len = sizeof "__builtin_trap" - 1, .ptr = "__builtin_trap"};

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

struct span {
    struct location start;
    struct location end;
};

struct type {
    enum TYPE kind;
    struct span span;
    // arg_list stores type constructor arguments.
    // For TYPE_FN, that's the parameter types followed by the return type.
    struct type_node *arg_list;
};

struct type_node {
    struct type_node *next;
    struct type type;
};

struct symbol {
    struct token ident_tok;
    struct type type;
};

struct symbol_node {
    struct symbol_node *next;
    struct symbol sym;
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
    // Heap state
    uintptr_t heap;
    uintptr_t heap_end;
    size_t commited_heap_size;
    size_t max_heap_size;
    struct type_node *free_type_nodes;
    struct symbol_node *free_symbol_nodes;
    // Compiler state
    size_t local_label_count;
    struct symbol_node *symbol_list;
};

///////////
// Utils //
///////////

static void*
alloc(struct context *ctx, size_t size, size_t align) {
    // NOTE: assume align is a non-zero, positive power of 2
    uintptr_t ptr = (ctx->heap_end + align - 1) & ~(align - 1);
    ctx->heap_end = ptr + size;
    size_t new_heap_size = ctx->heap_end - ctx->heap;
    if (new_heap_size > ctx->commited_heap_size) {
        if (new_heap_size > ctx->max_heap_size) {
            fprintf(stderr, "error: heap size limit exceeded\n");
            exit(EXIT_FAILURE);
        }
        // NOTE: assume HEAP_COMMIT_SIZE is a non-zero, positive power of 2
        size_t new_commited_heap_size = (new_heap_size + HEAP_COMMIT_SIZE - 1) & ~(HEAP_COMMIT_SIZE - 1);
        size_t commit_size = new_commited_heap_size - ctx->commited_heap_size;
        void *result = mmap((void *)(ctx->heap + ctx->commited_heap_size), commit_size,
                            PROT_READ | PROT_WRITE, MAP_ANON | MAP_FIXED | MAP_PRIVATE, -1, 0);
        if (result == MAP_FAILED) {
            fprintf(stderr, "internal compiler error while expanding the heap: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        ctx->commited_heap_size = new_commited_heap_size;
    }
    return (void*)ptr;
}

static struct type_node*
alloc_type_node(struct context *ctx) {
    struct type_node *node;
    if (ctx->free_type_nodes == NULL) {
        node = alloc(ctx, sizeof(struct type_node), __alignof__(struct type_node));
    } else {
        node = ctx->free_type_nodes;
        ctx->free_type_nodes = node->next;
    }
    return node;
}

static struct symbol_node*
alloc_symbol_node(struct context *ctx) {
    struct symbol_node *node;
    if (ctx->free_symbol_nodes == NULL) {
        node = alloc(ctx, sizeof(struct symbol_node), __alignof__(struct symbol_node));
    } else {
        node = ctx->free_symbol_nodes;
        ctx->free_symbol_nodes = node->next;
    }
    return node;
}

static bool
str_equals(struct str a, struct str b) {
    if (a.len != b.len) { return false; }
    for (size_t i = 0; i < a.len; i++) {
        if (a.ptr[i] != b.ptr[i]) { return false; }
    }
    return true;
}

static bool
str_starts_with(struct str str, struct str prefix) {
    return str.len >= prefix.len && str_equals((struct str){ .len = prefix.len, .ptr = str.ptr }, prefix);
}

static struct str
token_str(struct context *ctx, struct token tok) {
    return (struct str){ .len = tok.len, .ptr = ctx->src + tok.loc.idx };
}

static struct symbol_node*
find_symbol_node(struct context *ctx, struct str ident) {
    for(struct symbol_node *node = ctx->symbol_list; node != NULL; node = node->next) {
        if (str_equals(token_str(ctx, node->sym.ident_tok), ident)) {
            return node;
        }
    }
    return NULL;
}

//////////////////////
// Lexical analysis //
//////////////////////

static struct token
lex(struct context *ctx) {
    char *src = ctx->src;
    size_t len = ctx->src_len;
    struct token tok = { .kind = TOKEN_UNKNOWN, .len = 1, .loc = ctx->src_loc };
    size_t i;
    while (tok.loc.idx < len) {
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
        case '!':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '=': tok.kind = TOKEN_EXCLAMATION_EQUAL; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_EXCLAMATION;
            return tok;
        case '%': tok.kind = TOKEN_PERCENT; return tok;
        case '&':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '&': tok.kind = TOKEN_AMPERSAND_AMPERSAND; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_AMPERSAND;
            return tok;
        case '(': tok.kind = TOKEN_LEFT_PAREN; return tok;
        case ')': tok.kind = TOKEN_RIGHT_PAREN; return tok;
        case '+': tok.kind = TOKEN_PLUS; return tok;
        case '*': tok.kind = TOKEN_ASTERISK; return tok;
        case '-':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '>': tok.kind = TOKEN_RIGHT_ARROW; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_MINUS;
            return tok;
        case '/':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '/':
                    // line comment
                    i = tok.loc.idx + 2;
                    while (i < len && src[i] != '\n') { i += 1; }
                    tok.loc.col += i - tok.loc.idx;
                    tok.loc.idx = i;
                    continue;
                }
            }
            tok.kind = TOKEN_SLASH;
            return tok;
        case '<':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '=': tok.kind = TOKEN_LESS_EQUAL; tok.len = 2; return tok;
                case '<': tok.kind = TOKEN_LESS_LESS; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_LESS_THAN;
            return tok;
        case '=':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '=': tok.kind = TOKEN_EQUAL_EQUAL; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_EQUAL;
            return tok;
        case '>':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '=': tok.kind = TOKEN_GREATER_EQUAL; tok.len = 2; return tok;
                case '>': tok.kind = TOKEN_GREATER_GREATER; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_GREATER_THAN;
            return tok;
        case '^': tok.kind = TOKEN_CARET; return tok;
        case '{': tok.kind = TOKEN_LEFT_BRACE; return tok;
        case '|':
            if (tok.loc.idx + 1 < len) {
                switch (src[tok.loc.idx + 1]) {
                case '|': tok.kind = TOKEN_BAR_BAR; tok.len = 2; return tok;
                }
            }
            tok.kind = TOKEN_BAR;
            return tok;
        case '}': tok.kind = TOKEN_RIGHT_BRACE; return tok;
        case '~': tok.kind = TOKEN_TILDE; return tok;
        case '_':
        case 'A' ... 'Z':
        case 'a' ... 'z':
            i = tok.loc.idx + 1;
            while (i < len) {
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
                if (str_equals(token_str(ctx, tok), IF_STR)) { tok.kind = TOKEN_KEYWORD_IF; return tok; }
                if (str_equals(token_str(ctx, tok), FN_STR)) { tok.kind = TOKEN_KEYWORD_FN; return tok; }
                break;
            case 3:
                if (str_equals(token_str(ctx, tok), LET_STR)) { tok.kind = TOKEN_KEYWORD_LET; return tok; }
                break;
            case 4:
                if (str_equals(token_str(ctx, tok), ELSE_STR)) { tok.kind = TOKEN_KEYWORD_ELSE; return tok; }
                break;
            case 5:
                if (str_equals(token_str(ctx, tok), CONST_STR)) { tok.kind = TOKEN_KEYWORD_CONST; return tok; }
                break;
            }
            tok.kind = TOKEN_IDENT;
            return tok;
        case '0' ... '9':
            i = tok.loc.idx;
            bool has_digit = false;
            bool is_reserved = false;
            if (src[i] == '0' && i + 1 < len
            && (src[i + 1] == 'b' || src[i + 1] == 'o' || src[i + 1] == 'x')) {
                i += 1;
                switch (src[i]) {
                case 'b':
                    // binary literal
                    i += 1;
                    while (i < len) {
                        switch (src[i]) {
                        case '_': i += 1; continue;
                        case '0' ... '1': i += 1; has_digit = true; continue;
                        case '2' ... '9': i += 1; is_reserved = true; break;
                        }
                        break;
                    }
                    break;
                case 'o':
                    // octal literal
                    i += 1;
                    while (i < len) {
                        switch (src[i]) {
                        case '_': i += 1; continue;
                        case '0' ... '7': i += 1; has_digit = true; continue;
                        case '8' ... '9': i += 1; is_reserved = true; break;
                        }
                        break;
                    }
                    break;
                case 'x':
                    // hexadecimal literal
                    i += 1;
                    while (i < len) {
                        switch (src[i]) {
                        case '_': i += 1; continue;
                        case '0' ... '9':
                        case 'A' ... 'F':
                        case 'a' ... 'f': i += 1; has_digit = true; continue;
                        }
                        break;
                    }
                    break;
                default: assert(!"unreachable");
                }
            } else {
                // decimal literal
                has_digit = true;
                i += 1;
                while (i < len) {
                    switch (src[i]) {
                    case '_':
                    case '0' ... '9': i += 1; continue;
                    }
                    break;
                }
            }
            if (i < len) {
                switch (src[i]) {
                case '.':
                case 'A' ... 'Z':
                case 'a' ... 'z': i += 1; is_reserved = true; break;
                }
            }
            is_reserved = is_reserved || !has_digit;
            tok.kind = is_reserved ? TOKEN_RESERVED : TOKEN_INT_LITERAL;
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

static struct span
join_spans(struct span left, struct span right) {
    return (struct span){ .start = left.start, .end = right.end };
}

static struct location
token_end(struct context *ctx, struct token tok) {
    // NOTE: assume tokens do not span multiple lines, for now
    return (struct location){
        .idx = tok.loc.idx + tok.len,
        .line = tok.loc.line,
        .col = tok.loc.col + tok.len,
    };
}

static struct span
token_span(struct context *ctx, struct token tok) {
    return (struct span){ .start = tok.loc, .end = token_end(ctx, tok) };
}

static void
push_token(struct context *ctx, struct token tok) {
    assert(ctx->token_count < MAX_TOKEN_LOOKAHEAD);
    ctx->tokens[(ctx->token_offset + ctx->token_count) % MAX_TOKEN_LOOKAHEAD] = tok;
    ctx->token_count += 1;
    ctx->src_loc = token_end(ctx, tok);
}

static void
fill_tokens(struct context *ctx) {
    while (ctx->token_count < MAX_TOKEN_LOOKAHEAD) {
        struct token tok = lex(ctx);
        push_token(ctx, tok);
    }
}

static void
take_token(struct context *ctx, struct token* token) {
    assert(token != NULL);
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

static struct location
peek_token_loc(struct context *ctx) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    return ctx->tokens[ctx->token_offset].loc;
}

static struct span
peek_token_span(struct context *ctx) {
    if (ctx->token_count == 0) { fill_tokens(ctx); }
    return token_span(ctx, ctx->tokens[ctx->token_offset]);
}

///////////////////////////////
// Diagnostics and Utilities //
///////////////////////////////

static struct str
get_span_line(struct context *ctx, struct span span) {
    size_t idx = 0;
    size_t line = 1;
    while (line < span.start.line) {
        switch (ctx->src[idx]) {
        case '\n': idx += 1; line += 1; break;
        default: idx += 1; break;
        }
    }
    size_t len = 0;
    while (idx + len < ctx->src_len && ctx->src[idx + len] != '\n') {
        len += 1;
    }
    return (struct str){ .len = len, .ptr = ctx->src + idx };;
}

static void
eprint_span(struct context *ctx, struct span span) {
    // TODO: handle multi-line spans
    assert(span.start.line == span.end.line);
    struct str token_line = get_span_line(ctx, span);
    fprintf(stderr,
        " --> %s:%zu:%zu\n"
        "  |\n"
        "  | %.*s\n",
        ctx->input_file_path, span.start.line, span.start.col,
        (int)token_line.len, token_line.ptr
    );
    fprintf(stderr, "  | ");
    for (size_t i = 0; i < span.start.col - 1; i++) {
        if (token_line.ptr[i] == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }
    size_t len = span.end.idx - span.start.idx;
    for (size_t i = 0; i < len; i++) { fputc('^', stderr); }
    fprintf(stderr, "\n");
}

static void
fail_expected_token_kind(struct context *ctx, enum TOKEN expected_token_kind, struct token tok) {
    fprintf(stderr, "error: unexpected token: %s\n", TOKEN_NAMES[tok.kind]);
    eprint_span(ctx, token_span(ctx, tok));
    fprintf(stderr, "Expected: %s\n", TOKEN_NAMES[expected_token_kind]);
    exit(EXIT_FAILURE);
}

static void
fail_expected(struct context *ctx, char *str) {
    struct token tok;
    take_token(ctx, &tok);
    fprintf(stderr, "error: unexpected token: %s\n", TOKEN_NAMES[tok.kind]);
    eprint_span(ctx, token_span(ctx, tok));
    fprintf(stderr, "Expected %s.\n", str);
    exit(EXIT_FAILURE);
}

static void
fail_reserved_ident(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: reserved identifier\n");
    eprint_span(ctx, token_span(ctx, tok));
    fprintf(stderr, "Identifiers starting with '__builtin' are reserved.\n");
    exit(EXIT_FAILURE);
}

static void
fail_unknown_builtin(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: unknown builtin function\n");
    eprint_span(ctx, token_span(ctx, tok));
    exit(EXIT_FAILURE);
}

static void
fail_undefined_fn(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: undefined function\n");
    eprint_span(ctx, token_span(ctx, tok));
    exit(EXIT_FAILURE);
}

static void
fail_fn_call_non_fn(struct context *ctx, struct token fn_call_tok, struct type ident_type) {
    fprintf(stderr, "error: function call to non-function:\n");
    eprint_span(ctx, token_span(ctx, fn_call_tok));
    fprintf(stderr, "%.*s has type %s and is defined here:\n",
        (int)fn_call_tok.len, ctx->src + fn_call_tok.loc.idx, TYPE_NAMES[ident_type.kind]);
    eprint_span(ctx, ident_type.span);
    exit(EXIT_FAILURE);
}

static void
fail_expected_type(struct context *ctx, enum TYPE expected, struct type actual) {
    fprintf(stderr, "error: expected type: %s\n", expected == TYPE_UNIT ? "unit" : "i32");
    eprint_span(ctx, actual.span);
    exit(EXIT_FAILURE);
}

static void
fail_type_mismatch(struct context *ctx, struct type expected, struct type actual) {
    fprintf(stderr, "error: type mismatch between %s:\n", TYPE_NAMES[expected.kind]);
    eprint_span(ctx, expected.span);
    fprintf(stderr, "and %s:\n", TYPE_NAMES[actual.kind]);
    eprint_span(ctx, actual.span);
    exit(EXIT_FAILURE);
}

static void
fail_int_literal_out_of_range(struct context *ctx, struct token tok) {
    fprintf(stderr, "error: integer literal out of range\n");
    eprint_span(ctx, token_span(ctx, tok));
    fprintf(stderr, "Range: %lld to %lld\n", LLONG_MIN, LLONG_MAX);
    exit(EXIT_FAILURE);
}

static void
take_token_expect_kind(struct context *ctx, struct token *tok_out, enum TOKEN token_kind) {
    struct token tok;
    take_token(ctx, &tok);
    if (tok.kind != token_kind) {
        fail_expected_token_kind(ctx, token_kind, tok);
    }
    if (tok_out != NULL) {
        *tok_out = tok;
    }
}

//////////////////////
// Compiler Backend //
//////////////////////

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
emit_clear_reg(struct context *ctx, char *reg) {
    fprintf(ctx->output_file, "\tmov\t%s, #0\n", reg);
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
emit_fn_epilogue(struct context *ctx, enum TYPE return_type) {
    switch (return_type) {
    case TYPE_UNIT:
        emit_clear_reg(ctx, "x0");
        break;
    case TYPE_FN:
        emit_pop(ctx, "x0");
        break;
    case TYPE_I32:
        emit_pop(ctx, "w0");
        break;
    }
    fprintf(ctx->output_file,
        "\n"
        "\tldp\tx29, x30, [sp], #16\n"
        "\tret\n"
        "\t.cfi_endproc\n"
    );
}

static void
emit_drop_type(struct context *ctx, enum TYPE ty) {
    if (ty != TYPE_UNIT) {
        emit_pop(ctx, "w0");
    }
}

static void
emit_fn_call(struct context *ctx, char *name, size_t name_len, enum TYPE return_type) {
    fprintf(ctx->output_file, "\tbl\t_%.*s\n", (int)name_len, name);
    if (return_type != TYPE_UNIT) { emit_push(ctx, "w0"); }
}

static void
emit_local_label(struct context *ctx, size_t label) {
    fprintf(ctx->output_file, "%zu:\n", label);
}

static void
emit_local_forward_branch(struct context *ctx, size_t label) {
    fprintf(ctx->output_file, "\tb\t%zuf\n", label);
}

static void
emit_local_forward_branch_if_zero(struct context *ctx, size_t label) {
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tcbz\tw0, %zuf\n", label);
}

static void
emit_local_forward_branch_if_nonzero(struct context *ctx, size_t label) {
    emit_pop(ctx, "w0");
    fprintf(ctx->output_file, "\tcbnz\tw0, %zuf\n", label);
}

static void
emit_unary_op(struct context *ctx, enum UNARY_OP op) {
    emit_pop(ctx, "w0");
    switch (op) {
    case UNARY_OP_NEG: fprintf(ctx->output_file, "\tneg\tw0, w0\n"); break;
    case UNARY_OP_BITWISE_NOT: fprintf(ctx->output_file, "\tmvn\tw0, w0\n"); break;
    case UNARY_OP_LOGICAL_NOT: fprintf(ctx->output_file, "\tcmp\tw0, #0\n\tcset\tw0, eq\n" ); break;
    }
    emit_push(ctx, "w0");
}

static void
emit_binary_op(struct context *ctx, enum BINARY_OP op, enum TYPE left_type, enum TYPE right_type) {
    assert(left_type == TYPE_I32);
    assert(right_type == TYPE_I32);
    emit_pop(ctx, "w1");
    emit_pop(ctx, "w0");
    switch (op) {
    case BINARY_OP_MUL: fprintf(ctx->output_file, "\tmul\tw0, w0, w1\n"); break;
    case BINARY_OP_DIV: fprintf(ctx->output_file, "\tsdiv\tw0, w0, w1\n"); break;
    case BINARY_OP_REM:
        fprintf(ctx->output_file,
            "\tsdiv\tw2, w0, w1\n"
            "\tmul\tw2, w2, w1\n"
            "\tsub\tw0, w0, w2\n"
        );
        break;
    case BINARY_OP_ADD: fprintf(ctx->output_file, "\tadd\tw0, w0, w1\n"); break;
    case BINARY_OP_SUB: fprintf(ctx->output_file, "\tsub\tw0, w0, w1\n"); break;
    case BINARY_OP_SHL: fprintf(ctx->output_file, "\tlsl\tw0, w0, w1\n"); break;
    case BINARY_OP_SHR:
        if (TYPE_SIGNED[left_type]) {
            fprintf(ctx->output_file, "\tasr\tw0, w0, w1\n");
        } else {
            fprintf(ctx->output_file, "\tlsr\tw0, w0, w1\n");
        }
        break;
    case BINARY_OP_LT: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, lt\n"); break;
    case BINARY_OP_LE: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, le\n"); break;
    case BINARY_OP_GT: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, gt\n"); break;
    case BINARY_OP_GE: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, ge\n"); break;
    case BINARY_OP_EQ: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, eq\n"); break;
    case BINARY_OP_NE: fprintf(ctx->output_file, "\tcmp\tw0, w1\n\tcset\tw0, ne\n"); break;
    case BINARY_OP_BITWISE_AND: fprintf(ctx->output_file, "\tand\tw0, w0, w1\n");; break;
    case BINARY_OP_BITWISE_XOR: fprintf(ctx->output_file, "\teor\tw0, w0, w1\n");; break;
    case BINARY_OP_BITWISE_OR: fprintf(ctx->output_file, "\torr\tw0, w0, w1\n");; break;
    case BINARY_OP_LOGICAL_AND:
    case BINARY_OP_LOGICAL_OR:
        assert(!"unreachable");
    }
    emit_push(ctx, "w0");
}

static void
emit_int_literal(struct context *ctx, long long int value) {
    // push the literal onto the stack
    fprintf(ctx->output_file, "\tldr\tw0, =%lld\n", value);
    emit_push(ctx, "w0");
}

static void
emit_builtin_trap(struct context *ctx) {
    fprintf(ctx->output_file, "\tbrk\t#0\n");
}

///////////////////////
// Compiler Frontend //
///////////////////////

static void compile_program(struct context *ctx);
static void compile_const_def(struct context *ctx);
static void compile_const_def(struct context *ctx);
static void compile_fn_def(struct context *ctx, struct token *name_tok);
static struct type compile_block(struct context *ctx);
static struct type compile_expr(struct context *ctx, int min_binding_power);
static struct type compile_if_expr(struct context *ctx);
static struct type compile_prefix_op_expr(struct context *ctx);
static struct type compile_fn_call(struct context *ctx);
static struct type compile_int_literal(struct context *ctx, bool negate);

static void
compile_program(struct context *ctx) {
    // EBNF: program = { const_def } ;
    emit_program_prologue(ctx);
    while (peek_token_kind(ctx) != TOKEN_EOF) {
        compile_const_def(ctx);
    }
    emit_program_epilogue(ctx);
}

static void
compile_const_def(struct context *ctx) {
    // EBNF: const_def = "const" ident "=" fn_def ;
    take_token_expect_kind(ctx, NULL, TOKEN_KEYWORD_CONST);
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    if (str_starts_with(token_str(ctx, name_tok), BUILTIN_STR)) {
        fail_reserved_ident(ctx, name_tok);
    }
    take_token_expect_kind(ctx, NULL, TOKEN_EQUAL);
    compile_fn_def(ctx, &name_tok);
}

static void
compile_fn_def(struct context *ctx, struct token *name_tok) {
    // EBNF:
    // fn_def = "fn" "(" ")" "->" type_expr block ;
    // type_expr = ident ;
    struct token fn_tok;
    struct token params_end_tok;
    struct type declared_return_type;
    take_token_expect_kind(ctx, &fn_tok, TOKEN_KEYWORD_FN);
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
    // TODO: create type and symbol list for parameters
    take_token_expect_kind(ctx, &params_end_tok, TOKEN_RIGHT_PAREN);
    if (peek_token_kind(ctx) == TOKEN_RIGHT_ARROW) {
        struct token right_arrow_tok;
        take_token_expect_kind(ctx, &right_arrow_tok, TOKEN_RIGHT_ARROW);
        struct token type_tok;
        take_token_expect_kind(ctx, &type_tok, TOKEN_IDENT);
        if (!str_equals(token_str(ctx, type_tok), I32_STR)) {
            // TODO: support other return types
            fail_expected(ctx, "i32"); // TODO: improve error message and loc
        }
        declared_return_type.kind = TYPE_I32;
        declared_return_type.span = token_span(ctx, type_tok);
    } else {
        declared_return_type.kind = TYPE_UNIT;
        declared_return_type.span.start = token_end(ctx, params_end_tok);
        declared_return_type.span.end = peek_token_loc(ctx);
    }

    struct span fn_type_span = (struct span){ .start = fn_tok.loc, .end = declared_return_type.span.end };
    struct type_node *fn_arg_list = alloc_type_node(ctx);
    *fn_arg_list = (struct type_node){ .next = NULL, .type = declared_return_type};
    struct type fn_type = { .kind = TYPE_FN, .span = fn_type_span, .arg_list = fn_arg_list };
    struct symbol fn_sym = { .ident_tok = *name_tok, .type = fn_type };
    // TODO: check for an existing definition or a conflicting forward declaration
    struct symbol_node *fn_sym_node = alloc_symbol_node(ctx);
    *fn_sym_node = (struct symbol_node){ .next = ctx->symbol_list, .sym = fn_sym };
    ctx->symbol_list = fn_sym_node;
    // TODO: push parameter symbol list
    ctx->local_label_count = 0;
    emit_fn_prologue(ctx, ctx->src + name_tok->loc.idx, name_tok->len);
    struct type block_return_type = compile_block(ctx);
    if (block_return_type.kind != declared_return_type.kind) {
        fail_type_mismatch(ctx, declared_return_type, block_return_type);
    }
    emit_fn_epilogue(ctx, block_return_type.kind);
    // TODO: pop and free parameter symbol list
}

static struct type
compile_block(struct context *ctx) {
    // EBNF: block = "{" { expr } "}" ;
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_BRACE);
    struct type previous_return_type = { 0 };
    struct type final_return_type  = { 0 };
    while (peek_token_kind(ctx) != TOKEN_RIGHT_BRACE) {
        emit_drop_type(ctx, previous_return_type.kind);
        struct type return_type = compile_expr(ctx, 0);
        previous_return_type = final_return_type;
        final_return_type = return_type;
    }
    take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_BRACE);
    return final_return_type;
}

static struct type
compile_expr(struct context *ctx, int min_binding_power) {
    // EBNF: expr = block | if_expr | "(" expr ")" | [ "-" ] int_literal | fn_call | op_expr ;
    struct type left_type, right_type;
    switch (peek_token_kind(ctx)) {
    case TOKEN_RIGHT_BRACE: return (struct type){ .kind = TYPE_UNIT, .span = peek_token_span(ctx) };
    case TOKEN_LEFT_BRACE: left_type = compile_block(ctx); break;
    case TOKEN_KEYWORD_IF: left_type = compile_if_expr(ctx); break;
    case TOKEN_LEFT_PAREN:
        take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
        left_type = compile_expr(ctx, 0);
        take_token_expect_kind(ctx, NULL, TOKEN_RIGHT_PAREN);
        break;
    case TOKEN_INT_LITERAL: left_type = compile_int_literal(ctx, false); break;
    case TOKEN_IDENT: left_type = compile_fn_call(ctx); break;
    case TOKEN_MINUS:
    case TOKEN_TILDE:
    case TOKEN_EXCLAMATION:
        left_type = compile_prefix_op_expr(ctx);
        break;
    default: fail_expected(ctx, "an expression");
    }
    // EBNF: infix_op = "*" | "/" | "%" | "+" | "-" | "<<" | ">>"
    //                | "<" | "<=" | ">" | ">=" | "==" | "!=" | "&&"  | "||" ;
    for (;;) {
        enum BINARY_OP op;
        switch (peek_token_kind(ctx)) {
        case TOKEN_ASTERISK:            op = BINARY_OP_MUL;         break;
        case TOKEN_SLASH:               op = BINARY_OP_DIV;         break;
        case TOKEN_PERCENT:             op = BINARY_OP_REM;         break;
        case TOKEN_PLUS:                op = BINARY_OP_ADD;         break;
        case TOKEN_MINUS:               op = BINARY_OP_SUB;         break;
        case TOKEN_LESS_LESS:           op = BINARY_OP_SHL;         break;
        case TOKEN_GREATER_GREATER:     op = BINARY_OP_SHR;         break;
        case TOKEN_LESS_THAN:           op = BINARY_OP_LT;          break;
        case TOKEN_LESS_EQUAL:          op = BINARY_OP_LE;          break;
        case TOKEN_GREATER_THAN:        op = BINARY_OP_GT;          break;
        case TOKEN_GREATER_EQUAL:       op = BINARY_OP_GE;          break;
        case TOKEN_EQUAL_EQUAL:         op = BINARY_OP_EQ;          break;
        case TOKEN_EXCLAMATION_EQUAL:   op = BINARY_OP_NE;          break;
        case TOKEN_AMPERSAND:           op = BINARY_OP_BITWISE_AND; break;
        case TOKEN_CARET:               op = BINARY_OP_BITWISE_XOR; break;
        case TOKEN_BAR:                 op = BINARY_OP_BITWISE_OR;  break;
        case TOKEN_AMPERSAND_AMPERSAND: op = BINARY_OP_LOGICAL_AND; break;
        case TOKEN_BAR_BAR:             op = BINARY_OP_LOGICAL_OR;  break;
        default: return left_type;
        }
        if (BINARY_OP_LEFT_BINDING_POWERS[op] < min_binding_power) {
            break;
        }
        struct token op_tok;
        take_token(ctx, &op_tok);
        struct type result_type;
        switch (op) {
        case BINARY_OP_LOGICAL_OR: {
            if (left_type.kind != TYPE_I32) {
                fail_expected_type(ctx, TYPE_I32, left_type);
            }
            size_t true_label = ctx->local_label_count++;
            size_t done_label = ctx->local_label_count++;
            emit_local_forward_branch_if_nonzero(ctx, true_label);
            right_type = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
            if (right_type.kind != TYPE_I32) {
                fail_expected_type(ctx, TYPE_I32, right_type);
            }
            emit_local_forward_branch_if_nonzero(ctx, true_label);
            emit_int_literal(ctx, 0);
            emit_local_forward_branch(ctx, done_label);
            emit_local_label(ctx, true_label);
            emit_int_literal(ctx, 1);
            emit_local_label(ctx, done_label);
            result_type = (struct type){
                .kind = TYPE_I32, // TODO: return TYPE_BOOL
                .span = join_spans(left_type.span, right_type.span)};
            break;
        }
        case BINARY_OP_LOGICAL_AND: {
            if (left_type.kind != TYPE_I32) {
                fail_expected_type(ctx, TYPE_I32, left_type);
            }
            size_t false_label = ctx->local_label_count++;
            size_t done_label = ctx->local_label_count++;
            emit_local_forward_branch_if_zero(ctx, false_label);
            right_type = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
            if (right_type.kind != TYPE_I32) {
                fail_expected_type(ctx, TYPE_I32, right_type);
            }
            emit_local_forward_branch_if_zero(ctx, false_label);
            emit_int_literal(ctx, 1);
            emit_local_forward_branch(ctx, done_label);
            emit_local_label(ctx, false_label);
            emit_int_literal(ctx, 0);
            emit_local_label(ctx, done_label);
            result_type = (struct type){
                .kind = TYPE_I32, // TODO: return TYPE_BOOL
                .span = join_spans(left_type.span, right_type.span)};
            break;
        }
        default:
            if (left_type.kind != TYPE_I32) {
                fail_expected_type(ctx, TYPE_I32, left_type);
            }
            right_type = compile_expr(ctx, BINARY_OP_RIGHT_BINDING_POWERS[op]);
            if (right_type.kind != TYPE_I32) {
                fail_expected_type(ctx, TYPE_I32, right_type);
            }
            emit_binary_op(ctx, op, left_type.kind, right_type.kind);
            result_type = (struct type){
                .kind = TYPE_I32, // TODO: infer type
                .span = join_spans(left_type.span, right_type.span)};
            break;
        }
        left_type = result_type;
    }
    return left_type;
}

static struct type
compile_if_expr(struct context *ctx) {
    // EBNF: if_expr = "if" expr block_expr { "else" "if" expr block_expr } [ "else" block_expr ] ;
    struct token if_tok;
    take_token_expect_kind(ctx, &if_tok, TOKEN_KEYWORD_IF);
    struct type condition_type = compile_expr(ctx, 0);
    if (condition_type.kind != TYPE_I32) {
        fail_expected_type(ctx, TYPE_I32, condition_type);
    }
    size_t false_label = ctx->local_label_count++;
    size_t done_label = ctx->local_label_count++;
    emit_local_forward_branch_if_zero(ctx, false_label);
    struct type then_type = compile_block(ctx);
    emit_local_forward_branch(ctx, done_label);
    emit_local_label(ctx, false_label);
    if (peek_token_kind(ctx) == TOKEN_KEYWORD_ELSE) {
        struct token else_tok;
        take_token_expect_kind(ctx, &else_tok, TOKEN_KEYWORD_ELSE);
        struct type else_type;
        if (peek_token_kind(ctx) == TOKEN_KEYWORD_IF) {
            else_type = compile_if_expr(ctx);
        } else {
            else_type = compile_block(ctx);
        }
        if (then_type.kind != else_type.kind) {
            fail_type_mismatch(ctx, then_type, else_type);
        }
    } else {
        if (then_type.kind != TYPE_UNIT) {
            fail_expected_type(ctx, TYPE_UNIT, then_type);
        }
    }
    emit_local_label(ctx, done_label);
    return then_type;
}

static struct type
compile_prefix_op_expr(struct context *ctx) {
    // EBNF: prefix_op = "!" | "-" ;
    struct type return_type;
    switch (peek_token_kind(ctx)) {
    case TOKEN_MINUS:
        take_token_expect_kind(ctx, NULL, TOKEN_MINUS);
        if (peek_token_kind(ctx) == TOKEN_INT_LITERAL) {
            return_type = compile_int_literal(ctx, true);
            if (return_type.kind != TYPE_I32) {
                fail_expected_type(ctx, TYPE_I32, return_type);
            }
        } else {
            return_type = compile_expr(ctx, UNARY_OP_RIGHT_BINDING_POWERS[UNARY_OP_NEG]);
            if (return_type.kind != TYPE_I32) {
                fail_expected_type(ctx, TYPE_I32, return_type);
            }
            emit_unary_op(ctx, UNARY_OP_NEG);
        }
        break;
    case TOKEN_TILDE:
        take_token_expect_kind(ctx, NULL, TOKEN_TILDE);
        return_type = compile_expr(ctx, UNARY_OP_RIGHT_BINDING_POWERS[UNARY_OP_BITWISE_NOT]);
        if (return_type.kind != TYPE_I32) {
            fail_expected_type(ctx, TYPE_I32, return_type);
        }
        emit_unary_op(ctx, UNARY_OP_BITWISE_NOT);
        break;
    case TOKEN_EXCLAMATION:
        take_token_expect_kind(ctx, NULL, TOKEN_EXCLAMATION);
        return_type = compile_expr(ctx, UNARY_OP_RIGHT_BINDING_POWERS[UNARY_OP_LOGICAL_NOT]);
        if (return_type.kind != TYPE_I32) {
            fail_expected_type(ctx, TYPE_I32, return_type);
        }
        emit_unary_op(ctx, UNARY_OP_LOGICAL_NOT);
        break;
    default: assert(!"unreachable");
    }
    return return_type;
}

static struct type
compile_fn_call(struct context *ctx) {
    // EBNF: fn_call = ident "(" ")" ;
    struct token name_tok;
    take_token_expect_kind(ctx, &name_tok, TOKEN_IDENT);
    take_token_expect_kind(ctx, NULL, TOKEN_LEFT_PAREN);
    // TODO: collect arguments
    struct token right_paren_tok;
    take_token_expect_kind(ctx, &right_paren_tok, TOKEN_RIGHT_PAREN);
    struct span fn_call_span = { .start = name_tok.loc, .end = token_end(ctx, right_paren_tok) };
    struct str name_str = token_str(ctx, name_tok);
    if (str_starts_with(name_str, BUILTIN_STR)) {
        if (str_equals(name_str, BUILTIN_TRAP_STR)) {
            emit_builtin_trap(ctx);
            // TODO: return TYPE_NEVER
            return (struct type){ .kind = TYPE_UNIT, .span = fn_call_span};
        }
        fail_unknown_builtin(ctx, name_tok);
    }
    struct symbol_node *fn_sym_node = find_symbol_node(ctx, name_str);
    if (fn_sym_node == NULL) { fail_undefined_fn(ctx, name_tok); }
    struct type fn_type = fn_sym_node->sym.type;
    if (fn_type.kind != TYPE_FN) { fail_fn_call_non_fn(ctx, name_tok, fn_type); } // TODO: test this
    // TODO: check argument types against parameter types
    struct type_node *return_type_node = fn_type.arg_list;
    for (; return_type_node->next != NULL; return_type_node = return_type_node->next) {}
    struct type return_type = return_type_node->type;
    emit_fn_call(ctx, ctx->src + name_tok.loc.idx, name_tok.len, return_type.kind);
    return_type.span = fn_call_span;
    return return_type;
}

static struct type
compile_int_literal(struct context *ctx, bool negate) {
    struct token tok;
    take_token_expect_kind(ctx, &tok, TOKEN_INT_LITERAL);
    unsigned long long int uvalue = 0;
    char *src = ctx->src + tok.loc.idx;
    size_t len = tok.len;
    size_t i = 0;
    if (src[i] == '0' && i + 1 < len
    && (src[i + 1] == 'b' || src[i + 1] == 'o' || src[i + 1] == 'x')) {
        switch (src[i + 1]) {
        case 'b':
            // binary literal
            for (i += 2; i < len; i++) {
                if (src[i] == '_') { continue; }
                if (__builtin_mul_overflow(uvalue, 2, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
                char digit = src[i] - '0';
                assert(digit < 2);
                if (__builtin_add_overflow(uvalue, digit, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
            }
            break;
        case 'o':
            // octal literal
            for (i += 2; i < len; i++) {
                if (src[i] == '_') { continue; }
                if (__builtin_mul_overflow(uvalue, 8, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
                char digit = src[i] - '0';
                assert(digit < 8);
                if (__builtin_add_overflow(uvalue, digit, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
            }
            break;
        case 'x':
            // hexadecimal literal
            for (i += 2; i < len; i++) {
                if (src[i] == '_') { continue; }
                if (__builtin_mul_overflow(uvalue, 16, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
                char digit;
                if (src[i] >= '0' && src[i] <= '9') {
                    digit = src[i] - '0';
                } else if (src[i] >= 'A' && src[i] <= 'F') {
                    digit = src[i] - 'A' + 10;
                } else if (src[i] >= 'a' && src[i] <= 'f') {
                    digit = src[i] - 'a' + 10;
                } else {
                    assert(!"unreachable");
                }
                assert(digit < 16);
                if (__builtin_add_overflow(uvalue, digit, &uvalue)) {
                    fail_int_literal_out_of_range(ctx, tok);
                }
            }
            break;
        default: assert(!"unreachable");
        }
    } else {
        // decimal literal
        for (; i < len; i++) {
            if (src[i] == '_') { continue; }
            if (__builtin_mul_overflow(uvalue, 10, &uvalue)) {
                fail_int_literal_out_of_range(ctx, tok);
            }
            char digit = src[i] - '0';
            assert(digit < 10);
            if (__builtin_add_overflow(uvalue, digit, &uvalue)) {
                fail_int_literal_out_of_range(ctx, tok);
            }
        }
    }
    if (negate && uvalue > LLONG_MAX) {
        fail_int_literal_out_of_range(ctx, tok);
    }
    long long int value;
    if (negate) {
        if (__builtin_sub_overflow(0, uvalue, &value)) {
            fail_int_literal_out_of_range(ctx, tok);
        }
    } else {
        if (__builtin_add_overflow(0, uvalue, &value)) {
            fail_int_literal_out_of_range(ctx, tok);
        }
    }
    emit_int_literal(ctx, value);
    return (struct type){ .kind = TYPE_I32, .span = token_span(ctx, tok) };
}

////////////////
// Entrypoint //
////////////////

void // output_file_path should have capacity PATH_MAX + 1
get_default_output_file_path(const char *input_file_path, char *output_file_path) {
    // Find the last dot in input_file_path
    char *dot = strrchr(input_file_path, '.');
    // Calculate the length of the base name
    size_t base_len = dot ? (size_t)(dot - input_file_path) : strlen(input_file_path);
    assert(base_len + 2 <= PATH_MAX);
    // Copy the base name
    strncpy(output_file_path, input_file_path, base_len);
    output_file_path[base_len] = '\0';
    // Append the new extension
    strcat(output_file_path, ".s");
}

static char tmp_output_file_path[] = "/tmp/spc-XXXXXX";
static int tmp_output_fd = -1;
static FILE *tmp_output_file = NULL;

void cleanup_tmp_output(void) {
    if (tmp_output_file != NULL) {
        fclose(tmp_output_file);
    } else if (tmp_output_fd != -1) {
        close(tmp_output_fd);
    }
    if (tmp_output_file_path[0] != '\0') {
        remove(tmp_output_file_path);
    }
}

int
main(int argc, char *argv[]) {
    // Parse command line options
    int opt;
    char output_file_path_buf[PATH_MAX + 1];
    char *output_file_path = NULL;
    size_t max_heap_size = 1024 * 1024 * 1024; // 1 GiB
    while ((opt = getopt(argc, argv, "o:m:")) != -1) {
        switch(opt) {
            case 'o':
                output_file_path = optarg;
                break;
            case 'm':
                max_heap_size = 0;
                for (size_t i = 0; optarg[i] != '\0'; i++) {
                    if (optarg[i] < '0' || optarg[i] > '9') {
                        fprintf(stderr, "error: invalid number: %s\n", optarg);
                        return EXIT_FAILURE;
                    }
                    if (__builtin_mul_overflow(max_heap_size, 10, &max_heap_size)) {
                        fprintf(stderr, "error: number out of range: %s\n", optarg);
                        return EXIT_FAILURE;
                    }
                    char digit = optarg[i] - '0';
                    if (__builtin_add_overflow(max_heap_size, digit, &max_heap_size)) {
                        fprintf(stderr, "error: number out of range: %s\n", optarg);
                        return EXIT_FAILURE;
                    }
                }
                break;
            case '?':
                // getopt already prints an error message for unknown options
                fprintf(stderr, "Usage: %s [-o OUT_FILE] [-m MAX_HEAP_SIZE] FILE\n", argv[0]);
                return EXIT_FAILURE;
            default:
                assert(!"unreachable");
        }
    }
    // After option parsing, optind is the index of the first non-option argument
    if (optind >= argc) {
        fprintf(stderr, "Expected FILE argument after options\n");
        fprintf(stderr, "Usage: %s [-o OUT_FILE] FILE\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *input_file_path = argv[optind];
    if (output_file_path == NULL) {
        get_default_output_file_path(input_file_path, output_file_path_buf);
        output_file_path = output_file_path_buf;
    }

    // Open the input file
    int input_fd = open(input_file_path, O_RDONLY);
    if (input_fd == -1) {
        fprintf(stderr, "internal compiler error while opening file '%s': %s\n", input_file_path, strerror(errno));
        return EXIT_FAILURE;
    }

    // Get the input file size
    struct stat sb;
    if (fstat(input_fd, &sb) == -1) {
        fprintf(stderr, "internal compiler error while getting file size for '%s': %s\n", input_file_path, strerror(errno));
        if (close(input_fd) == -1) { fprintf(stderr, "internal compiler warning while closing file '%s': %s\n", input_file_path, strerror(errno)); }
        return EXIT_FAILURE;
    }
    size_t input_file_size = sb.st_size;
    if (input_file_size == 0) {
        fprintf(stderr, "error: empty input file '%s'\n", input_file_path);
        return EXIT_FAILURE;
    }
    // Memory-map the input file
    void *mapped_input_file = mmap(NULL, input_file_size, PROT_READ, MAP_PRIVATE, input_fd, 0);
    if (mapped_input_file == MAP_FAILED) {
        fprintf(stderr, "internal compiler error while mapping file '%s': %s\n", input_file_path, strerror(errno));
        if (close(input_fd) == -1) { fprintf(stderr, "internal compiler warning while closing file '%s': %s\n", input_file_path, strerror(errno)); }
        return EXIT_FAILURE;
    }
    if (close(input_fd) == -1) {
        fprintf(stderr, "internal compiler warning while closing file '%s': %s\n", input_file_path, strerror(errno));
        // Continue anyway
    }

    // Prepare a temporary file for the output
    tmp_output_fd = mkstemp(tmp_output_file_path);
    if (tmp_output_fd == -1) {
        fprintf(stderr, "internal compiler error while creating temporary output file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    atexit(cleanup_tmp_output);
    tmp_output_file = fdopen(tmp_output_fd, "w");
    if (tmp_output_file == NULL) {
        fprintf(stderr, "internal compiler error while opening temporary output file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Get the system's page size
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf");
        exit(EXIT_FAILURE);
    }
    // Align max_heap_size to page size
    max_heap_size = (max_heap_size / page_size) * page_size;
    if (max_heap_size == 0) {
        fprintf(stderr, "error: max heap size must be larger than the page size: %ld\n", page_size);
        return EXIT_FAILURE;
    }
    // Align max_heap_size to the commit size
    max_heap_size = (max_heap_size / HEAP_COMMIT_SIZE) * HEAP_COMMIT_SIZE;
    if (max_heap_size == 0) {
        fprintf(stderr, "error: max heap size must be larger than the commit size: %d\n", HEAP_COMMIT_SIZE);
        return EXIT_FAILURE;
    }
    // Reserve a memory block for the heap
    void *heap = mmap(NULL, max_heap_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (heap == MAP_FAILED) {
        fprintf(stderr, "internal compiler error while reserving memory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Compile the program
    struct context ctx = {
        .output_file = tmp_output_file,
        .input_file_path = input_file_path,
        .src = (char *)mapped_input_file,
        .src_len = input_file_size,
        .src_loc = { .idx = 0, .line = 1, .col = 1 },
        .heap = (uintptr_t)heap,
        .heap_end = (uintptr_t)heap,
        .commited_heap_size = 0,
        .max_heap_size = max_heap_size,
        .symbol_list = NULL,
        .free_symbol_nodes = NULL,
    };
    compile_program(&ctx);

    if (fclose(tmp_output_file) != 0) {
        fprintf(stderr, "internal compiler error while closing temporary output file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    tmp_output_file = NULL;
    tmp_output_fd = -1;

    if (rename(tmp_output_file_path, output_file_path) != 0) {
        fprintf(stderr, "internal compiler error while moving temporary output file to '%s': %s\n",
                output_file_path, strerror(errno));
        return EXIT_FAILURE;
    }
    tmp_output_file_path[0] = '\0';

    return 0;
}
