#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "../include/virex.h"
#include "../include/lexer.h"
#include "../include/token.h"
#include "../include/error.h"
#include "../include/parser.h"
#include "../include/ast.h"
#include "../include/semantic.h"
#include "../include/ir.h"
#include "../include/irgen.h"
#include "../include/iropt.h"
#include "../include/codegen.h"
#include "../include/compiler.h"

void print_version(void) {
    printf("Virex compiler v%s\n", VIREX_VERSION);
}

void print_help(void) {
    printf("Virex - Explicit control. Predictable speed. Minimal magic.\n\n");
    printf("Usage: virex [OPTIONS] [COMMAND]\n\n");
    printf("Commands:\n");
    printf("  build <file>    Compile a Virex source file\n\n");
    printf("Options:\n");
    printf("  --version       Print version information\n");
    printf("  --help          Print this help message\n");
    printf("  --lex <file>    Tokenize a file and print tokens\n");
    printf("  --ast <file>    Parse a file and print AST\n");
    printf("  --check <file>  Type check a file\n");
    printf("  --emit-ir <file> Generate and print IR\n\n");
    printf("Examples:\n");
    printf("  virex build main.vx\n");
}





static int compile_file(const char *filename, int extra_argc, char **extra_argv) {
    Project *project = project_create();
    if (!project_load_module(project, filename, ".")) {
        project_free(project);
        return 1;
    }
    if (!project_analyze(project)) {
        project_free(project);
        return 1;
    }
    
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "virex_out.c");
    FILE *output = fopen(output_filename, "w");
    if (!output) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", output_filename);
        project_free(project);
        return 1;
    }
    CodeGenerator *codegen = codegen_create();
    codegen_generate_c(codegen, project, output);
    fclose(output);
    
    char exe_name[256];
    strncpy(exe_name, basename(project->main_module->path), 255);
    char *dot = strrchr(exe_name, '.');
    if (dot) *dot = '\0';
    
    char compile_cmd[4096];
    int offset = snprintf(compile_cmd, sizeof(compile_cmd), "gcc -O2 %s runtime/virex_runtime.o", output_filename);
    
    // Add extra arguments (flags, objects, libs)
    for (int i = 0; i < extra_argc; i++) {
        offset += snprintf(compile_cmd + offset, sizeof(compile_cmd) - offset, " %s", extra_argv[i]);
    }
    
    // Output file
    snprintf(compile_cmd + offset, sizeof(compile_cmd) - offset, " -o %s 2>&1", exe_name);
    
    printf("✓ Generated C code: %s\n", output_filename);
    printf("✓ Compiling with gcc...\n");
    // printf("  %s\n", compile_cmd); // Debug info
    
    int result = system(compile_cmd);
    if (result == 0) {
        printf("✓ Build successful: %s\n", exe_name);
    } else {
        fprintf(stderr, "✗ Compilation failed\n");
        codegen_free(codegen);
        project_free(project);
        return 1;
    }
    codegen_free(codegen);
    project_free(project);
    return 0;
}

int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <command> <file> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  build <file> [flags/libs]  - Compile and build executable\n");
        return 1;
    }
    
    const char *command = argv[1];
    const char *filename = argv[2];
    
    if (strcmp(command, "build") == 0) {
        return compile_file(filename, argc - 3, argv + 3);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        return 1;
    }
}
