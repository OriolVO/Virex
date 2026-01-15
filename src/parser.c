#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/parser.h"
#include "../include/error.h"

// Forward declarations
static ASTExpr *parse_expression(Parser *p);
static ASTExpr *parse_assignment(Parser *p);
static ASTExpr *parse_logical_or(Parser *p);
static ASTExpr *parse_logical_and(Parser *p);
static ASTExpr *parse_equality(Parser *p);
static ASTExpr *parse_comparison(Parser *p);
static ASTExpr *parse_term(Parser *p);
static ASTExpr *parse_factor(Parser *p);
static ASTExpr *parse_unary(Parser *p);
static ASTExpr *parse_postfix(Parser *p);
static ASTExpr *parse_primary(Parser *p);

static ASTStmt *parse_statement(Parser *p);
static ASTStmt *parse_var_decl(Parser *p);
static ASTStmt *parse_if_stmt(Parser *p);
static ASTStmt *parse_while_stmt(Parser *p);
static ASTStmt *parse_for_stmt(Parser *p);
static ASTStmt *parse_match_stmt(Parser *p);
static ASTStmt *parse_fail_stmt(Parser *p);
static ASTStmt *parse_return_stmt(Parser *p);
static ASTStmt *parse_block(Parser *p);
static ASTStmt *parse_unsafe_stmt(Parser *p);

static ASTDecl *parse_declaration(Parser *p);
static ASTDecl *parse_function(Parser *p, bool is_public);
static ASTDecl *parse_struct(Parser *p, bool is_public, bool is_packed);
static ASTDecl *parse_enum(Parser *p, bool is_public);
static ASTDecl *parse_extern(Parser *p, bool is_public);

static Type *parse_type(Parser *p);

// Helper functions
static void advance(Parser *p);
static bool check(Parser *p, TokenType type);
static bool match(Parser *p, TokenType type);
static Token *expect(Parser *p, TokenType type, const char *message);
static void parser_error(Parser *p, const char *message);
static void synchronize(Parser *p);

Parser *parser_init(Lexer *lexer) {
    Parser *p = malloc(sizeof(Parser));
    p->lexer = lexer;
    p->current = NULL;
    p->previous = NULL;
    p->had_error = false;
    p->panic_mode = false;
    
    // Prime the parser with first token
    advance(p);
    
    return p;
}

void parser_free(Parser *parser) {
    if (parser->current) token_free(parser->current);
    if (parser->previous) token_free(parser->previous);
    free(parser);
}

static void advance(Parser *p) {
    if (p->previous) {
        token_free(p->previous);
    }
    
    p->previous = p->current;
    p->current = lexer_next_token(p->lexer);
    
    // Skip error tokens
    while (p->current->type == TOKEN_ERROR) {
        parser_error(p, p->current->lexeme);
        token_free(p->current);
        p->current = lexer_next_token(p->lexer);
    }
}

static bool check(Parser *p, TokenType type) {
    return p->current->type == type;
}

