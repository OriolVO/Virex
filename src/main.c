#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
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
#include "../include/llvm_codegen.h"
#include "../include/compiler.h"

void print_version(void) {
    printf("Virex compiler v%s\n", VIREX_VERSION);
}

void print_help(void) {
    printf("Virex - Explicit control. Predictable speed. Minimal magic.\n\n");
    printf("Usage: virex [OPTIONS] [COMMAND]\n\n");
    printf("Commands:\n");
    printf("  build <file> [flags]  Compile a Virex source file\n");
    printf("                        (GCC flags like -o, -l, -I are passed through)\n\n");
    printf("Options:\n");
    printf("  --backend=<backend>   Select backend: 'c' (default) or 'llvm'\n");
    printf("  --strict-unsafe       Treat checks like unnecessary unsafe blocks as errors\n");
    printf("  --version             Print version information\n");
    printf("  --help                Print this help message\n");
    printf("  -o <file>             Specify output file path (directories auto-created)\n\n");
    printf("Examples:\n");
    printf("  virex build main.vx\n");
    printf("  virex build main.vx -o build/app\n");
    printf("  virex build main.vx --backend=llvm\n");
}





static int ensure_directory_exists(const char *path) {
    char temp[1024];
    char *p = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    
    // Remove trailing slash
    if(temp[len - 1] == '/')
        temp[len - 1] = 0;
        
    // Iterate string
    for(p = temp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            // Create dir
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "Error creating directory '%s': %s\n", temp, strerror(errno));
                return 0;
            }
            *p = '/';
        }
    }
    // Create final dir
    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error creating directory '%s': %s\n", temp, strerror(errno));
        return 0;
    }
    return 1;
}

static int compile_file(const char *filename, int extra_argc, char **extra_argv) {
    Project *project = project_create();
    
    char exe_name[256];
    // Default exe name from input filename
    strncpy(exe_name, basename((char*)filename), 255);
    char *dot = strrchr(exe_name, '.');
    if (dot) *dot = '\0';
    
    bool user_output_name = false;
    const char *backend = "c"; // Default to C backend
    
    // Parse Virex-specific flags and check for -o
    for (int i = 0; i < extra_argc; i++) {
        if (strcmp(extra_argv[i], "--strict-unsafe") == 0) {
            project->strict_unsafe_mode = true;
        } else if (strncmp(extra_argv[i], "--backend=", 10) == 0) {
            backend = extra_argv[i] + 10;
            if (strcmp(backend, "c") != 0 && strcmp(backend, "llvm") != 0) {
                fprintf(stderr, "Error: Unknown backend '%s'. Use 'c' or 'llvm'\n", backend);
                project_free(project);
                return 1;
            }
        } else if (strcmp(extra_argv[i], "-o") == 0 && i + 1 < extra_argc) {
            strncpy(exe_name, extra_argv[i+1], 255);
            user_output_name = true;
            
            // Extract dir and ensure it exists
            char *path_copy = strdup(exe_name);
            char *dir = dirname(path_copy);
            if (strcmp(dir, ".") != 0) {
                if (!ensure_directory_exists(dir)) {
                    free(path_copy);
                    project_free(project);
                    return 1;
                }
            }
            free(path_copy);
        }
    }
    
    if (!project_load_module(project, filename, ".")) {
        project_free(project);
        return 1;
    }

    if (!project_analyze(project)) {
        project_free(project);
        return 1;
    }
    
    // Backend selection
    if (strcmp(backend, "llvm") == 0) {
#ifdef HAVE_LLVM
        printf("✓ Using LLVM backend\n");
        LLVMCodeGenerator *llvm_gen = llvm_codegen_create();
        if (!llvm_gen) {
            fprintf(stderr, "Error: Failed to create LLVM code generator\n");
            project_free(project);
            return 1;
        }
        
        // For now, LLVM backend just verifies and prints IR
        int result = llvm_codegen_generate(llvm_gen, project, exe_name);
        llvm_codegen_free(llvm_gen);
        project_free(project);
        
        if (result != 0) {
            fprintf(stderr, "✗ LLVM code generation failed\n");
            return 1;
        }
        
        printf("✓ LLVM backend completed (full implementation pending)\n");
        return 0;
#else
        fprintf(stderr, "Error: LLVM backend not available. Rebuild with 'make llvm'\n");
        project_free(project);
        return 1;
#endif
    }
    
    // C backend (default)
    printf("✓ Using C backend\n");
    
    char output_filename[256];
    // Write C file to same dir as output exe or default
    if (user_output_name) {
         // Maintain directory structure for C file too? 
         // Or just put it in a consistent place? 
         // For now, keep it simple: virex_out.c in current dir.
         snprintf(output_filename, sizeof(output_filename), "virex_out.c");
    } else {
         snprintf(output_filename, sizeof(output_filename), "virex_out.c");
    }

    FILE *output = fopen(output_filename, "w");
    if (!output) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", output_filename);
        project_free(project);
        return 1;
    }
    CodeGenerator *codegen = codegen_create();
    codegen_generate_c(codegen, project, output);
    fclose(output);
    
    char compile_cmd[4096];
    int offset = snprintf(compile_cmd, sizeof(compile_cmd), "gcc -O2 %s runtime/virex_runtime.o -lm", output_filename);
    
    // Add extra arguments (flags, objects, libs)
    for (int i = 0; i < extra_argc; i++) {
        // Skip Virex-specific flags
        if (strcmp(extra_argv[i], "--strict-unsafe") == 0) continue;
        if (strncmp(extra_argv[i], "--backend=", 10) == 0) continue;
        
        // Skip -o and its argument if we handled it
        if (strcmp(extra_argv[i], "-o") == 0) {
            i++; // skip next arg
            continue;
        }
        
        offset += snprintf(compile_cmd + offset, sizeof(compile_cmd) - offset, " %s", extra_argv[i]);
    }
    
    // Output file (explicit check to ensure we use our decided name)
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
    
    // Quick check for simple flags
    if (argc >= 2) {
        if (strcmp(argv[1], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_help();
            return 0;
        }
    }
    
    // Check if we have at least 3 args (virex <cmd> <file>)
    // OR allow 2 args if help/version handled above (handled), or if cmd is explicit help?
    if (argc < 3) {
        if (argc == 2 && strcmp(argv[1], "build") == 0) {
            fprintf(stderr, "Error: Missing input file\n");
        }
        print_help();
        return 1;
    }
    
    const char *command = argv[1];
    const char *filename = argv[2];
    
    if (strcmp(command, "build") == 0) {
        return compile_file(filename, argc - 3, argv + 3);
    } else {
        fprintf(stderr, "Unknown command: %s\n\n", command);
        print_help();
        return 1;
    }
}
