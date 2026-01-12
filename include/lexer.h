#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include <stdbool.h>
#include "token.h"

// Lexer state
typedef struct {
    const char *source;     // Source code
    const char *filename;   // Filename for error reporting
    size_t pos;            // Current position in source
    size_t line;           // Current line (1-indexed)
    size_t column;         // Current column (1-indexed)
    char current;          // Current character
} Lexer;

// Lexer functions
Lexer *lexer_init(const char *source, const char *filename);
void lexer_free(Lexer *lexer);
Token *lexer_next_token(Lexer *lexer);

#endif // LEXER_H
