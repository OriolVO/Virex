#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/llvm_codegen.h"
#include "../include/compiler.h"

// Check if LLVM is available
#ifdef HAVE_LLVM
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#endif

struct LLVMCodeGenerator {
#ifdef HAVE_LLVM
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
#else
    int dummy; // Placeholder when LLVM not available
#endif
};

LLVMCodeGenerator *llvm_codegen_create(void) {
    LLVMCodeGenerator *gen = malloc(sizeof(LLVMCodeGenerator));
    if (!gen) return NULL;
    
#ifdef HAVE_LLVM
    gen->context = LLVMContextCreate();
    gen->module = LLVMModuleCreateWithNameInContext("virex_module", gen->context);
    gen->builder = LLVMCreateBuilderInContext(gen->context);
    
    printf("LLVM Code Generator initialized\n");
#else
    gen->dummy = 0;
    fprintf(stderr, "Error: LLVM support not compiled in. Rebuild with LLVM enabled.\n");
#endif
    
    return gen;
}

void llvm_codegen_free(LLVMCodeGenerator *gen) {
    if (!gen) return;
    
#ifdef HAVE_LLVM
    if (gen->builder) LLVMDisposeBuilder(gen->builder);
    if (gen->module) LLVMDisposeModule(gen->module);
    if (gen->context) LLVMContextDispose(gen->context);
#endif
    
    free(gen);
}

int llvm_codegen_generate(LLVMCodeGenerator *gen, Project *project, const char *output_path) {
    if (!gen || !project || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to llvm_codegen_generate\n");
        return 1;
    }
    
#ifdef HAVE_LLVM
    printf("LLVM codegen: Processing %zu modules\n", project->module_count);
    
    // TODO: Implement LLVM IR generation
    // For now, just verify module and print IR
    
    char *error = NULL;
    if (LLVMVerifyModule(gen->module, LLVMPrintMessageAction, &error)) {
        fprintf(stderr, "LLVM module verification failed: %s\n", error);
        LLVMDisposeMessage(error);
        return 1;
    }
    
    // Print LLVM IR to stdout for debugging
    printf("\n=== LLVM IR ===\n");
    LLVMDumpModule(gen->module);
    printf("=== End LLVM IR ===\n\n");
    
    printf("LLVM codegen: Would write to %s (not yet implemented)\n", output_path);
    return 0;
#else
    fprintf(stderr, "Error: LLVM backend not available. Rebuild with LLVM support.\n");
    (void)project; // Suppress unused warning
    (void)output_path;
    return 1;
#endif
}
