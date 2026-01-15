#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/lexer.h"
#include "../include/error.h"

// Keyword table
typedef struct {
    const char *keyword;
    TokenType type;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    {"var", TOKEN_VAR},
    {"const", TOKEN_CONST},
    {"func", TOKEN_FUNC},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"while", TOKEN_WHILE},
    {"for", TOKEN_FOR},
    {"return", TOKEN_RETURN},
    {"struct", TOKEN_STRUCT},
    {"enum", TOKEN_ENUM},
    {"type", TOKEN_TYPEDEF},
    {"unsafe", TOKEN_UNSAFE},
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"public", TOKEN_PUBLIC},
    {"module", TOKEN_MODULE},
    {"import", TOKEN_IMPORT},
    {"extern", TOKEN_EXTERN},
    {"as", TOKEN_AS},
    {"match", TOKEN_MATCH},
    {"fail", TOKEN_FAIL},
    {"null", TOKEN_NULL},
    {"in", TOKEN_IN},
    {"result", TOKEN_RESULT},
    {"packed", TOKEN_PACKED},
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    // Primitive types
    {"i8", TOKEN_I8},
    {"i16", TOKEN_I16},
    {"i32", TOKEN_I32},
    {"i64", TOKEN_I64},
    {"u8", TOKEN_U8},
    {"u16", TOKEN_U16},
    {"u32", TOKEN_U32},
    {"u64", TOKEN_U64},
    {"f32", TOKEN_F32},
    {"f64", TOKEN_F64},
    {"bool", TOKEN_BOOL},
    {"void", TOKEN_VOID},
    // C ABI types

    {NULL, TOKEN_EOF} // Sentinel
};

// Helper functions
static char lexer_peek_next(Lexer *lexer);
static void lexer_advance(Lexer *lexer);
static void lexer_skip_whitespace(Lexer *lexer);
static bool lexer_skip_comment(Lexer *lexer);
static Token *lexer_lex_identifier(Lexer *lexer);
static Token *lexer_lex_number(Lexer *lexer);
static Token *lexer_lex_string(Lexer *lexer);
static bool lexer_match(Lexer *lexer, char expected);
static char lexer_peek(Lexer *lexer);
static bool lexer_is_at_end(Lexer *lexer);
static Token *lexer_make_token(Lexer *lexer, TokenType type, const char *lexeme, size_t start_line, size_t start_column);
static Token *lexer_error_token(Lexer *lexer, const char *message);

Lexer *lexer_init(const char *source, const char *filename) {
    Lexer *lexer = malloc(sizeof(Lexer));
    if (!lexer) {
        fprintf(stderr, "Error: Failed to allocate memory for lexer\n");
        exit(1);
    }
    
    lexer->source = source;
    lexer->filename = filename ? filename : "<input>";
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->current = source[0];
    
    return lexer;
}

void lexer_free(Lexer *lexer) {
    free(lexer);
}

static char lexer_peek_next(Lexer *lexer) {
    return lexer->source[lexer->pos + 1];
}

static char lexer_peek(Lexer *lexer) {
    return lexer->current;
}

static bool lexer_is_at_end(Lexer *lexer) {
    return lexer->current == '\0';
}

static bool lexer_match(Lexer *lexer, char expected) {
    if (lexer_is_at_end(lexer)) return false;
    if (lexer->current != expected) return false;
    lexer_advance(lexer);
    return true;
}

static void lexer_advance(Lexer *lexer) {
    if (lexer->current == '\0') {
        return;
    }
    
    if (lexer->current == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    lexer->pos++;
    lexer->current = lexer->source[lexer->pos];
}

static void lexer_skip_whitespace(Lexer *lexer) {
    while (isspace(lexer->current)) {
        lexer_advance(lexer);
    }
}

static bool lexer_skip_comment(Lexer *lexer) {
    // Single-line comment
    if (lexer->current == '/' && lexer_peek_next(lexer) == '/') {
        while (lexer->current != '\n' && lexer->current != '\0') {
            lexer_advance(lexer);
        }
        return true;
    }
    
    // Multi-line comment
    if (lexer->current == '/' && lexer_peek_next(lexer) == '*') {
        lexer_advance(lexer); // consume '/'
        lexer_advance(lexer); // consume '*'
        
        while (lexer->current != '\0') {
            if (lexer->current == '*' && lexer_peek_next(lexer) == '/') {
                lexer_advance(lexer); // consume '*'
                lexer_advance(lexer); // consume '/'
                return true;
            }
            lexer_advance(lexer);
        }
        
        // Unterminated comment
        error_report(lexer->filename, lexer->line, lexer->column, "unterminated comment");
        return true;
    }
    
    return false;
}

static Token *lexer_make_token(Lexer *lexer, TokenType type, const char *lexeme, size_t start_line, size_t start_column) {
    (void)lexer; // Unused parameter
    return token_create(type, lexeme, start_line, start_column);
}

static Token *lexer_error_token(Lexer *lexer, const char *message) {
    error_report(lexer->filename, lexer->line, lexer->column, message);
    return token_create(TOKEN_ERROR, message, lexer->line, lexer->column);
}

static Token *lexer_lex_identifier(Lexer *lexer) {
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;
    size_t start = lexer->pos;
    
    // Identifier: [a-zA-Z_][a-zA-Z0-9_]*
    while (isalnum(lexer->current) || lexer->current == '_') {
        lexer_advance(lexer);
    }
    
    size_t length = lexer->pos - start;
    char *lexeme = strndup(&lexer->source[start], length);
    
    // Check if it's a keyword
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strcmp(lexeme, keywords[i].keyword) == 0) {
            Token *token = lexer_make_token(lexer, keywords[i].type, lexeme, start_line, start_column);
            
            // Set boolean values
            if (keywords[i].type == TOKEN_TRUE) {
                token->value.bool_value = true;
            } else if (keywords[i].type == TOKEN_FALSE) {
                token->value.bool_value = false;
            }
            
            free(lexeme);
            return token;
        }
    }
    
    // It's an identifier
    Token *token = lexer_make_token(lexer, TOKEN_IDENTIFIER, lexeme, start_line, start_column);
    free(lexeme);
    return token;
}

