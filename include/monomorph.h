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
    
    ASTDecl **instantiated_types; // Structs and Enums
    size_t instantiated_types_count;
    size_t instantiated_types_capacity;
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

// Check if a type is generic
bool is_generic_type(ASTDecl *decl);

// Instantiate a generic type (struct/enum) with concrete types
Type *instantiate_type(MonomorphContext *ctx, Type *type);

// Instantiate generic declarations
ASTDecl *instantiate_generic_struct(MonomorphContext *ctx, ASTDecl *generic_struct, Type **concrete_types, size_t type_count);
ASTDecl *instantiate_generic_enum(MonomorphContext *ctx, ASTDecl *generic_enum, Type **concrete_types, size_t type_count);

// Create a monomorphized symbol for a generic type (for use during semantic analysis)
// This is separate from AST-level instantiation and creates Symbol* for symbol table
struct Symbol;  // Forward declaration
struct Symbol *monomorph_create_type_symbol(
    const char *mangled_name,
    struct Symbol *generic_symbol,
    Type **type_args,
    size_t type_arg_count,
    TypeKind kind
);

#endif // MONOMORPH_H
