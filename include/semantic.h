#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include "ast.h"
#include "symtable.h"

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
} SemanticAnalyzer;

// Semantic analysis functions
SemanticAnalyzer *semantic_create(void);
void semantic_free(SemanticAnalyzer *sa);
bool semantic_analyze(SemanticAnalyzer *sa, ASTProgram *program);
bool semantic_analyze_declarations(SemanticAnalyzer *sa, ASTProgram *program);
bool semantic_analyze_bodies(SemanticAnalyzer *sa, ASTProgram *program);

#endif // SEMANTIC_H
