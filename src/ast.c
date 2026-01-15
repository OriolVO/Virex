#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ast.h"

// Expression creation functions

ASTExpr *ast_create_literal(Token *token) {
    ASTExpr *expr = malloc(sizeof(ASTExpr));
    expr->type = AST_LITERAL_EXPR;
    expr->line = token->line;
    expr->column = token->column;
    expr->expr_type = NULL;
    expr->data.literal.token = token;
    
    switch (token->type) {
        case TOKEN_INTEGER:
            expr->data.literal.value.int_value = token->value.int_value;
            break;
        case TOKEN_FLOAT:
            expr->data.literal.value.float_value = token->value.float_value;
            break;
        case TOKEN_STRING:
            expr->data.literal.value.string_value = strdup(token->lexeme);
            break;
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            expr->data.literal.value.bool_value = token->value.bool_value;
            break;
        case TOKEN_NULL:
            break;
        default:
            break;
    }
    
    return expr;
}

ASTExpr *ast_create_variable(const char *name, size_t line, size_t column) {
    ASTExpr *expr = malloc(sizeof(ASTExpr));
    expr->type = AST_VARIABLE_EXPR;
    expr->line = line;
    expr->column = column;
    expr->expr_type = NULL;
    expr->data.variable.name = strdup(name);
    return expr;
}

ASTExpr *ast_create_binary(TokenType op, ASTExpr *left, ASTExpr *right, size_t line, size_t column) {
    ASTExpr *expr = malloc(sizeof(ASTExpr));
    expr->type = AST_BINARY_EXPR;
    expr->line = line;
    expr->column = column;
    expr->expr_type = NULL;
    expr->data.binary.op = op;
    expr->data.binary.left = left;
    expr->data.binary.right = right;
    return expr;
}

ASTExpr *ast_create_unary(TokenType op, ASTExpr *operand, size_t line, size_t column) {
    ASTExpr *expr = malloc(sizeof(ASTExpr));
    expr->type = AST_UNARY_EXPR;
    expr->line = line;
    expr->column = column;
    expr->expr_type = NULL;
    expr->data.unary.op = op;
    expr->data.unary.operand = operand;
    return expr;
}

ASTExpr *ast_create_call(ASTExpr *callee, ASTExpr **args, size_t arg_count, Type **generic_args, size_t generic_count, size_t line, size_t column) {
    ASTExpr *expr = malloc(sizeof(ASTExpr));
    expr->type = AST_CALL_EXPR;
    expr->line = line;
    expr->column = column;
    expr->expr_type = NULL;
    expr->data.call.callee = callee;
    expr->data.call.arguments = args;
    expr->data.call.arg_count = arg_count;
    expr->data.call.generic_args = generic_args;
    expr->data.call.generic_count = generic_count;
    return expr;
}

ASTExpr *ast_create_index(ASTExpr *array, ASTExpr *index, size_t line, size_t column) {
    ASTExpr *expr = malloc(sizeof(ASTExpr));
    expr->type = AST_INDEX_EXPR;
    expr->line = line;
    expr->column = column;
    expr->expr_type = NULL;
    expr->data.index.array = array;
    expr->data.index.index = index;
    return expr;
}

ASTExpr *ast_create_slice_expr(ASTExpr *array, ASTExpr *start, ASTExpr *end, size_t line, size_t column) {
    ASTExpr *expr = malloc(sizeof(ASTExpr));
    expr->type = AST_SLICE_EXPR;
    expr->line = line;
    expr->column = column;
    expr->expr_type = NULL;
    expr->data.slice.array = array;
    expr->data.slice.start = start;
    expr->data.slice.end = end;
    return expr;
}

ASTExpr *ast_create_member(ASTExpr *object, const char *member, bool is_arrow, size_t line, size_t column) {
    ASTExpr *expr = malloc(sizeof(ASTExpr));
    expr->type = AST_MEMBER_EXPR;
    expr->line = line;
    expr->column = column;
    expr->expr_type = NULL;
    expr->data.member.object = object;
    expr->data.member.member = strdup(member);
    expr->data.member.is_arrow = is_arrow;
    return expr;
}

// Statement creation functions