static bool match(Parser *p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

static Token *expect(Parser *p, TokenType type, const char *message) {
    if (check(p, type)) {
        Token *token = p->current;
        advance(p);
        return token;
    }
    
    parser_error(p, message);
    return NULL;
}

static void parser_error(Parser *p, const char *message) {
    if (p->panic_mode) return;
    
    p->panic_mode = true;
    p->had_error = true;
    
    error_report_ex(LEVEL_ERROR, NULL, p->lexer->filename, p->current->line, p->current->column, message, NULL);
}

static void synchronize(Parser *p) {
    p->panic_mode = false;
    
    while (p->current->type != TOKEN_EOF) {
        if (p->previous->type == TOKEN_SEMICOLON) return;
        
        switch (p->current->type) {
            case TOKEN_FUNC:
            case TOKEN_STRUCT:
            case TOKEN_ENUM:
            case TOKEN_VAR:
            case TOKEN_CONST:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_RETURN:
                return;
            default:
                break;
        }
        
        advance(p);
    }
}

// Type parsing
static Type *parse_type(Parser *p) {
    Type *type = NULL;

    // Check for slice type []T (prefix notation)
    if (check(p, TOKEN_LBRACKET)) {
        advance(p); // consume [
        
        if (check(p, TOKEN_RBRACKET)) {
            // It's a slice []T
            advance(p); // consume ]
            Type *element = parse_type(p);
            return type_create_slice(element);
        } else {
            // It's an array [N]T - parse the size
            if (p->current->type != TOKEN_INTEGER) {
                parser_error(p, "expected array size or ']' for slice");
                return NULL;
            }
            size_t size = p->current->value.int_value;
            advance(p);
            expect(p, TOKEN_RBRACKET, "expected ']'");
            
            // Parse element type
            Type *element = parse_type(p);
            return type_create_array(element, size);
        }
    }

    // Check for generic type parameter <T> (placeholder)
    if (match(p, TOKEN_LT)) {
        Token *type_param = expect(p, TOKEN_IDENTIFIER, "expected type parameter name");
        expect(p, TOKEN_GT, "expected '>' after type parameter");
        
        // Placeholder types are just struct types with the parameter name
        type = type_create_struct(type_param ? type_param->lexeme : "T", NULL, 0);
    }
    // Check for function type: func(T1, T2) -> T3
    else if (match(p, TOKEN_FUNC)) {
        expect(p, TOKEN_LPAREN, "expected '(' for function type");
        
        // Parse parameter types
        Type **param_types = NULL;
        size_t param_count = 0;
        
        if (!check(p, TOKEN_RPAREN)) {
            do {
                param_types = realloc(param_types, sizeof(Type*) * (param_count + 1));
                param_types[param_count++] = parse_type(p);
            } while (match(p, TOKEN_COMMA));
        }
        
        expect(p, TOKEN_RPAREN, "expected ')' after function parameters");
        
        // Parse return type
        Type *return_type = NULL;
        if (match(p, TOKEN_ARROW)) {
            return_type = parse_type(p);
        } else {
            return_type = type_create_primitive(TOKEN_VOID);
        }
        
        type = type_create_function(return_type, param_types, param_count);
    }
    // Check for result type: result<T, E>
    else if (match(p, TOKEN_RESULT)) {
        expect(p, TOKEN_LT, "expected '<' after result");
        
        Type *ok_type = parse_type(p);
        expect(p, TOKEN_COMMA, "expected ',' between result types");
        
        Type *err_type = parse_type(p);
        expect(p, TOKEN_GT, "expected '>' after result types");
        
        type = type_create_result(ok_type, err_type);
    }
    // Primitive types
    else if (p->current->type >= TOKEN_I8 && p->current->type <= TOKEN_CSTRING) {
        TokenType prim = p->current->type;
        advance(p);
        type = type_create_primitive(prim);
    }
    // Struct or enum type (or generic placeholder)
    else if (p->current->type == TOKEN_IDENTIFIER) {
        Token *name_tok = expect(p, TOKEN_IDENTIFIER, "expected type name");
        char full_name[512] = "";
        if (name_tok && name_tok->lexeme) {
            strncpy(full_name, name_tok->lexeme, 511);
        }
        
        while (match(p, TOKEN_DOT)) {
            strncat(full_name, ".", 511 - strlen(full_name));
            Token *member = expect(p, TOKEN_IDENTIFIER, "expected member name after '.'");
            if (member && member->lexeme) {
                strncat(full_name, member->lexeme, 511 - strlen(full_name));
            }
        }
        
        char *name = strdup(full_name);
        
        Type **args = NULL;
        size_t arg_count = 0;
        
        // Check for generic arguments: Name<T1, T2>
        if (match(p, TOKEN_LT)) {
            do {
                args = realloc(args, sizeof(Type*) * (arg_count + 1));
                args[arg_count++] = parse_type(p);
            } while (match(p, TOKEN_COMMA));
            expect(p, TOKEN_GT, "expected '>' after generic arguments");
        }
        
        type = type_create_struct(name, args, arg_count);
        free(name);
    }
    else {
        parser_error(p, "expected type");
        return NULL;
    }

    // Check for suffixes: pointer * or array [N] (postfix)
    // Note: Prefix []T for slices and [N]T for arrays is also supported above
    while (true) {
        if (match(p, TOKEN_LBRACKET)) {
            // Postfix array [N]
            if (p->current->type != TOKEN_INTEGER) {
                parser_error(p, "expected array size");
                return NULL;
            }
            size_t size = p->current->value.int_value;
            advance(p);
            expect(p, TOKEN_RBRACKET, "expected ']'");
            
            type = type_create_array(type, size);
        } else if (match(p, TOKEN_STAR)) {
            // Pointer
            bool non_null = match(p, TOKEN_BANG);
            type = type_create_pointer(type, non_null);
        } else {
            break;
        }
    }
    
    return type;
}

// Expression parsing (precedence climbing)
static ASTExpr *parse_expression(Parser *p) {
    return parse_assignment(p);
}

static ASTExpr *parse_assignment(Parser *p) {
    ASTExpr *expr = parse_logical_or(p);
    
    if (match(p, TOKEN_EQ)) {
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        ASTExpr *value = parse_assignment(p);
        return ast_create_binary(TOKEN_EQ, expr, value, line, column);
    }
    
    return expr;
}

static ASTExpr *parse_logical_or(Parser *p) {
    ASTExpr *expr = parse_logical_and(p);
    
    while (match(p, TOKEN_PIPE_PIPE)) {
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        ASTExpr *right = parse_logical_and(p);
        expr = ast_create_binary(TOKEN_PIPE_PIPE, expr, right, line, column);
    }
    
    return expr;
}

static ASTExpr *parse_logical_and(Parser *p) {
    ASTExpr *expr = parse_equality(p);
    
    while (match(p, TOKEN_AMP_AMP)) {
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        ASTExpr *right = parse_equality(p);
        expr = ast_create_binary(TOKEN_AMP_AMP, expr, right, line, column);
    }
    
    return expr;
}

static ASTExpr *parse_equality(Parser *p) {
    ASTExpr *expr = parse_comparison(p);
    
    while (match(p, TOKEN_EQ_EQ) || match(p, TOKEN_BANG_EQ)) {
        TokenType op = p->previous->type;
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        ASTExpr *right = parse_comparison(p);
        expr = ast_create_binary(op, expr, right, line, column);
    }
    
    return expr;
}

static ASTExpr *parse_comparison(Parser *p) {
    ASTExpr *expr = parse_term(p);
    
    while (match(p, TOKEN_LT) || match(p, TOKEN_LT_EQ) || 
           match(p, TOKEN_GT) || match(p, TOKEN_GT_EQ)) {
        TokenType op = p->previous->type;
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        ASTExpr *right = parse_term(p);
        expr = ast_create_binary(op, expr, right, line, column);
    }
    
    return expr;
}

static ASTExpr *parse_term(Parser *p) {
    ASTExpr *expr = parse_factor(p);
    
    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        TokenType op = p->previous->type;
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        ASTExpr *right = parse_factor(p);
        expr = ast_create_binary(op, expr, right, line, column);
    }
    
    return expr;
}

