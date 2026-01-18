#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/monomorph.h"
#include "../include/util.h"
#include "../include/type.h"
#include "../include/symtable.h"

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




// Note: Full AST cloning with type substitution would be complex
// For v0.1, we'll mark functions as instantiated but keep original AST
// Full implementation would recursively clone and substitute the entire function body

// Helper: Clone and substitute types in parameters
static ASTParam *clone_and_substitute_params(ASTParam *params, size_t param_count, char **type_params, size_t type_param_count, Type **concrete_types) {
    if (!params || param_count == 0) return NULL;
    
    ASTParam *new_params = malloc(sizeof(ASTParam) * param_count);
    for (size_t i = 0; i < param_count; i++) {
        new_params[i].name = strdup(params[i].name);
        new_params[i].param_type = type_substitute(params[i].param_type, type_params, concrete_types, type_param_count);
    }
    
    return new_params;
}

ASTDecl *instantiate_generic_function(MonomorphContext *ctx, ASTDecl *generic_func, Type **concrete_types, size_t type_count) {
    if (!is_generic_function(generic_func)) return generic_func;
    if (type_count != generic_func->data.function.type_param_count) return NULL;
    
    // Create mangled name
    char *mangled_name = util_mangle_instantiation(
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
    
    Type *new_return_type = type_substitute(
        generic_func->data.function.return_type,
        generic_func->data.function.type_params,
        concrete_types,
        generic_func->data.function.type_param_count
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
        generic_func->data.function.is_unsafe,  // Preserve unsafe status
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

// Helper: Clone and substitute types in struct fields
static ASTField *clone_and_substitute_fields(ASTField *fields, size_t field_count, char **type_params, size_t type_param_count, Type **concrete_types) {
    if (!fields || field_count == 0) return NULL;
    ASTField *new_fields = malloc(sizeof(ASTField) * field_count);
    for (size_t i = 0; i < field_count; i++) {
        new_fields[i].name = strdup(fields[i].name);
        new_fields[i].field_type = type_substitute(fields[i].field_type, type_params, concrete_types, type_param_count);
    }
    return new_fields;
}

// Helper: Clone and substitute types in enum variants
static ASTEnumVariant *clone_and_substitute_variants(ASTEnumVariant *variants, size_t variant_count, char **type_params, size_t type_param_count, Type **concrete_types) {
    if (!variants || variant_count == 0) return NULL;
    ASTEnumVariant *new_variants = malloc(sizeof(ASTEnumVariant) * variant_count);
    for (size_t i = 0; i < variant_count; i++) {
        new_variants[i].name = strdup(variants[i].name);
        // Enums variants don't have associated data types yet in Virex
    }
    return new_variants;
}

ASTDecl *instantiate_generic_struct(MonomorphContext *ctx, ASTDecl *generic_struct, Type **concrete_types, size_t type_count) {
    if (!is_generic_type(generic_struct) || generic_struct->type != AST_STRUCT_DECL) return generic_struct;
    if (type_count != generic_struct->data.struct_decl.type_param_count) return NULL;
    
    char *mangled_name = util_mangle_instantiation(generic_struct->data.struct_decl.name, concrete_types, type_count);
    
    // Check if already instantiated
    for (size_t i = 0; i < ctx->instantiated_types_count; i++) {
        ASTDecl *inst = ctx->instantiated_types[i];
        if (inst->type == AST_STRUCT_DECL && strcmp(inst->data.struct_decl.name, mangled_name) == 0) {
            free(mangled_name);
            return inst;
        }
    }
    
    ASTField *new_fields = clone_and_substitute_fields(
        generic_struct->data.struct_decl.fields,
        generic_struct->data.struct_decl.field_count,
        generic_struct->data.struct_decl.type_params,
        generic_struct->data.struct_decl.type_param_count,
        concrete_types
    );
    
    ASTDecl *instantiated = ast_create_struct(
        mangled_name,
        NULL, 0, // Concrete
        new_fields,
        generic_struct->data.struct_decl.field_count,
        generic_struct->data.struct_decl.is_public,
        generic_struct->data.struct_decl.is_packed,
        generic_struct->line,
        generic_struct->column
    );
    
    // Add to instantiated list
    if (ctx->instantiated_types_count >= ctx->instantiated_types_capacity) {
        ctx->instantiated_types_capacity = ctx->instantiated_types_capacity == 0 ? 4 : ctx->instantiated_types_capacity * 2;
        ctx->instantiated_types = realloc(ctx->instantiated_types, sizeof(ASTDecl*) * ctx->instantiated_types_capacity);
    }
    ctx->instantiated_types[ctx->instantiated_types_count++] = instantiated;
    
    free(mangled_name);
    return instantiated;
}

ASTDecl *instantiate_generic_enum(MonomorphContext *ctx, ASTDecl *generic_enum, Type **concrete_types, size_t type_count) {
    if (!is_generic_type(generic_enum) || generic_enum->type != AST_ENUM_DECL) return generic_enum;
    if (type_count != generic_enum->data.enum_decl.type_param_count) return NULL;
    
    char *mangled_name = util_mangle_instantiation(generic_enum->data.enum_decl.name, concrete_types, type_count);
    
    // Check if already instantiated
    for (size_t i = 0; i < ctx->instantiated_types_count; i++) {
        ASTDecl *inst = ctx->instantiated_types[i];
        if (inst->type == AST_ENUM_DECL && strcmp(inst->data.enum_decl.name, mangled_name) == 0) {
            free(mangled_name);
            return inst;
        }
    }
    
    ASTEnumVariant *new_variants = clone_and_substitute_variants(
        generic_enum->data.enum_decl.variants,
        generic_enum->data.enum_decl.variant_count,
        generic_enum->data.enum_decl.type_params,
        generic_enum->data.enum_decl.type_param_count,
        concrete_types
    );
    
    ASTDecl *instantiated = ast_create_enum(
        mangled_name,
        NULL, 0, // Concrete
        new_variants,
        generic_enum->data.enum_decl.variant_count,
        generic_enum->data.enum_decl.is_public,
        generic_enum->line,
        generic_enum->column
    );
    
    // Add to instantiated list
    if (ctx->instantiated_types_count >= ctx->instantiated_types_capacity) {
        ctx->instantiated_types_capacity = ctx->instantiated_types_capacity == 0 ? 4 : ctx->instantiated_types_capacity * 2;
        ctx->instantiated_types = realloc(ctx->instantiated_types, sizeof(ASTDecl*) * ctx->instantiated_types_capacity);
    }
    ctx->instantiated_types[ctx->instantiated_types_count++] = instantiated;
    
    free(mangled_name);
    return instantiated;
}

ASTProgram *monomorph_program(MonomorphContext *ctx) {
    if (!ctx || !ctx->program) return ctx->program;
    
    // Add instantiated functions and types to program
    size_t total_added = ctx->instantiated_count + ctx->instantiated_types_count;
    if (total_added > 0) {
        size_t new_count = ctx->program->decl_count + total_added;
        ASTDecl **new_decls = malloc(sizeof(ASTDecl*) * new_count);
        
        // Copy original declarations
        for (size_t i = 0; i < ctx->program->decl_count; i++) {
            new_decls[i] = ctx->program->declarations[i];
        }
        
        // Add instantiated functions
        for (size_t i = 0; i < ctx->instantiated_count; i++) {
            new_decls[ctx->program->decl_count + i] = ctx->instantiated_functions[i];
        }
        
        // Add instantiated types
        for (size_t i = 0; i < ctx->instantiated_types_count; i++) {
            new_decls[ctx->program->decl_count + ctx->instantiated_count + i] = ctx->instantiated_types[i];
        }
        
        free(ctx->program->declarations);
        ctx->program->declarations = new_decls;
        ctx->program->decl_count = new_count;
    }
    
    return ctx->program;
}

// Create a monomorphized symbol for a generic type
// Extracted from semantic.c to decouple monomorphization from type resolution
Symbol *monomorph_create_type_symbol(
    const char *mangled_name,
    Symbol *generic_symbol,
    Type **type_args,
    size_t type_arg_count,
    TypeKind kind
) {
    if (!mangled_name || !generic_symbol) return NULL;
    
    if (kind == TYPE_STRUCT) {
        // Create monomorphized struct symbol
        Symbol *mono_sym = symbol_create(mangled_name, SYMBOL_TYPE,
                                        type_create_struct(mangled_name, NULL, 0),
                                        generic_symbol->line, generic_symbol->column);
        mono_sym->is_public = generic_symbol->is_public;
        mono_sym->is_packed = generic_symbol->is_packed;
        mono_sym->field_count = generic_symbol->field_count;
        mono_sym->fields = malloc(sizeof(StructField) * mono_sym->field_count);
        
        // Substitute type parameters in fields
        for (size_t i = 0; i < generic_symbol->field_count; i++) {
            mono_sym->fields[i].name = strdup(generic_symbol->fields[i].name);
            mono_sym->fields[i].type = type_substitute(generic_symbol->fields[i].type,
                                                      generic_symbol->type_params,
                                                      type_args,
                                                      type_arg_count);
        }
        
        return mono_sym;
    } else if (kind == TYPE_ENUM) {
        // Create monomorphized enum symbol
        Symbol *mono_sym = symbol_create(mangled_name, SYMBOL_TYPE,
                                        type_create_enum(mangled_name, NULL, 0),
                                        generic_symbol->line, generic_symbol->column);
        mono_sym->is_public = generic_symbol->is_public;
        mono_sym->variant_count = generic_symbol->variant_count;
        mono_sym->variants = malloc(sizeof(char*) * mono_sym->variant_count);
        
        // Copy variants (enums don't have associated data yet)
        for (size_t i = 0; i < generic_symbol->variant_count; i++) {
            mono_sym->variants[i] = strdup(generic_symbol->variants[i]);
        }
        
        return mono_sym;
    }
    
    return NULL;
}