ASTStmt *ast_create_expr_stmt(ASTExpr *expr, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_EXPR_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.expr_stmt.expr = expr;
    return stmt;
}

ASTStmt *ast_create_var_decl(bool is_const, Type *type, const char *name, ASTExpr *init, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_VAR_DECL_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.var_decl.is_const = is_const;
    stmt->data.var_decl.var_type = type;
    stmt->data.var_decl.name = strdup(name);
    stmt->data.var_decl.initializer = init;
    return stmt;
}

ASTStmt *ast_create_if(ASTExpr *cond, ASTStmt *then_branch, ASTStmt *else_branch, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_IF_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.if_stmt.condition = cond;
    stmt->data.if_stmt.then_branch = then_branch;
    stmt->data.if_stmt.else_branch = else_branch;
    return stmt;
}

ASTStmt *ast_create_while(ASTExpr *cond, ASTStmt *body, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_WHILE_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.while_stmt.condition = cond;
    stmt->data.while_stmt.body = body;
    return stmt;
}

ASTStmt *ast_create_for(ASTStmt *init, ASTExpr *cond, ASTExpr *inc, ASTStmt *body, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_FOR_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.for_stmt.initializer = init;
    stmt->data.for_stmt.condition = cond;
    stmt->data.for_stmt.increment = inc;
    stmt->data.for_stmt.body = body;
    return stmt;
}

ASTStmt *ast_create_return(ASTExpr *value, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_RETURN_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.return_stmt.value = value;
    return stmt;
}

ASTStmt *ast_create_block(ASTStmt **stmts, size_t count, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_BLOCK_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.block.statements = stmts;
    stmt->data.block.stmt_count = count;
    return stmt;
}

ASTStmt *ast_create_match(ASTExpr *expr, ASTMatchCase *cases, size_t case_count, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_MATCH_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.match_stmt.expr = expr;
    stmt->data.match_stmt.cases = cases;
    stmt->data.match_stmt.case_count = case_count;
    return stmt;
}

ASTStmt *ast_create_fail(ASTExpr *message, size_t line, size_t column) {
    ASTStmt *stmt = malloc(sizeof(ASTStmt));
    stmt->type = AST_FAIL_STMT;
    stmt->line = line;
    stmt->column = column;
    stmt->data.fail_stmt.message = message;
    return stmt;
}

ASTStmt *ast_create_unsafe(ASTStmt *body, size_t line, size_t column) {
    ASTStmt *stmt = calloc(1, sizeof(ASTStmt));
    stmt->type = AST_UNSAFE_STMT;
    stmt->data.unsafe_stmt.body = body;
    stmt->line = line;
    stmt->column = column;
    return stmt;
}

ASTStmt *ast_create_break(size_t line, size_t column) {
    ASTStmt *stmt = calloc(1, sizeof(ASTStmt));
    stmt->type = AST_BREAK_STMT;
    stmt->line = line;
    stmt->column = column;
    return stmt;
}

ASTStmt *ast_create_continue(size_t line, size_t column) {
    ASTStmt *stmt = calloc(1, sizeof(ASTStmt));
    stmt->type = AST_CONTINUE_STMT;
    stmt->line = line;
    stmt->column = column;
    return stmt;
}

// Declaration creation functions

ASTDecl *ast_create_function(const char *name, char **type_params, size_t type_param_count, ASTParam *params, size_t param_count, Type *ret_type, ASTStmt *body, bool is_public, bool is_extern, bool is_variadic, size_t line, size_t column) {
    ASTDecl *decl = malloc(sizeof(ASTDecl));
    decl->type = AST_FUNCTION_DECL;
    decl->line = line;
    decl->column = column;
    decl->data.function.name = strdup(name);
    decl->data.function.type_params = type_params;
    decl->data.function.type_param_count = type_param_count;
    decl->data.function.params = params;
    decl->data.function.param_count = param_count;
    decl->data.function.return_type = ret_type;
    decl->data.function.body = body;
    decl->data.function.is_public = is_public;
    decl->data.function.is_extern = is_extern;
    decl->data.function.is_variadic = is_variadic;
    return decl;
}

