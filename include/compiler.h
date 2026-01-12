#ifndef COMPILER_H
#define COMPILER_H

#include <stddef.h>
#include <stdbool.h>
#include "ast.h"
#include "symtable.h"

// Represents a single Virex source file/module
typedef struct {
    char *path;
    char *name;             // Module name (from filename or module decl)
    ASTProgram *ast;
    SymbolTable *symtable;  // This module's symbol table
    bool is_analyzed;
    bool is_loading;        // Used for circular dependency detection
} Module;

// Represents a project consisting of multiple modules
typedef struct {
    Module **modules;
    size_t module_count;
    Module *main_module;
} Project;

Project *project_create(void);
void project_free(Project *project);

// High-level compilation steps
Module *project_load_module(Project *project, const char *path, const char *relative_to);
bool project_analyze(Project *project);
void project_generate_code(Project *project, const char *output_file);

#endif // COMPILER_H
