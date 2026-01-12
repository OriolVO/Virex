#ifndef CODEGEN_H
#define CODEGEN_H

#include "compiler.h"
#include "ir.h"
#include "ast.h"
#include <stdio.h>

// C Code Generator
typedef struct CodeGenerator CodeGenerator;

// Create and destroy code generator
CodeGenerator *codegen_create(void);
void codegen_free(CodeGenerator *gen);

// Generate C code from IR
void codegen_generate_c(CodeGenerator *gen, Project *project, FILE *output);

#endif // CODEGEN_H