ASTDecl *ast_create_struct(const char *name, char **type_params, size_t type_param_count, ASTField *fields, size_t field_count, bool is_public, bool is_packed, size_t line, size_t column) {
    ASTDecl *decl = malloc(sizeof(ASTDecl));
    decl->type = AST_STRUCT_DECL;
    decl->line = line;
    decl->column = column;
    decl->data.struct_decl.name = strdup(name);
    decl->data.struct_decl.type_params = type_params;
    decl->data.struct_decl.type_param_count = type_param_count;
    decl->data.struct_decl.fields = fields;
    decl->data.struct_decl.field_count = field_count;
    decl->data.struct_decl.is_public = is_public;
    decl->data.struct_decl.is_packed = is_packed;
    return decl;
}

ASTDecl *ast_create_enum(const char *name, char **type_params, size_t type_param_count, ASTEnumVariant *variants, size_t variant_count, bool is_public, size_t line, size_t column) {
    ASTDecl *decl = malloc(sizeof(ASTDecl));
    decl->type = AST_ENUM_DECL;
    decl->line = line;
    decl->column = column;
    decl->data.enum_decl.name = strdup(name);
    decl->data.enum_decl.type_params = type_params;
    decl->data.enum_decl.type_param_count = type_param_count;
    decl->data.enum_decl.variants = variants;
    decl->data.enum_decl.variant_count = variant_count;
    decl->data.enum_decl.is_public = is_public;
    return decl;
}

ASTDecl *ast_create_module(const char *module_name, size_t line, size_t column) {
    ASTDecl *decl = malloc(sizeof(ASTDecl));
    decl->type = AST_MODULE_DECL;
    decl->line = line;
    decl->column = column;
    decl->data.module_decl.module_name = strdup(module_name);
    return decl;
}

ASTDecl *ast_create_import(const char *import_path, const char *alias, size_t line, size_t column) {
    ASTDecl *decl = malloc(sizeof(ASTDecl));
    decl->type = AST_IMPORT_DECL;
    decl->line = line;
    decl->column = column;
    decl->data.import_decl.import_path = strdup(import_path);
    decl->data.import_decl.alias = alias ? strdup(alias) : NULL;
    return decl;
}

ASTDecl *ast_create_variable_decl(const char *name, Type *type, ASTExpr *initializer, bool is_const, bool is_public, size_t line, size_t column) {
    ASTDecl *decl = malloc(sizeof(ASTDecl));
    decl->type = AST_VAR_DECL_STMT;
    decl->line = line;
    decl->column = column;
    decl->data.var_decl.name = strdup(name);
    decl->data.var_decl.var_type = type;
    decl->data.var_decl.initializer = initializer;
    decl->data.var_decl.is_const = is_const;
    decl->data.var_decl.is_public = is_public;
    return decl;
}

ASTDecl *ast_create_type_alias(const char *name, Type *target_type, bool is_public, size_t line, size_t column) {
    ASTDecl *decl = malloc(sizeof(ASTDecl));
    decl->type = AST_TYPE_ALIAS_DECL;
    decl->line = line;
    decl->column = column;
    decl->data.type_alias.name = strdup(name);
    decl->data.type_alias.target_type = target_type;
    decl->data.type_alias.is_public = is_public;
    return decl;
}

ASTProgram *ast_create_program(const char *module_name, ASTImportDecl **imports, size_t import_count, ASTDecl **decls, size_t count) {
    ASTProgram *program = malloc(sizeof(ASTProgram));
    program->module_name = module_name ? strdup(module_name) : NULL;
    program->imports = imports;
    program->import_count = import_count;
    program->declarations = decls;
    program->decl_count = count;
    return program;
}

// Destruction functions

