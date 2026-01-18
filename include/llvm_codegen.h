#ifndef LLVM_CODEGEN_H
#define LLVM_CODEGEN_H

#include "compiler.h"
#include "ir.h"
#include "ast.h"
#include <stdio.h>

// LLVM Code Generator
typedef struct LLVMCodeGenerator LLVMCodeGenerator;

// Create and destroy LLVM code generator
LLVMCodeGenerator *llvm_codegen_create(void);
void llvm_codegen_free(LLVMCodeGenerator *gen);

// Generate native code via LLVM from IR
// Returns 0 on success, non-zero on error
int llvm_codegen_generate(LLVMCodeGenerator *gen, Project *project, const char *output_path);

#endif // LLVM_CODEGEN_H
