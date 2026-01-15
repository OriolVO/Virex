#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>
#include <stdbool.h>

// Token types
typedef enum {
    // Keywords
    TOKEN_VAR,
    TOKEN_CONST,
    TOKEN_FUNC,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_RETURN,
    TOKEN_STRUCT,
    TOKEN_ENUM,
    TOKEN_UNSAFE,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_PUBLIC,
    TOKEN_MODULE,
    TOKEN_IMPORT,
    TOKEN_EXTERN,
    TOKEN_AS,
    TOKEN_MATCH,
    TOKEN_RESULT,
    TOKEN_PACKED,
    TOKEN_FAIL,
    TOKEN_NULL,
    TOKEN_IN,
    TOKEN_CAST,
    
    // Primitive types
    TOKEN_I8,
    TOKEN_I16,
    TOKEN_I32,
    TOKEN_I64,
    TOKEN_U8,
    TOKEN_U16,
    TOKEN_U32,
    TOKEN_U64,
    TOKEN_F32,
    TOKEN_F64,
    TOKEN_BOOL,
    TOKEN_VOID,
    
    // C ABI types
    TOKEN_C_CHAR,
    TOKEN_C_SHORT,
    TOKEN_C_USHORT,
    TOKEN_C_INT,
    TOKEN_C_UINT,
    TOKEN_C_LONG,
    TOKEN_C_ULONG,
    TOKEN_C_LONGLONG,
    TOKEN_C_ULONGLONG,
    TOKEN_C_LONGDOUBLE,
    TOKEN_CSTRING,
    
    // Operators
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_EQ,           // =
    TOKEN_EQ_EQ,        // ==
    TOKEN_BANG,         // !
    TOKEN_BANG_EQ,      // !=
    TOKEN_LT,           // <
    TOKEN_LT_EQ,        // <=
    TOKEN_GT,           // >
    TOKEN_GT_EQ,        // >=
    TOKEN_AMP,          // &
    TOKEN_AMP_AMP,      // &&
    TOKEN_PIPE,         // |
    TOKEN_PIPE_PIPE,    // ||
    TOKEN_ARROW,        // ->
    TOKEN_FAT_ARROW,    // =>
    
    // Delimiters
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACKET,     // [
    TOKEN_RBRACKET,     // ]
    TOKEN_SEMICOLON,    // ;
    TOKEN_COMMA,        // ,
    TOKEN_DOT,          // .
    TOKEN_COLON,        // :
    TOKEN_COLON_COLON,  // ::
    TOKEN_DOT_DOT,      // ..
    TOKEN_ELLIPSIS,     // ...
    
    // Literals
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_TRUE,
    TOKEN_FALSE,
    
    // Special
    TOKEN_IDENTIFIER,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    char *lexeme;       // The actual text
    size_t line;        // Line number (1-indexed)
    size_t column;      // Column number (1-indexed)
    
    // For literals
    union {
        long long int_value;
        double float_value;
        char *string_value;
        bool bool_value;
    } value;
} Token;

// Token functions
Token *token_create(TokenType type, const char *lexeme, size_t line, size_t column);
void token_free(Token *token);
const char *token_type_name(TokenType type);
void token_print(const Token *token);

#endif // TOKEN_H