void ast_free_expr(ASTExpr *expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case AST_LITERAL_EXPR:
            // string_value in literal data is just a copy of the token's value or vice versa.
            // Actually, ast_create_literal strdups it, so we must free it.
            // But wait, the token_free also frees it if it's the same token.
            // To be safe and clean, we handle string literal memory in token_free.
            if (expr->data.literal.token->type == TOKEN_STRING) {
                // If the AST node has its own strdup, free it.
                // However, let's check if it's shared.
                free(expr->data.literal.value.string_value);
            }
            token_free(expr->data.literal.token);
            break;
        case AST_VARIABLE_EXPR:
            free(expr->data.variable.name);
            break;
        case AST_BINARY_EXPR:
            ast_free_expr(expr->data.binary.left);
            ast_free_expr(expr->data.binary.right);
            break;
        case AST_UNARY_EXPR:
            ast_free_expr(expr->data.unary.operand);
            break;
        case AST_CALL_EXPR:
            ast_free_expr(expr->data.call.callee);
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                ast_free_expr(expr->data.call.arguments[i]);
            }
            free(expr->data.call.arguments);
            for (size_t i = 0; i < expr->data.call.generic_count; i++) {
                type_free(expr->data.call.generic_args[i]);
            }
            free(expr->data.call.generic_args);
            break;
        case AST_INDEX_EXPR:
            ast_free_expr(expr->data.index.array);
            ast_free_expr(expr->data.index.index);
            break;
        case AST_SLICE_EXPR:
            ast_free_expr(expr->data.slice.array);
            ast_free_expr(expr->data.slice.start);
            ast_free_expr(expr->data.slice.end);
            break;
        case AST_MEMBER_EXPR:
            ast_free_expr(expr->data.member.object);
            free(expr->data.member.member);
            break;
        default:
            break;
    }
    
    if (expr->expr_type) {
        type_free(expr->expr_type);
    }
    
    free(expr);
}

void ast_free_stmt(ASTStmt *stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case AST_EXPR_STMT:
            ast_free_expr(stmt->data.expr_stmt.expr);
            break;
        case AST_VAR_DECL_STMT:
            type_free(stmt->data.var_decl.var_type);
            free(stmt->data.var_decl.name);
            ast_free_expr(stmt->data.var_decl.initializer);
            break;
        case AST_IF_STMT:
            ast_free_expr(stmt->data.if_stmt.condition);
            ast_free_stmt(stmt->data.if_stmt.then_branch);
            ast_free_stmt(stmt->data.if_stmt.else_branch);
            break;
        case AST_WHILE_STMT:
            ast_free_expr(stmt->data.while_stmt.condition);
            ast_free_stmt(stmt->data.while_stmt.body);
            break;
        case AST_FOR_STMT:
            ast_free_stmt(stmt->data.for_stmt.initializer);
            ast_free_expr(stmt->data.for_stmt.condition);
            ast_free_expr(stmt->data.for_stmt.increment);
            ast_free_stmt(stmt->data.for_stmt.body);
            break;
        case AST_RETURN_STMT:
            ast_free_expr(stmt->data.return_stmt.value);
            break;
        case AST_BLOCK_STMT:
            for (size_t i = 0; i < stmt->data.block.stmt_count; i++) {
                ast_free_stmt(stmt->data.block.statements[i]);
            }
            free(stmt->data.block.statements);
            break;
        case AST_MATCH_STMT:
            ast_free_expr(stmt->data.match_stmt.expr);
            for (size_t i = 0; i < stmt->data.match_stmt.case_count; i++) {
                free(stmt->data.match_stmt.cases[i].pattern_tag);
                if (stmt->data.match_stmt.cases[i].capture_name) {
                    free(stmt->data.match_stmt.cases[i].capture_name);
                }
                ast_free_stmt(stmt->data.match_stmt.cases[i].body);
            }
            free(stmt->data.match_stmt.cases);
            break;
        case AST_FAIL_STMT:
            if (stmt->data.fail_stmt.message) {
                ast_free_expr(stmt->data.fail_stmt.message);
            }
            break;
        case AST_UNSAFE_STMT:
            ast_free_stmt(stmt->data.unsafe_stmt.body);
            break;
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
            // Nothing to free
            break;
        default:
            break;
    }
    
    free(stmt);
}

