#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"
#include "ast.h"

// Parser state
typedef struct {
    Lexer *lexer;
    Token *current;
    Token *previous;
    bool had_error;
    bool panic_mode;
} Parser;

// Parser functions
Parser *parser_init(Lexer *lexer);
void parser_free(Parser *parser);
ASTProgram *parser_parse(Parser *parser);

#endif // PARSER_H
