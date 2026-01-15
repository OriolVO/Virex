#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include "ast.h"
#include "symtable.h"

// Generic type instantiation tracking
typedef struct {
    char *base_name;           // e.g., "Pair"
    Type **type_args;          // e.g., [i32, i64]
    size_t type_arg_count;
    char *mangled_name;        // e.g., "Pair_i32_i64"
    Symbol *original_symbol;   // Points to generic struct/enum symbol
    Symbol *monomorphized_symbol; // The specialized symbol
} GenericInstantiation;

typedef struct {
    GenericInstantiation *instantiations;
    size_t count;
    size_t capacity;
} InstantiationRegistry;

// Semantic analyzer
typedef struct {
    SymbolTable *symtable;
    Type *current_function_return_type;
    bool in_unsafe_block;
    int loop_depth;
    int scope_depth;
    bool had_error;
    bool strict_unsafe_mode;
    bool current_block_has_unsafe_op;
    const char *current_filename;
    InstantiationRegistry *instantiation_registry;
} SemanticAnalyzer;

// Semantic analysis functions
SemanticAnalyzer *semantic_create(void);
void semantic_free(SemanticAnalyzer *sa);
bool semantic_analyze(SemanticAnalyzer *sa, ASTProgram *program);
bool semantic_analyze_declarations(SemanticAnalyzer *sa, ASTProgram *program);
bool semantic_analyze_bodies(SemanticAnalyzer *sa, ASTProgram *program);

#endif // SEMANTIC_H
