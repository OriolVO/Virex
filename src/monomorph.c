#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/monomorph.h"

MonomorphContext *monomorph_create(ASTProgram *program) {
    MonomorphContext *ctx = malloc(sizeof(MonomorphContext));
    ctx->program = program;
    ctx->instantiated_functions = malloc(sizeof(ASTDecl*) * 8);
    ctx->instantiated_count = 0;
    ctx->instantiated_capacity = 8;
    
    ctx->instantiated_types = malloc(sizeof(ASTDecl*) * 8);
    ctx->instantiated_types_count = 0;
    ctx->instantiated_types_capacity = 8;
    
    return ctx;
}

void monomorph_free(MonomorphContext *ctx) {
    if (!ctx) return;
    // Note: Don't free instantiated_functions as they're now part of the program
    free(ctx->instantiated_functions);
    free(ctx->instantiated_types);
    free(ctx);
}

bool is_generic_function(ASTDecl *decl) {
    if (!decl || decl->type != AST_FUNCTION_DECL) return false;
    return decl->data.function.type_param_count > 0;
}

bool is_generic_type(ASTDecl *decl) {
    if (!decl) return false;
    if (decl->type == AST_STRUCT_DECL) return decl->data.struct_decl.type_param_count > 0;
    if (decl->type == AST_ENUM_DECL) return decl->data.enum_decl.type_param_count > 0;
    return false;
}

// Helper: Create mangled name for instantiated function
static char *create_mangled_name(const char *base_name, Type **concrete_types, size_t type_count) {
    // Simple mangling: func_name$T1$T2
    size_t len = strlen(base_name) + 1;
    for (size_t i = 0; i < type_count; i++) {
        char *type_str = type_to_string(concrete_types[i]);
        len += strlen(type_str) + 1; // +1 for '$'
        free(type_str);
    }
    
    char *mangled = malloc(len + 1);
    strcpy(mangled, base_name);
    
    for (size_t i = 0; i < type_count; i++) {
        strcat(mangled, "$");
        char *type_str = type_to_string(concrete_types[i]);
        strcat(mangled, type_str);
        free(type_str);
    }
    
    return mangled;
}

// Helper: Substitute type parameters in a type
static Type *substitute_type(Type *original, char **type_params, size_t param_count, Type **concrete_types) {
    if (!original) return NULL;
    
    // Check if this is a type parameter
    if (original->kind == TYPE_STRUCT || original->kind == TYPE_ENUM) {
        for (size_t i = 0; i < param_count; i++) {
            if (strcmp(original->data.struct_enum.name, type_params[i]) == 0) {
                // This is a type parameter - substitute it
                return concrete_types[i];
            }
        }
    }
    
    // Handle pointer types
    if (original->kind == TYPE_POINTER) {
        Type *substituted_base = substitute_type(original->data.pointer.base, type_params, param_count, concrete_types);
        return type_create_pointer(substituted_base, original->data.pointer.non_null);
    }
    
    // Handle array types
    if (original->kind == TYPE_ARRAY) {
        Type *substituted_elem = substitute_type(original->data.array.element, type_params, param_count, concrete_types);
        return type_create_array(substituted_elem, original->data.array.size);
    }
    
    // For other types, return as-is
    return original;
}

// Helper: Clone and substitute types in parameters
static ASTParam *clone_and_substitute_params(ASTParam *params, size_t param_count, char **type_params, size_t type_param_count, Type **concrete_types) {
    if (!params || param_count == 0) return NULL;
    
    ASTParam *new_params = malloc(sizeof(ASTParam) * param_count);
    for (size_t i = 0; i < param_count; i++) {
        new_params[i].name = strdup(params[i].name);
        new_params[i].param_type = substitute_type(params[i].param_type, type_params, type_param_count, concrete_types);
    }
    
    return new_params;
}

// Note: Full AST cloning with type substitution would be complex
// For v0.1, we'll mark functions as instantiated but keep original AST
// Full implementation would recursively clone and substitute the entire function body

ASTDecl *instantiate_generic_function(MonomorphContext *ctx, ASTDecl *generic_func, Type **concrete_types, size_t type_count) {
    if (!is_generic_function(generic_func)) return generic_func;
    if (type_count != generic_func->data.function.type_param_count) return NULL;
    
    // Create mangled name
    char *mangled_name = create_mangled_name(
        generic_func->data.function.name,
        concrete_types,
        type_count
    );
    
    // Check if already instantiated
    for (size_t i = 0; i < ctx->instantiated_count; i++) {
        if (strcmp(ctx->instantiated_functions[i]->data.function.name, mangled_name) == 0) {
            free(mangled_name);
            return ctx->instantiated_functions[i];
        }
    }
    
    // Create new instantiated function
    ASTParam *new_params = clone_and_substitute_params(
        generic_func->data.function.params,
        generic_func->data.function.param_count,
        generic_func->data.function.type_params,
        generic_func->data.function.type_param_count,
        concrete_types
    );
    
    Type *new_return_type = substitute_type(
        generic_func->data.function.return_type,
        generic_func->data.function.type_params,
        generic_func->data.function.type_param_count,
        concrete_types
    );
    
    // Create instantiated function (non-generic)
    ASTDecl *instantiated = ast_create_function(
        mangled_name,
        NULL,  // No type parameters - this is a concrete function
        0,
        new_params,
        generic_func->data.function.param_count,
        new_return_type,
        generic_func->data.function.body,  // Reuse body for now
        generic_func->data.function.is_public,
        false,  // Not extern
        false,  // Monomorphized functions are not variadic
        generic_func->line,
        generic_func->column
    );
    
    // Add to instantiated list
    if (ctx->instantiated_count >= ctx->instantiated_capacity) {
        ctx->instantiated_capacity = ctx->instantiated_capacity == 0 ? 4 : ctx->instantiated_capacity * 2;
        ctx->instantiated_functions = realloc(ctx->instantiated_functions, 
                                              sizeof(ASTDecl*) * ctx->instantiated_capacity);
    }
    ctx->instantiated_functions[ctx->instantiated_count++] = instantiated;
    
    free(mangled_name);
    return instantiated;
}

ASTProgram *monomorph_program(MonomorphContext *ctx) {
    if (!ctx || !ctx->program) return ctx->program;
    
    // For v0.1: Return original program
    // Full implementation would:
    // 1. Scan for generic function calls
    // 2. Instantiate needed versions
    // 3. Replace calls with instantiated versions
    // 4. Add instantiated functions to program
    
    // Add instantiated functions to program
    if (ctx->instantiated_count > 0) {
        size_t new_count = ctx->program->decl_count + ctx->instantiated_count;
        ASTDecl **new_decls = malloc(sizeof(ASTDecl*) * new_count);
        
        // Copy original declarations
        for (size_t i = 0; i < ctx->program->decl_count; i++) {
            new_decls[i] = ctx->program->declarations[i];
        }
        
        // Add instantiated functions
        for (size_t i = 0; i < ctx->instantiated_count; i++) {
            new_decls[ctx->program->decl_count + i] = ctx->instantiated_functions[i];
        }
        
        free(ctx->program->declarations);
        ctx->program->declarations = new_decls;
        ctx->program->decl_count = new_count;
    }
    
    return ctx->program;
}