static Token *lexer_lex_number(Lexer *lexer) {
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;
    size_t start = lexer->pos;
    bool is_float = false;
    
    // Integer part
    while (isdigit(lexer->current)) {
        lexer_advance(lexer);
    }
    
    // Check for decimal point
    if (lexer->current == '.' && isdigit(lexer_peek_next(lexer))) {
        is_float = true;
        lexer_advance(lexer); // consume '.'
        
        while (isdigit(lexer->current)) {
            lexer_advance(lexer);
        }
    }
    
    // Check for scientific notation (e.g., 1.5e10)
    if (lexer->current == 'e' || lexer->current == 'E') {
        is_float = true;
        lexer_advance(lexer);
        
        if (lexer->current == '+' || lexer->current == '-') {
            lexer_advance(lexer);
        }
        
        while (isdigit(lexer->current)) {
            lexer_advance(lexer);
        }
    }
    
    size_t length = lexer->pos - start;
    char *lexeme = strndup(&lexer->source[start], length);
    
    Token *token;
    if (is_float) {
        token = lexer_make_token(lexer, TOKEN_FLOAT, lexeme, start_line, start_column);
        token->value.float_value = atof(lexeme);
    } else {
        token = lexer_make_token(lexer, TOKEN_INTEGER, lexeme, start_line, start_column);
        token->value.int_value = atoll(lexeme);
    }
    
    free(lexeme);
    return token;
}

static Token *lexer_lex_string(Lexer *lexer) {
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;
    
    lexer_advance(lexer); // Skip opening quote
    
    char buffer[1024];
    size_t len = 0;
    
    while (lexer->current != '"' && lexer->current != '\0') {
        if (len >= sizeof(buffer) - 2) {  // Check BEFORE adding characters
            return lexer_error_token(lexer, "string too long");
        }
        
        char c = lexer->current;
        
        if (c == '\\') {
            lexer_advance(lexer); // Skip backslash
            if (lexer->current == '\0') {
                return lexer_error_token(lexer, "unterminated string");
            }
            
            char next = lexer->current;
            switch (next) {
                case 'n':  buffer[len++] = '\n'; break;
                case 't':  buffer[len++] = '\t'; break;
                case 'r':  buffer[len++] = '\r'; break;
                case '\\': buffer[len++] = '\\'; break;
                case '"':  buffer[len++] = '"'; break;
                default:
                    buffer[len++] = '\\';
                    if (len >= sizeof(buffer) - 1) {
                        return lexer_error_token(lexer, "string too long");
                    }
                    buffer[len++] = next;
                    break;
            }
            lexer_advance(lexer);
        } else {
            buffer[len++] = c;
            lexer_advance(lexer);
        }
    }
    
    if (lexer->current != '"') {
        return lexer_error_token(lexer, "unterminated string");
    }
    
    lexer_advance(lexer); // Skip closing quote
    buffer[len] = '\0';
    
    Token *token = lexer_make_token(lexer, TOKEN_STRING, buffer, start_line, start_column);
    return token;
}