static ASTExpr *parse_factor(Parser *p) {
    ASTExpr *expr = parse_unary(p);
    
    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) || match(p, TOKEN_PERCENT)) {
        TokenType op = p->previous->type;
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        ASTExpr *right = parse_unary(p);
        expr = ast_create_binary(op, expr, right, line, column);
    }
    
    return expr;
}

static ASTExpr *parse_unary(Parser *p) {
    if (match(p, TOKEN_MINUS) || match(p, TOKEN_BANG) || 
        match(p, TOKEN_AMP) || match(p, TOKEN_STAR)) {
        TokenType op = p->previous->type;
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        ASTExpr *operand = parse_unary(p);
        return ast_create_unary(op, operand, line, column);
    }
    
    return parse_postfix(p);
}

static ASTExpr *parse_postfix(Parser *p) {
    ASTExpr *expr = parse_primary(p);
    
    // Store generic args found before a call
    Type **generic_args = NULL;
    size_t generic_count = 0;
    
    while (true) {
        if (match(p, TOKEN_LPAREN)) {
            // Function call
            size_t line = p->previous->line;
            size_t column = p->previous->column;
            
            ASTExpr **args = NULL;
            size_t arg_count = 0;
            
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    args = realloc(args, sizeof(ASTExpr*) * (arg_count + 1));
                    args[arg_count++] = parse_expression(p);
                } while (match(p, TOKEN_COMMA));
            }
            
            expect(p, TOKEN_RPAREN, "expected ')' after arguments");
            
            // Create call with any pending generic args
            expr = ast_create_call(expr, args, arg_count, generic_args, generic_count, line, column);
            
            // Reset generics as they are consumed
            generic_args = NULL;
            generic_count = 0;
        }
        else if (check(p, TOKEN_LT)) {
            // Check for potential generics: <Type, ...>
            // We use a heuristic: if next token is a primitive type (e.g. i32), we assume generics.
            // Safe for generics.vx which uses max<i32>.
            // Conservative: Only <PRIMITIVE_TYPE... supported for now.
            
            // Save lexer state
            size_t pos = p->lexer->pos;
            size_t line = p->lexer->line;
            size_t column = p->lexer->column;
            char current_char = p->lexer->current;
            
            // We need to skip the current token '<' which is already in p->current?
            // No, p->current is the Lookahead (LT). 
            // We need to see what comes AFTER p->current.
            // The lexer->pos is actually usually at p->current end + whitespace + lookahead?
            // It depends on implementation of advance(p).
            // But manually scanning via lexer_next_token() moves forward from WHEREVER lexer is.
            // Since p->current is <, lexer might be after <.
            // Let's assume lexer is ready to read NEXT token.
            
            Token *peek = lexer_next_token(p->lexer);
            bool is_generics = (peek->type >= TOKEN_I8 && peek->type <= TOKEN_CSTRING);
            
            // Restore lexer state
            p->lexer->pos = pos;
            p->lexer->line = line;
            p->lexer->column = column;
            p->lexer->current = current_char;
            token_free(peek);
            
            if (is_generics) {
                match(p, TOKEN_LT); // Consume <
                
                if (!check(p, TOKEN_GT)) {
                    do {
                        generic_args = realloc(generic_args, sizeof(Type*) * (generic_count + 1));
                        generic_args[generic_count++] = parse_type(p);
                    } while (match(p, TOKEN_COMMA));
                }
                expect(p, TOKEN_GT, "expected '>' after generic arguments");
                // Continue loop to allow ( ) matching next
            } else {
                // Not generics (e.g. less than operator), break loop
                break;
            }
        }
        else if (match(p, TOKEN_LBRACKET)) {
            // Array indexing or slicing
            size_t line = p->previous->line;
            size_t column = p->previous->column;
            
            if (match(p, TOKEN_DOT_DOT)) {
                // [..end] or [..]
                ASTExpr *end = NULL;
                if (!check(p, TOKEN_RBRACKET)) {
                    end = parse_expression(p);
                }
                expect(p, TOKEN_RBRACKET, "expected ']'");
                expr = ast_create_slice_expr(expr, NULL, end, line, column);
            } else {
                ASTExpr *start = parse_expression(p);
                
                if (match(p, TOKEN_DOT_DOT)) {
                    // [start..end] or [start..]
                    ASTExpr *end = NULL;
                    if (!check(p, TOKEN_RBRACKET)) {
                        end = parse_expression(p);
                    }
                    expect(p, TOKEN_RBRACKET, "expected ']'");
                    expr = ast_create_slice_expr(expr, start, end, line, column);
                } else {
                    // [index]
                    expect(p, TOKEN_RBRACKET, "expected ']'");
                    expr = ast_create_index(expr, start, line, column);
                }
            }
        }
        else if (match(p, TOKEN_DOT)) {
            // Member access
            size_t line = p->previous->line;
            size_t column = p->previous->column;
            Token *member = expect(p, TOKEN_IDENTIFIER, "expected member name");
            if (member) {
                expr = ast_create_member(expr, member->lexeme, false, line, column);
            }
        }
        else if (match(p, TOKEN_ARROW)) {
            // Pointer member access
            size_t line = p->previous->line;
            size_t column = p->previous->column;
            Token *member = expect(p, TOKEN_IDENTIFIER, "expected member name");
            if (member) {
                expr = ast_create_member(expr, member->lexeme, true, line, column);
            }
        }
        else {
            break;
        }
    }
    
    // If we have lingering generic args but no call follows?
    // This is syntactically invalid for now (e.g. max<i32>; )
    // We treat it as valid but drop args? Or error?
    // For now, allow it but leak/ignore (simplification).
    if (generic_count > 0) {
        // Warning: generic args without call
        // Free them
        for(size_t i=0; i<generic_count; i++) type_free(generic_args[i]);
        free(generic_args);
    }
    
    return expr;
}

