#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/token.h"

Token *token_create(TokenType type, const char *lexeme, size_t line, size_t column) {
    Token *token = malloc(sizeof(Token));
    if (!token) {
        fprintf(stderr, "Error: Failed to allocate memory for token\n");
        exit(1);
    }
    
    token->type = type;
    token->lexeme = strdup(lexeme);
    token->line = line;
    token->column = column;
    
    return token;
}

void token_free(Token *token) {
    if (token) {
        free(token->lexeme);
        free(token);
    }
}

const char *token_type_name(TokenType type) {
    switch (type) {
        // Keywords
        case TOKEN_VAR: return "VAR";
        case TOKEN_CONST: return "CONST";
        case TOKEN_FUNC: return "FUNC";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_STRUCT: return "STRUCT";
        case TOKEN_ENUM: return "ENUM";
        case TOKEN_UNSAFE: return "UNSAFE";
        case TOKEN_PUBLIC: return "PUBLIC";
        case TOKEN_MODULE: return "MODULE";
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_EXTERN: return "EXTERN";
        case TOKEN_MATCH: return "match";
        case TOKEN_FAIL: return "fail";
        case TOKEN_NULL: return "NULL";
        case TOKEN_RESULT: return "result";
        
        // Primitive types
        case TOKEN_I8: return "I8";
        case TOKEN_I16: return "I16";
        case TOKEN_I32: return "I32";
        case TOKEN_I64: return "I64";
        case TOKEN_U8: return "U8";
        case TOKEN_U16: return "U16";
        case TOKEN_U32: return "U32";
        case TOKEN_U64: return "U64";
        case TOKEN_F32: return "F32";
        case TOKEN_F64: return "F64";
        case TOKEN_BOOL: return "BOOL";
        case TOKEN_VOID: return "VOID";
        
        // C ABI types

        
        // Operators
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_EQ: return "EQ";
        case TOKEN_EQ_EQ: return "EQ_EQ";
        case TOKEN_BANG: return "BANG";
        case TOKEN_BANG_EQ: return "BANG_EQ";
        case TOKEN_LT: return "LT";
        case TOKEN_LT_EQ: return "LT_EQ";
        case TOKEN_GT: return "GT";
        case TOKEN_GT_EQ: return "GT_EQ";
        case TOKEN_AMP: return "AMP";
        case TOKEN_AMP_AMP: return "AMP_AMP";
        case TOKEN_PIPE: return "PIPE";
        case TOKEN_PIPE_PIPE: return "PIPE_PIPE";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_FAT_ARROW: return "FAT_ARROW";
        
        // Delimiters
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_DOT: return "DOT";
        case TOKEN_COLON: return "COLON";
        case TOKEN_COLON_COLON: return "COLON_COLON";
        case TOKEN_ELLIPSIS: return "ELLIPSIS";
        
        // Literals
        case TOKEN_INTEGER: return "INTEGER";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_STRING: return "STRING";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        
        // Special
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        
        default: return "UNKNOWN";
    }
}

void token_print(const Token *token) {
    printf("%-15s %-20s (%zu:%zu)", 
           token_type_name(token->type),
           token->lexeme,
           token->line,
           token->column);
    
    // Print value for literals
    if (token->type == TOKEN_INTEGER) {
        printf(" [value: %lld]", token->value.int_value);
    } else if (token->type == TOKEN_FLOAT) {
        printf(" [value: %g]", token->value.float_value);
    } else if (token->type == TOKEN_STRING) {
        printf(" [value: \"%s\"]", token->value.string_value);
    } else if (token->type == TOKEN_TRUE || token->type == TOKEN_FALSE) {
        printf(" [value: %s]", token->value.bool_value ? "true" : "false");
    }
    
    printf("\n");
}
