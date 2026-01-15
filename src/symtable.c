#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/symtable.h"

// Symbol functions
Symbol *symbol_create(const char *name, SymbolKind kind, Type *type, size_t line, size_t column) {
    Symbol *symbol = malloc(sizeof(Symbol));
    symbol->name = strdup(name);
    symbol->kind = kind;
    symbol->type = type;
    symbol->is_const = false;
    symbol->is_initialized = false;
    symbol->is_public = false;
    symbol->is_packed = false;
    symbol->is_extern = false;
    symbol->is_type_alias = false;
    symbol->line = line;
    symbol->column = column;
    symbol->scope_depth = 0;
    symbol->param_count = 0;
    symbol->type_params = NULL;
    symbol->type_param_count = 0;
    symbol->is_variadic = false;
    symbol->enum_value = 0;
    symbol->module_table = NULL;
    symbol->fields = NULL;
    symbol->field_count = 0;
    return symbol;
}

void symbol_free(Symbol *symbol) {
    if (!symbol) return;
    free(symbol->name);
    if (symbol->type_params) {
        for (size_t i = 0; i < symbol->type_param_count; i++) {
            free(symbol->type_params[i]);
        }
        free(symbol->type_params);
    }
    if (symbol->fields) {
        for (size_t i = 0; i < symbol->field_count; i++) {
            free(symbol->fields[i].name);
            type_free(symbol->fields[i].type);
        }
        free(symbol->fields);
    }
    // Note: module_table is owned by the Module/Project, don't free here
    // Note: type is owned by the AST, don't free here
    free(symbol);
}

// Scope functions
static Scope *scope_create(Scope *parent) {
    Scope *scope = malloc(sizeof(Scope));
    scope->parent = parent;
    scope->symbols = NULL;
    scope->symbol_count = 0;
    scope->symbol_capacity = 0;
    return scope;
}

static void scope_free(Scope *scope) {
    if (!scope) return;
    
    for (size_t i = 0; i < scope->symbol_count; i++) {
        symbol_free(scope->symbols[i]);
    }
    free(scope->symbols);
    free(scope);
}

static bool scope_insert(Scope *scope, Symbol *symbol) {
    if (!scope || !symbol) return false;
    
    // Check for duplicate
    for (size_t i = 0; i < scope->symbol_count; i++) {
        if (strcmp(scope->symbols[i]->name, symbol->name) == 0) {
            return false;
        }
    }
    
    if (scope->symbol_count >= scope->symbol_capacity) {
        scope->symbol_capacity = scope->symbol_capacity == 0 ? 8 : scope->symbol_capacity * 2;
        scope->symbols = realloc(scope->symbols, sizeof(Symbol*) * scope->symbol_capacity);
    }
    
    scope->symbols[scope->symbol_count++] = symbol;
    return true;
}

static Symbol *scope_lookup(Scope *scope, const char *name) {
    for (size_t i = 0; i < scope->symbol_count; i++) {
        if (strcmp(scope->symbols[i]->name, name) == 0) {
            return scope->symbols[i];
        }
    }
    return NULL;
}

// Symbol table functions
SymbolTable *symtable_create(void) {
    SymbolTable *table = malloc(sizeof(SymbolTable));
    table->name = NULL;
    table->global_scope = scope_create(NULL);
    table->current_scope = table->global_scope;
    return table;
}

void symtable_free(SymbolTable *table) {
    if (!table) return;
    
    if (table->name) free(table->name);
    
    // Free all scopes (walk up from current to global)
    Scope *scope = table->current_scope;
    while (scope) {
        Scope *parent = scope->parent;
        scope_free(scope);
        scope = parent;
    }
    
    free(table);
}

void symtable_enter_scope(SymbolTable *table) {
    Scope *new_scope = scope_create(table->current_scope);
    table->current_scope = new_scope;
}

void symtable_exit_scope(SymbolTable *table) {
    if (table->current_scope == table->global_scope) {
        fprintf(stderr, "Error: Cannot exit global scope\n");
        return;
    }
    
    Scope *old_scope = table->current_scope;
    table->current_scope = old_scope->parent;
    scope_free(old_scope);
}

bool symtable_insert(SymbolTable *table, Symbol *symbol) {
    return scope_insert(table->current_scope, symbol);
}

Symbol *symtable_lookup(SymbolTable *table, const char *name) {
    // Search from current scope up to global
    Scope *scope = table->current_scope;
    while (scope) {
        Symbol *symbol = scope_lookup(scope, name);
        if (symbol) {
            return symbol;
        }
        scope = scope->parent;
    }
    return NULL;
}

Symbol *symtable_lookup_current(SymbolTable *table, const char *name) {
    return scope_lookup(table->current_scope, name);
}