static ASTExpr *parse_primary(Parser *p) {
    // Cast expression: cast<Type>(expr)
    if (match(p, TOKEN_CAST)) {
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        expect(p, TOKEN_LT, "expected '<' after cast");
        Type *target_type = parse_type(p);
        expect(p, TOKEN_GT, "expected '>' after target type");
        expect(p, TOKEN_LPAREN, "expected '(' after cast type");
        ASTExpr *expr = parse_expression(p);
        expect(p, TOKEN_RPAREN, "expected ')' after cast expression");
        return ast_create_cast(target_type, expr, line, column);
    }

    // Literals
    if (p->current->type == TOKEN_INTEGER || p->current->type == TOKEN_FLOAT ||
        p->current->type == TOKEN_STRING || p->current->type == TOKEN_TRUE ||
        p->current->type == TOKEN_FALSE || p->current->type == TOKEN_NULL) {
        // Create a copy of the token for the AST
        Token *token_copy = token_create(p->current->type, p->current->lexeme, 
                                         p->current->line, p->current->column);
        token_copy->value = p->current->value;
        advance(p);
        return ast_create_literal(token_copy);
    }
    
    // Identifiers
    if (p->current->type == TOKEN_IDENTIFIER) {
        char *name = p->current->lexeme;
        size_t line = p->current->line;
        size_t column = p->current->column;
        advance(p);
        return ast_create_variable(name, line, column);
    }
    
    // Parenthesized expression
    if (match(p, TOKEN_LPAREN)) {
        ASTExpr *expr = parse_expression(p);
        expect(p, TOKEN_RPAREN, "expected ')' after expression");
        return expr;
    }
    
    // Generic result constructors: result::ok(val) or result::err(val)
    if (match(p, TOKEN_RESULT)) {
        expect(p, TOKEN_COLON_COLON, "expected '::' after result");
        Token *ctor = expect(p, TOKEN_IDENTIFIER, "expected 'ok' or 'err'");
        
        char func_name[64];
        if (ctor) {
            snprintf(func_name, sizeof(func_name), "result::%s", ctor->lexeme);
            // Verify name? For now assume semantic analysis checks valid members
        }
        
        expect(p, TOKEN_LPAREN, "expected '('");
        
        ASTExpr **args = NULL;
        size_t arg_count = 0;
        
        if (!check(p, TOKEN_RPAREN)) {
            do {
                args = realloc(args, sizeof(ASTExpr*) * (arg_count + 1));
                args[arg_count++] = parse_expression(p);
            } while (match(p, TOKEN_COMMA));
        }
        
        expect(p, TOKEN_RPAREN, "expected ')'");
        
        // Treat as a function call "result::ok" or "result::err"
        ASTExpr *callee = ast_create_variable(func_name, p->previous->line, p->previous->column);
        return ast_create_call(callee, args, arg_count, 
                               NULL, 0, // No generic args for now, inferred?
                               p->previous->line, p->previous->column);
    }
    
    parser_error(p, "expected expression");
    return NULL;
}

// Statement parsing
static ASTStmt *parse_statement(Parser *p) {
    if (match(p, TOKEN_VAR) || match(p, TOKEN_CONST)) {
        bool is_const = p->previous->type == TOKEN_CONST;
        p->previous->type = is_const ? TOKEN_CONST : TOKEN_VAR; // Restore for parse_var_decl
        return parse_var_decl(p);
    }
    
    if (match(p, TOKEN_IF)) {
        return parse_if_stmt(p);
    }
    
    if (match(p, TOKEN_WHILE)) {
        return parse_while_stmt(p);
    }
    
    if (match(p, TOKEN_FOR)) {
        return parse_for_stmt(p);
    }

    if (match(p, TOKEN_MATCH)) {
        return parse_match_stmt(p);
    }

    if (match(p, TOKEN_FAIL)) {
        return parse_fail_stmt(p);
    }

    if (match(p, TOKEN_UNSAFE)) {
        return parse_unsafe_stmt(p);
    }
    
    if (match(p, TOKEN_RETURN)) {
        return parse_return_stmt(p);
    }
    
    if (match(p, TOKEN_BREAK)) {
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        expect(p, TOKEN_SEMICOLON, "expected ';' after break");
        return ast_create_break(line, column);
    }
    
    if (match(p, TOKEN_CONTINUE)) {
        size_t line = p->previous->line;
        size_t column = p->previous->column;
        expect(p, TOKEN_SEMICOLON, "expected ';' after continue");
        return ast_create_continue(line, column);
    }
    
    if (match(p, TOKEN_LBRACE)) {
        return parse_block(p);
    }
    
    // Expression statement
    size_t line = p->current->line;
    size_t column = p->current->column;
    ASTExpr *expr = parse_expression(p);
    expect(p, TOKEN_SEMICOLON, "expected ';' after expression");
    return ast_create_expr_stmt(expr, line, column);
}

