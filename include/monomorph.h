#ifndef MONOMORPH_H
#define MONOMORPH_H

#include "ast.h"
#include <stdbool.h>

// Monomorphization context
typedef struct {
    ASTProgram *program;
    ASTDecl **instantiated_functions;
    size_t instantiated_count;
    size_t instantiated_capacity;
} MonomorphContext;

// Create monomorphization context
MonomorphContext *monomorph_create(ASTProgram *program);

// Free monomorphization context
void monomorph_free(MonomorphContext *ctx);

// Perform monomorphization on the program
// Returns new program with instantiated generic functions
ASTProgram *monomorph_program(MonomorphContext *ctx);

// Check if a function is generic
bool is_generic_function(ASTDecl *decl);

// Instantiate a generic function with concrete types
ASTDecl *instantiate_generic_function(MonomorphContext *ctx, ASTDecl *generic_func, Type **concrete_types, size_t type_count);

#endif // MONOMORPH_H
