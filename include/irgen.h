#ifndef IRGEN_H
#define IRGEN_H

#include "ir.h"
#include "ast.h"

typedef struct IRGenerator IRGenerator;

// Create and destroy IR generator
IRGenerator *irgen_create(void);
void irgen_free(IRGenerator *gen);

#include "symtable.h"

#include <stdbool.h>
// Generate IR from AST
IRModule *irgen_generate(IRGenerator *gen, ASTProgram *program, const char *module_name, SymbolTable *symtable, bool is_main);

#endif // IRGEN_H