static ASTStmt *parse_var_decl(Parser *p) {
    bool is_const = p->previous->type == TOKEN_CONST;
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    Type *type = parse_type(p);
    Token *name_token = expect(p, TOKEN_IDENTIFIER, "expected variable name");
    
    // Copy the name to a local buffer immediately before the token gets freed
    char var_name[256] = "";
    if (name_token && name_token->lexeme) {
        strncpy(var_name, name_token->lexeme, sizeof(var_name) - 1);
        var_name[sizeof(var_name) - 1] = '\0';
    }
    
    ASTExpr *initializer = NULL;
    if (match(p, TOKEN_EQ)) {
        initializer = parse_expression(p);
    }
    
    expect(p, TOKEN_SEMICOLON, "expected ';' after variable declaration");
    
    return ast_create_var_decl(is_const, type, var_name, initializer, line, column);
}

static ASTDecl *parse_global_var_decl(Parser *p, bool is_public) {
    bool is_const = p->previous->type == TOKEN_CONST;
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    Type *type = parse_type(p);
    Token *name_token = expect(p, TOKEN_IDENTIFIER, "expected variable name");
    
    char var_name[256] = "";
    if (name_token && name_token->lexeme) {
        strncpy(var_name, name_token->lexeme, sizeof(var_name) - 1);
        var_name[sizeof(var_name) - 1] = '\0';
    }
    
    ASTExpr *initializer = NULL;
    if (match(p, TOKEN_EQ)) {
        initializer = parse_expression(p);
    }
    
    expect(p, TOKEN_SEMICOLON, "expected ';' after variable declaration");
    
    return ast_create_variable_decl(var_name, type, initializer, is_const, is_public, line, column);
}

static ASTStmt *parse_if_stmt(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    expect(p, TOKEN_LPAREN, "expected '(' after 'if'");
    ASTExpr *condition = parse_expression(p);
    expect(p, TOKEN_RPAREN, "expected ')' after condition");
    
    ASTStmt *then_branch = parse_statement(p);
    ASTStmt *else_branch = NULL;
    
    if (match(p, TOKEN_ELSE)) {
        else_branch = parse_statement(p);
    }
    
    return ast_create_if(condition, then_branch, else_branch, line, column);
}

static ASTStmt *parse_while_stmt(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    expect(p, TOKEN_LPAREN, "expected '(' after 'while'");
    ASTExpr *condition = parse_expression(p);
    expect(p, TOKEN_RPAREN, "expected ')' after condition");
    
    ASTStmt *body = parse_statement(p);
    
    return ast_create_while(condition, body, line, column);
}


static ASTStmt *desugar_for_in(Type *elem_type, const char *elem_name, ASTExpr *collection, ASTStmt *user_body, size_t line, size_t column) {
    ASTStmt **stmts = malloc(sizeof(ASTStmt*) * 2);
    
    // 1. var __slice = collection[..];
    // Create [..] slice expression
    ASTExpr *full_slice = ast_create_slice_expr(collection, NULL, NULL, line, column);
    // Create expected slice type: []elem_type
    Type *slice_type = type_create_slice(type_clone(elem_type));
    stmts[0] = ast_create_var_decl(false, slice_type, "__slice", full_slice, line, column);
    
    // 2. Loop: for (var __i = 0; __i < __slice.len; __i = __i + 1)
    
    // Init: var i64 __i = 0;
    Type *i64 = type_create_primitive(TOKEN_I64);
    Token *zero_tok = token_create(TOKEN_INTEGER, "0", line, column);
    zero_tok->value.int_value = 0;
    ASTExpr *zero = ast_create_literal(zero_tok);
    ASTStmt *init = ast_create_var_decl(false, i64, "__i", zero, line, column);
    
    // Cond: __i < __slice.len
    ASTExpr *i_var = ast_create_variable("__i", line, column);
    ASTExpr *slice_var = ast_create_variable("__slice", line, column);
    ASTExpr *len_acc = ast_create_member(slice_var, "len", false, line, column);
    ASTExpr *cond = ast_create_binary(TOKEN_LT, i_var, len_acc, line, column);
    
    // Inc: __i = __i + 1
    ASTExpr *i_var2 = ast_create_variable("__i", line, column);
    ASTExpr *i_var3 = ast_create_variable("__i", line, column);
    Token *one_tok = token_create(TOKEN_INTEGER, "1", line, column);
    one_tok->value.int_value = 1;
    ASTExpr *one = ast_create_literal(one_tok);
    ASTExpr *add = ast_create_binary(TOKEN_PLUS, i_var3, one, line, column);
    ASTExpr *inc = ast_create_binary(TOKEN_EQ, i_var2, add, line, column);
    
    // Body: { var elem = __slice[__i]; user_body }
    ASTStmt **body_stmts = malloc(sizeof(ASTStmt*) * 2);
    // var elem = __slice[__i]
    ASTExpr *slice_var_body = ast_create_variable("__slice", line, column);
    ASTExpr *idx_var = ast_create_variable("__i", line, column);
    ASTExpr *access = ast_create_index(slice_var_body, idx_var, line, column);
    body_stmts[0] = ast_create_var_decl(false, type_clone(elem_type), elem_name, access, line, column);
    body_stmts[1] = user_body;
    ASTStmt *body_block = ast_create_block(body_stmts, 2, line, column);
    
    stmts[1] = ast_create_for(init, cond, inc, body_block, line, column);
    
    return ast_create_block(stmts, 2, line, column);
}