void ast_free_decl(ASTDecl *decl) {
    if (!decl) return;
    
    switch (decl->type) {
        case AST_FUNCTION_DECL:
            free(decl->data.function.name);
            if (decl->data.function.type_params) {
                for (size_t i = 0; i < decl->data.function.type_param_count; i++) {
                    free(decl->data.function.type_params[i]);
                }
                free(decl->data.function.type_params);
            }
            for (size_t i = 0; i < decl->data.function.param_count; i++) {
                type_free(decl->data.function.params[i].param_type);
                free(decl->data.function.params[i].name);
            }
            free(decl->data.function.params);
            type_free(decl->data.function.return_type);
            ast_free_stmt(decl->data.function.body);
            break;
        case AST_STRUCT_DECL:
            free(decl->data.struct_decl.name);
            if (decl->data.struct_decl.type_params) {
                for (size_t i = 0; i < decl->data.struct_decl.type_param_count; i++) {
                    free(decl->data.struct_decl.type_params[i]);
                }
                free(decl->data.struct_decl.type_params);
            }
            for (size_t i = 0; i < decl->data.struct_decl.field_count; i++) {
                type_free(decl->data.struct_decl.fields[i].field_type);
                free(decl->data.struct_decl.fields[i].name);
            }
            free(decl->data.struct_decl.fields);
            break;
        case AST_ENUM_DECL:
            free(decl->data.enum_decl.name);
            if (decl->data.enum_decl.type_params) {
                for (size_t i = 0; i < decl->data.enum_decl.type_param_count; i++) {
                    free(decl->data.enum_decl.type_params[i]);
                }
                free(decl->data.enum_decl.type_params);
            }
            for (size_t i = 0; i < decl->data.enum_decl.variant_count; i++) {
                free(decl->data.enum_decl.variants[i].name);
            }
            free(decl->data.enum_decl.variants);
            break;
        case AST_MODULE_DECL:
            free(decl->data.module_decl.module_name);
            break;
        case AST_IMPORT_DECL:
            free(decl->data.import_decl.import_path);
            if (decl->data.import_decl.alias) {
                free(decl->data.import_decl.alias);
            }
            break;
        case AST_TYPE_ALIAS_DECL:
            free(decl->data.type_alias.name);
            type_free(decl->data.type_alias.target_type);
            break;
        default:
            break;
    }
    
    free(decl);
}

void ast_free_program(ASTProgram *program) {
    if (!program) return;
    
    if (program->module_name) {
        free(program->module_name);
    }
    
    for (size_t i = 0; i < program->import_count; i++) {
        if (program->imports[i]) {
            free(program->imports[i]->import_path);
            if (program->imports[i]->alias) {
                free(program->imports[i]->alias);
            }
            free(program->imports[i]);
        }
    }
    free(program->imports);
    
    for (size_t i = 0; i < program->decl_count; i++) {
        ast_free_decl(program->declarations[i]);
    }
    free(program->declarations);
    free(program);
}

// Printing functions (for debugging)

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print_expr(const ASTExpr *expr, int indent) {
    if (!expr) return;
    
    print_indent(indent);
    
    switch (expr->type) {
        case AST_LITERAL_EXPR:
            printf("Literal: %s\n", expr->data.literal.token->lexeme);
            break;
        case AST_VARIABLE_EXPR:
            printf("Variable: %s\n", expr->data.variable.name);
            break;
        case AST_BINARY_EXPR:
            printf("Binary: %s\n", token_type_name(expr->data.binary.op));
            ast_print_expr(expr->data.binary.left, indent + 1);
            ast_print_expr(expr->data.binary.right, indent + 1);
            break;
        case AST_UNARY_EXPR:
            printf("Unary: %s\n", token_type_name(expr->data.unary.op));
            ast_print_expr(expr->data.unary.operand, indent + 1);
            break;
        case AST_CALL_EXPR:
            printf("Call:\n");
            ast_print_expr(expr->data.call.callee, indent + 1);
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                ast_print_expr(expr->data.call.arguments[i], indent + 1);
            }
            break;
        case AST_INDEX_EXPR:
            printf("Index:\n");
            ast_print_expr(expr->data.index.array, indent + 1);
            ast_print_expr(expr->data.index.index, indent + 1);
            break;
        case AST_SLICE_EXPR:
            printf("Slice:\n");
            ast_print_expr(expr->data.slice.array, indent + 1);
            printf("Start:\n");
            ast_print_expr(expr->data.slice.start, indent + 2);
            printf("End:\n");
            ast_print_expr(expr->data.slice.end, indent + 2);
            break;
        case AST_MEMBER_EXPR:
            printf("Member: %s %s\n", expr->data.member.is_arrow ? "->" : ".", expr->data.member.member);
            ast_print_expr(expr->data.member.object, indent + 1);
            break;
        default:
            printf("Unknown expression\n");
    }
}

