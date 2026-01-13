#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "../include/compiler.h"
#include "../include/util.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/semantic.h"

Project *project_create(void) {
    Project *project = malloc(sizeof(Project));
    project->modules = NULL;
    project->module_count = 0;
    project->main_module = NULL;
    project->strict_unsafe_mode = false;
    return project;
}

static char *read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    
    fclose(file);
    return buffer;
}

Module *project_load_module(Project *project, const char *path, const char *relative_to) {
    char *res_path = resolve_module_path(relative_to, path);
    if (!res_path) {
        fprintf(stderr, "Error: Could not resolve module '%s' relative to '%s'\n", path, relative_to);
        return NULL;
    }
    printf("Debug: Loading module '%s' (resolved: '%s')\n", path, res_path);
    
    // Check if already loaded
    for (size_t i = 0; i < project->module_count; i++) {
        if (strcmp(project->modules[i]->path, res_path) == 0) {
            free(res_path);
            if (project->modules[i]->is_loading) {
                fprintf(stderr, "Error: Circular dependency detected involving module '%s'\n", project->modules[i]->path);
                return NULL;
            }
            return project->modules[i];
        }
    }

    // Load source
    char *source = read_file(res_path);
    if (!source) {
        fprintf(stderr, "Error: Could not read file '%s'\n", res_path);
        free(res_path);
        return NULL;
    }

    // Parse
    Lexer *lexer = lexer_init(source, res_path);
    Parser *parser = parser_init(lexer);
    ASTProgram *ast = parser_parse(parser);
    
    if (!ast) {
        parser_free(parser);
        lexer_free(lexer);
        free(source);
        free(res_path);
        return NULL;
    }

    // Create module
    Module *module = malloc(sizeof(Module));
    module->path = res_path;
    
    // Determine module name
    if (ast->module_name) {
        module->name = strdup(ast->module_name);
    } else {
        char *path_copy = strdup(res_path);
        char *bname = basename(path_copy);
        char *dot = strrchr(bname, '.');
        if (dot) *dot = '\0';
        module->name = strdup(bname);
        free(path_copy);
    }
    
    module->ast = ast;
    module->symtable = symtable_create();
    module->symtable->name = strdup(module->name);
    module->is_analyzed = false;
    module->is_loading = true; // Mark as currently loading

    // Add to project
    project->modules = realloc(project->modules, sizeof(Module*) * (project->module_count + 1));
    project->modules[project->module_count++] = module;
    if (project->module_count == 1) project->main_module = module;

    // Recursively parse imports
    for (size_t i = 0; i < ast->import_count; i++) {
        Module *imported = project_load_module(project, ast->imports[i]->import_path, res_path);
        if (!imported) {
            // Error already printed
            return NULL;
        }
    }
    
    module->is_loading = false; // Finished loading dependencies

    parser_free(parser);
    lexer_free(lexer);
    free(source);
    return module;
}

bool project_analyze(Project *project) {
    // 1. First pass: Collect all definitions from all modules
    for (size_t i = 0; i < project->module_count; i++) {
        Module *m = project->modules[i];
        SemanticAnalyzer *sa = semantic_create();
        sa->strict_unsafe_mode = project->strict_unsafe_mode;
        sa->current_filename = m->path;
        symtable_free(sa->symtable);
        sa->symtable = m->symtable;
        
        if (!semantic_analyze_declarations(sa, m->ast)) {
            return false;
        }
        
        sa->symtable = symtable_create();
        semantic_free(sa);
    }

    // 2. Resolve imports: Link module symbols to imported modules
    for (size_t i = 0; i < project->module_count; i++) {
        Module *m = project->modules[i];
        for (size_t j = 0; j < m->ast->import_count; j++) {
            ASTImportDecl *imp = m->ast->imports[j];
            char *res_path = resolve_module_path(m->path, imp->import_path);
            if (!res_path) {
                fprintf(stderr, "Error: Could not resolve import '%s' in %s\n", imp->import_path, m->path);
                return false;
            }
            
            Module *target = NULL;
            for (size_t k = 0; k < project->module_count; k++) {
                if (strcmp(project->modules[k]->path, res_path) == 0) {
                    target = project->modules[k];
                    break;
                }
            }
            free(res_path);
            
            if (!target) {
                fprintf(stderr, "Error: Imported module '%s' not loaded in project\n", imp->import_path);
                return false;
            }

            const char *alias_to_use = target->name;
            if (imp->alias) {
                alias_to_use = imp->alias;
            } else {
                // If no alias is provided, use the filename base as the default alias
                // instead of the global module name. This ensures that 'import "io.vx"'
                // results in 'io.' prefix being available.
                char *path_copy = strdup(imp->import_path);
                char *bname = basename(path_copy);
                char *dot = strrchr(bname, '.');
                if (dot) *dot = '\0';
                
                // We need to keep this bname alive or copy it since symbol_create strdups it.
                // We'll use a local name for symbol creation and free the copy.
                Symbol *mod_sym = symbol_create(bname, SYMBOL_MODULE, NULL, 0, 0);
                mod_sym->module_table = target->symtable;
                symtable_insert(m->symtable, mod_sym);
                
                free(path_copy);
                continue; // Skip the default symbol creation below
            }

            Symbol *mod_sym = symbol_create(alias_to_use, SYMBOL_MODULE, NULL, 0, 0);
            mod_sym->module_table = target->symtable;
            symtable_insert(m->symtable, mod_sym);
        }
    }

    // 3. Second pass: Body analysis (now that all imports are linked)
    for (size_t i = 0; i < project->module_count; i++) {
        Module *m = project->modules[i];
        SemanticAnalyzer *sa = semantic_create();
        sa->strict_unsafe_mode = project->strict_unsafe_mode;
        sa->current_filename = m->path;
        symtable_free(sa->symtable);
        sa->symtable = m->symtable;
        
        if (!semantic_analyze_bodies(sa, m->ast)) {
            return false;
        }
        
        sa->symtable = symtable_create();
        semantic_free(sa);
    }

    return true;
}

void project_free(Project *project) {
    if (!project) return;
    for (size_t i = 0; i < project->module_count; i++) {
        Module *m = project->modules[i];
        ast_free_program(m->ast);
        free(m->path);
        free(m->name);
        
        symtable_free(m->symtable);
        free(m);
    }
    free(project->modules);
    free(project);
}