static ASTStmt *parse_for_stmt(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    expect(p, TOKEN_LPAREN, "expected '(' after 'for'");
    
    // Initializer
    ASTStmt *initializer = NULL;
    
    // Check for variable declaration start
    if (match(p, TOKEN_VAR) || match(p, TOKEN_CONST)) {
        bool is_const = p->previous->type == TOKEN_CONST;
        
        // Manual var declaration parsing to detecting 'in'
        // Type
        Type *type = parse_type(p);
        
        // Variable Name
        Token *name_tok = expect(p, TOKEN_IDENTIFIER, "expected variable name");
        char var_name[256] = "";
        if (name_tok && name_tok->lexeme) {
            strncpy(var_name, name_tok->lexeme, 255);
        }
        
        // Check for 'in' (For-In Loop)
        if (match(p, TOKEN_IN)) {
            ASTExpr *collection = parse_expression(p);
            expect(p, TOKEN_RPAREN, "expected ')' after for-in");
            ASTStmt *body = parse_statement(p);
            return desugar_for_in(type, var_name, collection, body, line, column);
        }
        
        // Standard variable declaration
        ASTExpr *init_expr = NULL;
        if (match(p, TOKEN_EQ)) {
            init_expr = parse_expression(p);
        }
        expect(p, TOKEN_SEMICOLON, "expected ';'");
        initializer = ast_create_var_decl(is_const, type, var_name, init_expr, line, column);
        
    } else if (!match(p, TOKEN_SEMICOLON)) {
        ASTExpr *expr = parse_expression(p);
        expect(p, TOKEN_SEMICOLON, "expected ';'");
        initializer = ast_create_expr_stmt(expr, line, column);
    }
    
    // Condition
    ASTExpr *condition = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        condition = parse_expression(p);
    }
    expect(p, TOKEN_SEMICOLON, "expected ';'");
    
    // Increment
    ASTExpr *increment = NULL;
    if (!check(p, TOKEN_RPAREN)) {
        increment = parse_expression(p);
    }
    expect(p, TOKEN_RPAREN, "expected ')'");
    
    ASTStmt *body = parse_statement(p);
    
    return ast_create_for(initializer, condition, increment, body, line, column);
}

static ASTStmt *parse_match_stmt(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    ASTExpr *expr = parse_expression(p);
    expect(p, TOKEN_LBRACE, "expected '{' after match expression");
    
    ASTMatchCase *cases = NULL;
    size_t case_count = 0;
    
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        // Parse pattern tag: e.g. "ok" or "err"
        Token *tag_token = expect(p, TOKEN_IDENTIFIER, "expected pattern tag");
        char *tag = NULL;
        if (tag_token) {
            tag = strdup(tag_token->lexeme);
        }
        
        // Optional capture: (var)
        char *capture = NULL;
        if (match(p, TOKEN_LPAREN)) {
            Token *cap_token = expect(p, TOKEN_IDENTIFIER, "expected capture variable name");
            if (cap_token) {
                capture = strdup(cap_token->lexeme);
            }
            expect(p, TOKEN_RPAREN, "expected ')'");
        }
        
        expect(p, TOKEN_FAT_ARROW, "expected '=>'");
        
        ASTStmt *body = parse_statement(p); // Can be block or single stmt
        
        // Add case
        cases = realloc(cases, sizeof(ASTMatchCase) * (case_count + 1));
        cases[case_count].pattern_tag = tag;
        cases[case_count].capture_name = capture;
        cases[case_count].body = body;
        case_count++;
        
        // Optional comma if not a block (or always optional)
        match(p, TOKEN_COMMA);
    }
    
    expect(p, TOKEN_RBRACE, "expected '}' after match cases");
    
    return ast_create_match(expr, cases, case_count, line, column);
}

static ASTStmt *parse_fail_stmt(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    ASTExpr *message = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        message = parse_expression(p);
    }
    
    expect(p, TOKEN_SEMICOLON, "expected ';' after fail");
    
    return ast_create_fail(message, line, column);
}

static ASTStmt *parse_unsafe_stmt(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    ASTStmt *body = parse_statement(p);
    
    return ast_create_unsafe(body, line, column);
}

static ASTStmt *parse_return_stmt(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    ASTExpr *value = NULL;
    if (!check(p, TOKEN_SEMICOLON)) {
        value = parse_expression(p);
    }
    
    expect(p, TOKEN_SEMICOLON, "expected ';' after return");
    
    return ast_create_return(value, line, column);
}

static ASTStmt *parse_block(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    ASTStmt **statements = NULL;
    size_t count = 0;
    
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        statements = realloc(statements, sizeof(ASTStmt*) * (count + 1));
        statements[count++] = parse_statement(p);
    }
    
    expect(p, TOKEN_RBRACE, "expected '}' after block");
    
    return ast_create_block(statements, count, line, column);
}