Token *lexer_next_token(Lexer *lexer) {
    // Skip whitespace and comments
    while (true) {
        lexer_skip_whitespace(lexer);
        if (!lexer_skip_comment(lexer)) {
            break;
        }
    }
    
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;
    
    // End of file
    if (lexer->current == '\0') {
        return lexer_make_token(lexer, TOKEN_EOF, "", start_line, start_column);
    }
    
    // Identifier or keyword
    if (isalpha(lexer->current) || lexer->current == '_') {
        return lexer_lex_identifier(lexer);
    }
    
    // Number
    if (isdigit(lexer->current)) {
        return lexer_lex_number(lexer);
    }
    
    // String
    if (lexer->current == '"') {
        return lexer_lex_string(lexer);
    }
    
    // Two-character operators
    char current = lexer->current;
    char next = lexer_peek_next(lexer);
    
    if (current == '=' && next == '=') {
        lexer_advance(lexer);
        lexer_advance(lexer);
        return lexer_make_token(lexer, TOKEN_EQ_EQ, "==", start_line, start_column);
    }
    if (current == '!' && next == '=') {
        lexer_advance(lexer);
        lexer_advance(lexer);
        return lexer_make_token(lexer, TOKEN_BANG_EQ, "!=", start_line, start_column);
    }
    if (current == '<' && next == '=') {
        lexer_advance(lexer);
        lexer_advance(lexer);
        return lexer_make_token(lexer, TOKEN_LT_EQ, "<=", start_line, start_column);
    }
    if (current == '>' && next == '=') {
        lexer_advance(lexer);
        lexer_advance(lexer);
        return lexer_make_token(lexer, TOKEN_GT_EQ, ">=", start_line, start_column);
    }
    if (current == '&' && next == '&') {
        lexer_advance(lexer);
        lexer_advance(lexer);
        return lexer_make_token(lexer, TOKEN_AMP_AMP, "&&", start_line, start_column);
    }
    if (current == '|' && next == '|') {
        lexer_advance(lexer);
        lexer_advance(lexer);
        return lexer_make_token(lexer, TOKEN_PIPE_PIPE, "||", start_line, start_column);
    }
    if (current == '-' && next == '>') {
        lexer_advance(lexer);
        lexer_advance(lexer);
        return lexer_make_token(lexer, TOKEN_ARROW, "->", start_line, start_column);
    }
    if (current == ':' && next == ':') {
        lexer_advance(lexer);
        lexer_advance(lexer);
        return lexer_make_token(lexer, TOKEN_COLON_COLON, "::", start_line, start_column);
    }
    // Single-character tokens and two-character operators
    char c = lexer->current;
    lexer_advance(lexer); // Consume the current character
    
    switch (c) {
        case '(': return lexer_make_token(lexer, TOKEN_LPAREN, "(", start_line, start_column);
        case ')': return lexer_make_token(lexer, TOKEN_RPAREN, ")", start_line, start_column);
        case '{': return lexer_make_token(lexer, TOKEN_LBRACE, "{", start_line, start_column);
        case '}': return lexer_make_token(lexer, TOKEN_RBRACE, "}", start_line, start_column);
        case '[': return lexer_make_token(lexer, TOKEN_LBRACKET, "[", start_line, start_column);
        case ']': return lexer_make_token(lexer, TOKEN_RBRACKET, "]", start_line, start_column);
        case ';': return lexer_make_token(lexer, TOKEN_SEMICOLON, ";", start_line, start_column);
        case ',': return lexer_make_token(lexer, TOKEN_COMMA, ",", start_line, start_column);
        
        case '-':
            if (lexer_match(lexer, '>')) {
                 return lexer_make_token(lexer, TOKEN_ARROW, "->", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_MINUS, "-", start_line, start_column);
            
        case '+': return lexer_make_token(lexer, TOKEN_PLUS, "+", start_line, start_column);
        case '*': return lexer_make_token(lexer, TOKEN_STAR, "*", start_line, start_column);
        case '/': 
            if (lexer_match(lexer, '/')) {
                // Comment
                while (lexer_peek(lexer) != '\n' && !lexer_is_at_end(lexer)) {
                    lexer_advance(lexer);
                }
                return lexer_next_token(lexer); // Re-lex after comment
            }
            return lexer_make_token(lexer, TOKEN_SLASH, "/", start_line, start_column);
            
        case '%': return lexer_make_token(lexer, TOKEN_PERCENT, "%", start_line, start_column);
        
        case '=':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TOKEN_EQ_EQ, "==", start_line, start_column);
            }
            if (lexer_match(lexer, '>')) {
                return lexer_make_token(lexer, TOKEN_FAT_ARROW, "=>", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_EQ, "=", start_line, start_column);
            
        case '!':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TOKEN_BANG_EQ, "!=", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_BANG, "!", start_line, start_column);
            
        case '<':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TOKEN_LT_EQ, "<=", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_LT, "<", start_line, start_column);
            
        case '>':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TOKEN_GT_EQ, ">=", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_GT, ">", start_line, start_column);
            
        case '&':
            if (lexer_match(lexer, '&')) {
                return lexer_make_token(lexer, TOKEN_AMP_AMP, "&&", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_AMP, "&", start_line, start_column);
            
        case '|':
            if (lexer_match(lexer, '|')) {
                return lexer_make_token(lexer, TOKEN_PIPE_PIPE, "||", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_PIPE, "|", start_line, start_column);
            
        case ':':
            if (lexer_match(lexer, ':')) {
                return lexer_make_token(lexer, TOKEN_COLON_COLON, "::", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_COLON, ":", start_line, start_column);
            
        case '.':
            if (lexer_match(lexer, '.')) {
               if (lexer_match(lexer, '.')) {
                   return lexer_make_token(lexer, TOKEN_ELLIPSIS, "...", start_line, start_column);
               }
               return lexer_make_token(lexer, TOKEN_DOT_DOT, "..", start_line, start_column);
            }
            return lexer_make_token(lexer, TOKEN_DOT, ".", start_line, start_column);
        
        default: {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "unexpected character '%c'", c);
            return lexer_error_token(lexer, error_msg);
        }
    }
}