void ast_print_stmt(const ASTStmt *stmt, int indent) {
    if (!stmt) return;
    
    print_indent(indent);
    
    switch (stmt->type) {
        case AST_EXPR_STMT:
            printf("ExprStmt:\n");
            ast_print_expr(stmt->data.expr_stmt.expr, indent + 1);
            break;
        case AST_VAR_DECL_STMT:
            printf("VarDecl: %s %s\n", stmt->data.var_decl.is_const ? "const" : "var", stmt->data.var_decl.name);
            ast_print_expr(stmt->data.var_decl.initializer, indent + 1);
            break;
        case AST_IF_STMT:
            printf("If:\n");
            print_indent(indent + 1);
            printf("Condition:\n");
            ast_print_expr(stmt->data.if_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Then:\n");
            ast_print_stmt(stmt->data.if_stmt.then_branch, indent + 2);
            if (stmt->data.if_stmt.else_branch) {
                print_indent(indent + 1);
                printf("Else:\n");
                ast_print_stmt(stmt->data.if_stmt.else_branch, indent + 2);
            }
            break;
        case AST_WHILE_STMT:
            printf("While:\n");
            ast_print_expr(stmt->data.while_stmt.condition, indent + 1);
            ast_print_stmt(stmt->data.while_stmt.body, indent + 1);
            break;
        case AST_FOR_STMT:
            printf("For:\n");
            ast_print_stmt(stmt->data.for_stmt.initializer, indent + 1);
            ast_print_expr(stmt->data.for_stmt.condition, indent + 1);
            ast_print_expr(stmt->data.for_stmt.increment, indent + 1);
            ast_print_stmt(stmt->data.for_stmt.body, indent + 1);
            break;
        case AST_RETURN_STMT:
            printf("Return:\n");
            ast_print_expr(stmt->data.return_stmt.value, indent + 1);
            break;
        case AST_BLOCK_STMT:
            printf("Block:\n");
            for (size_t i = 0; i < stmt->data.block.stmt_count; i++) {
                ast_print_stmt(stmt->data.block.statements[i], indent + 1);
            }
            break;
        case AST_UNSAFE_STMT:
            printf("Unsafe:\n");
            ast_print_stmt(stmt->data.unsafe_stmt.body, indent + 1);
            break;
        default:
            printf("Unknown statement\n");
    }
}

void ast_print_decl(const ASTDecl *decl, int indent) {
    if (!decl) return;
    
    print_indent(indent);
    
    switch (decl->type) {
        case AST_FUNCTION_DECL:
            printf("Function: %s\n", decl->data.function.name);
            print_indent(indent + 1);
            printf("Parameters: %zu\n", decl->data.function.param_count);
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print_stmt(decl->data.function.body, indent + 2);
            break;
        case AST_STRUCT_DECL:
            printf("Struct: %s (%zu fields)\n", decl->data.struct_decl.name, decl->data.struct_decl.field_count);
            break;
        case AST_ENUM_DECL:
            printf("Enum: %s (%zu variants)\n", decl->data.enum_decl.name, decl->data.enum_decl.variant_count);
            break;
        case AST_VAR_DECL_STMT:
            printf("Global Var: %s\n", decl->data.var_decl.name);
            break;
        default:
            printf("Unknown declaration\n");
    }
}

void ast_print_program(const ASTProgram *program) {
    if (!program) return;
    
    if (program->module_name) {
        printf("Module: %s\n", program->module_name);
    }
    
    if (program->import_count > 0) {
        printf("Imports (%zu):\n", program->import_count);
        for (size_t i = 0; i < program->import_count; i++) {
            printf("  import \"%s\"", program->imports[i]->import_path);
            if (program->imports[i]->alias) {
                printf(" as %s", program->imports[i]->alias);
            }
            printf("\n");
        }
    }
    
    printf("Program (%zu declarations):\n", program->decl_count);
    for (size_t i = 0; i < program->decl_count; i++) {
        ast_print_decl(program->declarations[i], 1);
    }
}