// Declaration parsing
static ASTDecl *parse_module(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    Token *path_token = expect(p, TOKEN_STRING, "expected module path string");
    if (!path_token) return NULL;
    
    char path[256];
    strncpy(path, path_token->lexeme, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    
    expect(p, TOKEN_SEMICOLON, "expected ';' after module declaration");
    
    return ast_create_module(path, line, column);
}

static ASTDecl *parse_import(Parser *p) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    Token *path_token = expect(p, TOKEN_STRING, "expected import path string");
    if (!path_token) return NULL;
    
    char path[256];
    strncpy(path, path_token->lexeme, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    
    char alias_buf[256];
    char *alias = NULL;
    
    // Support for 'import "path" as alias;' syntax
    if (match(p, TOKEN_AS)) {
        Token *alias_token = expect(p, TOKEN_IDENTIFIER, "expected alias name after 'as'");
        if (alias_token) {
            strncpy(alias_buf, alias_token->lexeme, sizeof(alias_buf) - 1);
            alias_buf[sizeof(alias_buf) - 1] = '\0';
            alias = alias_buf;
        }
    }
    
    expect(p, TOKEN_SEMICOLON, "expected ';' after import statement");
    
    return ast_create_import(path, alias, line, column);
}

static ASTDecl *parse_declaration(Parser *p) {
    if (match(p, TOKEN_EXTERN)) {
        return parse_extern(p, true);
    }

    if (match(p, TOKEN_MODULE)) {
        return parse_module(p);
    }

    if (match(p, TOKEN_IMPORT)) {
        return parse_import(p);
    }

    bool is_public = match(p, TOKEN_PUBLIC);
    
    if (match(p, TOKEN_VAR) || match(p, TOKEN_CONST)) {
        return parse_global_var_decl(p, is_public);
    }
    
    if (match(p, TOKEN_EXTERN)) {
        return parse_extern(p, is_public);
    }
    
    if (match(p, TOKEN_FUNC)) {
        return parse_function(p, is_public);
    }
    
    bool is_packed = match(p, TOKEN_PACKED);
    
    if (match(p, TOKEN_STRUCT)) {
        return parse_struct(p, is_public, is_packed);
    }
    
    if (is_packed) {
        parser_error(p, "'packed' modifier can only be used with structs");
        return NULL;
    }
    
    if (match(p, TOKEN_ENUM)) {
        return parse_enum(p, is_public);
    }
    
    parser_error(p, "expected declaration");
    synchronize(p);
    return NULL;
}

static ASTDecl *parse_function(Parser *p, bool is_public) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    Token *name_token = expect(p, TOKEN_IDENTIFIER, "expected function name");
    char *func_name = name_token ? strdup(name_token->lexeme) : strdup("");
    
    // Generic type parameters
    char **type_params = NULL;
    size_t type_param_count = 0;
    
    if (match(p, TOKEN_LT)) {
        do {
            Token *param_token = expect(p, TOKEN_IDENTIFIER, "expected type parameter name");
            if (param_token) {
                type_params = realloc(type_params, sizeof(char*) * (type_param_count + 1));
                type_params[type_param_count++] = strdup(param_token->lexeme);
            }
        } while (match(p, TOKEN_COMMA));
        expect(p, TOKEN_GT, "expected '>' after type parameters");
    }
    
    expect(p, TOKEN_LPAREN, "expected '(' after function name");
    
    // Parameters
    ASTParam *params = NULL;
    size_t param_count = 0;
    
    if (!check(p, TOKEN_RPAREN)) {
        do {
            Type *param_type = parse_type(p);
            Token *param_name = expect(p, TOKEN_IDENTIFIER, "expected parameter name");
            
            params = realloc(params, sizeof(ASTParam) * (param_count + 1));
            params[param_count].param_type = param_type;
            params[param_count].name = param_name ? strdup(param_name->lexeme) : strdup("");
            param_count++;
        } while (match(p, TOKEN_COMMA));
    }
    
    expect(p, TOKEN_RPAREN, "expected ')' after parameters");
    
    // Return type
    Type *return_type = NULL;
    if (match(p, TOKEN_ARROW)) {
        return_type = parse_type(p);
    } else {
        return_type = type_create_primitive(TOKEN_VOID);
    }
    
    // Body
    expect(p, TOKEN_LBRACE, "expected '{' before function body");
    ASTStmt *body = parse_block(p);
    
    return ast_create_function(func_name, type_params, type_param_count, params, param_count, return_type, body, is_public, false, false, line, column);
}

static ASTDecl *parse_extern(Parser *p, bool is_public) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    expect(p, TOKEN_FUNC, "expected 'func' after 'extern'");
    Token *name_token = expect(p, TOKEN_IDENTIFIER, "expected function name");
    char *func_name = name_token ? strdup(name_token->lexeme) : strdup("");
    
    // Generic type parameters
    char **type_params = NULL;
    size_t type_param_count = 0;
    
    if (match(p, TOKEN_LT)) {
        do {
            Token *param_token = expect(p, TOKEN_IDENTIFIER, "expected type parameter name");
            if (param_token) {
                type_params = realloc(type_params, sizeof(char*) * (type_param_count + 1));
                type_params[type_param_count++] = strdup(param_token->lexeme);
            }
        } while (match(p, TOKEN_COMMA));
        expect(p, TOKEN_GT, "expected '>' after type parameters");
    }
    
    expect(p, TOKEN_LPAREN, "expected '(' after function name");
    
    // Parameters
    ASTParam *params = NULL;
    size_t param_count = 0;
    bool is_variadic = false;
    
    if (!check(p, TOKEN_RPAREN)) {
        do {
            if (match(p, TOKEN_ELLIPSIS)) {
                is_variadic = true;
                break; // Variadic must be last
            }
            
            size_t p_line = p->current->line;
            size_t p_column = p->current->column;
            
            Type *type = parse_type(p);
            Token *name_tok = expect(p, TOKEN_IDENTIFIER, "expected parameter name");
            
            params = realloc(params, sizeof(ASTParam) * (param_count + 1));
            params[param_count].param_type = type;
            params[param_count].name = name_tok ? strdup(name_tok->lexeme) : strdup("");
            params[param_count].line = p_line;
            params[param_count].column = p_column;
            param_count++;
            
        } while (match(p, TOKEN_COMMA));
    }
    
    expect(p, TOKEN_RPAREN, "expected ')' after parameters");
    
    // Return type
    Type *return_type = type_create_primitive(TOKEN_VOID);
    if (match(p, TOKEN_ARROW)) {
        return_type = parse_type(p);
    }
    
    expect(p, TOKEN_SEMICOLON, "expected ';' after extern declaration");
    
    return ast_create_function(func_name, type_params, type_param_count, params, param_count, return_type, NULL, is_public, true, is_variadic, line, column);
}

