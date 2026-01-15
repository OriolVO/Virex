#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include "type.h"

// Symbol kinds
typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_TYPE,
    SYMBOL_MODULE,
    SYMBOL_CONSTANT,
} SymbolKind;

// Forward declaration of SymbolTable to use in Symbol
typedef struct SymbolTable SymbolTable;

typedef struct {
    char *name;
    Type *type;
} StructField;

// Symbol structure
typedef struct Symbol {
    char *name;
    SymbolKind kind;
    Type *type;
    bool is_const;
    bool is_initialized;
    bool is_public;
    bool is_packed;
    bool is_extern;             // Whether it's an extern declaration
    bool is_type_alias;         // Whether it's a type alias (type Name = Target)
    size_t line;
    size_t column;
    int scope_depth;            // 0 = global, >0 = local depth
    
    // For functions
    size_t param_count;
    
    // For generic functions
    char **type_params;
    size_t type_param_count;
    bool is_variadic;  // For C FFI functions

    // For constants/enums
    long enum_value;
    char **variants;    // Array of variant names
    size_t variant_count;
    
    // For modules
    SymbolTable *module_table;

    // For structs
    StructField *fields;
    size_t field_count;
} Symbol;

// Scope structure
typedef struct Scope {
    struct Scope *parent;
    Symbol **symbols;
    size_t symbol_count;
    size_t symbol_capacity;
} Scope;

// Symbol table
typedef struct SymbolTable {
    char *name;                 // Name of the table (e.g., module name)
    Scope *current_scope;
    Scope *global_scope;
} SymbolTable;

// Symbol table functions
SymbolTable *symtable_create(void);
void symtable_free(SymbolTable *table);

void symtable_enter_scope(SymbolTable *table);
void symtable_exit_scope(SymbolTable *table);

bool symtable_insert(SymbolTable *table, Symbol *symbol);
Symbol *symtable_lookup(SymbolTable *table, const char *name);
Symbol *symtable_lookup_current(SymbolTable *table, const char *name);

// Symbol functions
Symbol *symbol_create(const char *name, SymbolKind kind, Type *type, size_t line, size_t column);
void symbol_free(Symbol *symbol);

#endif // SYMTABLE_H