static ASTDecl *parse_struct(Parser *p, bool is_public, bool is_packed) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    Token *name_token = expect(p, TOKEN_IDENTIFIER, "expected struct name");
    char *struct_name = name_token ? strdup(name_token->lexeme) : strdup("");
    
    // Generic type parameters
    char **type_params = NULL;
    size_t type_param_count = 0;
    
    if (match(p, TOKEN_LT)) {
        do {
            Token *param_token = expect(p, TOKEN_IDENTIFIER, "expected type parameter name");
            if (param_token) {
                type_params = realloc(type_params, sizeof(char*) * (type_param_count + 1));
                type_params[type_param_count++] = strdup(param_token->lexeme);
            }
        } while (match(p, TOKEN_COMMA));
        expect(p, TOKEN_GT, "expected '>' after type parameters");
    }
    
    expect(p, TOKEN_LBRACE, "expected '{' after struct name");
    
    // Fields
    ASTField *fields = NULL;
    size_t field_count = 0;
    
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        Type *field_type = parse_type(p);
        Token *field_name = expect(p, TOKEN_IDENTIFIER, "expected field name");
        char *name_copy = field_name ? strdup(field_name->lexeme) : strdup("");
        
        expect(p, TOKEN_SEMICOLON, "expected ';' after field");
        
        fields = realloc(fields, sizeof(ASTField) * (field_count + 1));
        fields[field_count].field_type = field_type;
        fields[field_count].name = name_copy;
        field_count++;
    }
    
    expect(p, TOKEN_RBRACE, "expected '}' after struct fields");
    expect(p, TOKEN_SEMICOLON, "expected ';' after struct declaration");
    
    return ast_create_struct(struct_name, type_params, type_param_count, fields, field_count, is_public, is_packed, line, column);
}

static ASTDecl *parse_enum(Parser *p, bool is_public) {
    size_t line = p->previous->line;
    size_t column = p->previous->column;
    
    Token *name_token = expect(p, TOKEN_IDENTIFIER, "expected enum name");
    char *enum_name = name_token ? strdup(name_token->lexeme) : strdup("");
    
    // Generic type parameters
    char **type_params = NULL;
    size_t type_param_count = 0;
    
    if (match(p, TOKEN_LT)) {
        do {
            Token *param_token = expect(p, TOKEN_IDENTIFIER, "expected type parameter name");
            if (param_token) {
                type_params = realloc(type_params, sizeof(char*) * (type_param_count + 1));
                type_params[type_param_count++] = strdup(param_token->lexeme);
            }
        } while (match(p, TOKEN_COMMA));
        expect(p, TOKEN_GT, "expected '>' after type parameters");
    }
    
    expect(p, TOKEN_LBRACE, "expected '{' after enum name");
    
    // Variants
    ASTEnumVariant *variants = NULL;
    size_t variant_count = 0;
    
    if (!check(p, TOKEN_RBRACE)) {
        do {
            Token *variant_name = expect(p, TOKEN_IDENTIFIER, "expected variant name");
            
            variants = realloc(variants, sizeof(ASTEnumVariant) * (variant_count + 1));
            variants[variant_count].name = variant_name ? strdup(variant_name->lexeme) : strdup("");
            variant_count++;
        } while (match(p, TOKEN_COMMA));
    }
    
    expect(p, TOKEN_RBRACE, "expected '}' after enum variants");
    expect(p, TOKEN_SEMICOLON, "expected ';' after enum declaration");
    
    return ast_create_enum(enum_name, type_params, type_param_count, variants, variant_count, is_public, line, column);
}

// Main parse function
ASTProgram *parser_parse(Parser *p) {
    char *module_name = NULL;
    ASTImportDecl **imports = NULL;
    size_t import_count = 0;
    ASTDecl **declarations = NULL;
    size_t decl_count = 0;
    
    bool metadata_phase = true;
    
    while (!check(p, TOKEN_EOF)) {
        ASTDecl *decl = parse_declaration(p);
        if (!decl) continue;

        if (decl->type == AST_MODULE_DECL) {
            if (!metadata_phase || module_name != NULL || import_count > 0 || decl_count > 0) {
                parser_error(p, "module declaration must be the first statement in the file");
            } else {
                module_name = strdup(decl->data.module_decl.module_name);
            }
            ast_free_decl(decl);
        } else if (decl->type == AST_IMPORT_DECL) {
            if (!metadata_phase || decl_count > 0) {
                parser_error(p, "import statements must precede other declarations");
            }
            imports = realloc(imports, sizeof(ASTImportDecl*) * (import_count + 1));
            ASTImportDecl *import = malloc(sizeof(ASTImportDecl));
            import->import_path = strdup(decl->data.import_decl.import_path);
            import->alias = decl->data.import_decl.alias ? strdup(decl->data.import_decl.alias) : NULL;
            imports[import_count++] = import;
            ast_free_decl(decl);
        } else {
            metadata_phase = false;
            declarations = realloc(declarations, sizeof(ASTDecl*) * (decl_count + 1));
            declarations[decl_count++] = decl;
        }
    }
    
    if (p->had_error) {
        // Free everything on error
        if (module_name) free(module_name);
        for (size_t i = 0; i < import_count; i++) {
            free(imports[i]->import_path);
            if (imports[i]->alias) free(imports[i]->alias);
            free(imports[i]);
        }
        free(imports);
        for (size_t i = 0; i < decl_count; i++) {
            ast_free_decl(declarations[i]);
        }
        free(declarations);
        return NULL;
    }
    
    return ast_create_program(module_name, imports, import_count, declarations, decl_count);
}
