#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/codegen.h"
#include "../include/irgen.h"
#include "../include/compiler.h"

struct CodeGenerator {
    FILE *output;
    int indent_level;
    Project *project;
};

// Forward declarations
static char *type_to_c_string(Type *type);
static void gen_instruction(CodeGenerator *gen, IRFunction *func, IRInstruction *instr);

CodeGenerator *codegen_create(void) {
    CodeGenerator *gen = malloc(sizeof(CodeGenerator));
    gen->output = NULL;
    gen->indent_level = 0;
    gen->project = NULL;
    return gen;
}

void codegen_free(CodeGenerator *gen) {
    free(gen);
}

// Helper: Print indentation
// Helper: Escape special characters in strings for C output
static void print_escaped_string(FILE *output, const char *str) {
    fprintf(output, "\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '\n': fprintf(output, "\\n"); break;
            case '\t': fprintf(output, "\\t"); break;
            case '\r': fprintf(output, "\\r"); break;
            case '\\': fprintf(output, "\\\\"); break;
            case '"':  fprintf(output, "\\\""); break;
            default:   fputc(*p, output); break;
        }
    }
    fprintf(output, "\"");
}

static void print_indent(CodeGenerator *gen) {
    for (int i = 0; i < gen->indent_level; i++) {
        fprintf(gen->output, "    ");
    }
}

// Helper: Generate operand
static void gen_operand(CodeGenerator *gen, IROperand *op) {
    if (!op) {
        fprintf(gen->output, "0");
        return;
    }
    
    switch (op->kind) {
        case IR_OP_TEMP:
            fprintf(gen->output, "t%d", op->data.temp_id);
            break;
        case IR_OP_CONST:
            fprintf(gen->output, "%ld", op->data.const_value);
            break;
        case IR_OP_STRING:
            print_escaped_string(gen->output, op->data.string_value);
            break;
        case IR_OP_VAR:
            fprintf(gen->output, "%s", op->data.var_name);
            break;
        case IR_OP_LABEL:
            fprintf(gen->output, "%s", op->data.label_name);
            break;
    }
}

// Helper: Get destination type string
static char *get_dest_type(IROperand *dest, IRFunction *func) {
    if (!dest) return strdup("long");
    if (dest->kind == IR_OP_TEMP && func) {
        if ((size_t)dest->data.temp_id < func->temp_count && func->temp_types && func->temp_types[dest->data.temp_id]) {
            return strdup(func->temp_types[dest->data.temp_id]);
        }
    }
    if (dest->kind == IR_OP_VAR && func) {
        for (size_t i = 0; i < func->local_var_count; i++) {
            if (strcmp(dest->data.var_name, func->local_vars[i]) == 0) {
                return strdup(func->local_var_types[i]);
            }
        }
        for (size_t i = 0; i < func->param_count; i++) {
            if (strcmp(dest->data.var_name, func->params[i]) == 0) {
                return strdup(func->param_types[i]);
            }
        }
    }
    return strdup("long");
}

static ASTDecl *find_function_decl(Project *project, const char *name) {
    if (!project) return NULL;
    for (size_t m_idx = 0; m_idx < project->module_count; m_idx++) {
        Module *m = project->modules[m_idx];
        if (!m->ast) continue;
        for (size_t i = 0; i < m->ast->decl_count; i++) {
            ASTDecl *decl = m->ast->declarations[i];
            if (decl->type == AST_FUNCTION_DECL && strcmp(decl->data.function.name, name) == 0) {
                return decl;
            }
        }
    }
    return NULL;
}

// Helper: Generate instruction
static void gen_instruction(CodeGenerator *gen, IRFunction *func, IRInstruction *instr) {
    if (!instr) return;
    
    // Handle labels specially
    if (instr->opcode == IR_LABEL) {
        gen->indent_level--;
        print_indent(gen);
        gen_operand(gen, instr->src1);
        fprintf(gen->output, ":;\n");
        gen->indent_level++;
        return;
    }
    
    print_indent(gen);
    
    switch (instr->opcode) {
        case IR_ADD: {
            char *d_type = get_dest_type(instr->dest, func);
            gen_operand(gen, instr->dest);
            // Only cast to (long) if it's actually long
            if (strcmp(d_type, "long") == 0) {
                fprintf(gen->output, " = (long)(");
            } else {
                fprintf(gen->output, " = (");
            }
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " + ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ");\n");
            free(d_type);
            break;
        }
            
        case IR_SUB:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " - ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_MUL:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " * ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_DIV:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " / ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_MOD:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " %% ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_EQ:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " == ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_NE:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " != ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_LT:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " < ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_LE:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " <= ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_GT:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " > ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_GE:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " >= ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_AND:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " && ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_OR:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " || ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_NOT:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = !");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_NEG:
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = -");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_ADDR: {
            char *d_type = get_dest_type(instr->dest, func);
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = (%s)&", d_type);
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            free(d_type);
            break;
        }
            
        case IR_DEREF: {
            char *d_type = get_dest_type(instr->dest, func);
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = *(%s*)", d_type);
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            free(d_type);
            break;
        }
            
        case IR_MOVE: {
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            break;
        }
            
        case IR_LOAD: {
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            break;
        }
            
        case IR_STORE: {
            // No cast for store - assume IR types are correct
            gen_operand(gen, instr->src1);
            fprintf(gen->output, " = ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
        }
            
        case IR_JUMP:
            fprintf(gen->output, "goto ");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_BRANCH:
            fprintf(gen->output, "if (");
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ") goto ");
            gen_operand(gen, instr->src2);
            fprintf(gen->output, ";\n");
            break;
            
        case IR_FAIL:
            if (instr->src1) {
                fprintf(gen->output, "fprintf(stderr, \"Error: %%s\\n\", (char*)");
                gen_operand(gen, instr->src1);
                fprintf(gen->output, ");\n");
            } else {
                fprintf(gen->output, "fprintf(stderr, \"Error: program failure\\n\");\n");
            }
            fprintf(gen->output, "exit(1);\n");
            break;
            
        case IR_CALL: {
            if (instr->dest) {
                char *d_type = get_dest_type(instr->dest, func);
                gen_operand(gen, instr->dest);
                fprintf(gen->output, " = (%s)", d_type);
                free(d_type);
            }
            
            // Try to find the function declaration to get parameter types
            ASTDecl *callee_decl = NULL;
            if (instr->src1->kind == IR_OP_VAR) {
                callee_decl = find_function_decl(gen->project, instr->src1->data.var_name);
            }
            
            gen_operand(gen, instr->src1);
            fprintf(gen->output, "(");
            if (instr->args) {
                for (size_t i = 0; i < instr->arg_count; i++) {
                    if (i > 0) fprintf(gen->output, ", ");
                    
                    if (callee_decl && i < callee_decl->data.function.param_count) {
                        char *p_type = type_to_c_string(callee_decl->data.function.params[i].param_type);
                        fprintf(gen->output, "(%s)", p_type);
                        free(p_type);
                    }
                    
                    gen_operand(gen, instr->args[i]);
                }
            }
            fprintf(gen->output, ");\n");
            break;
        }
            
        case IR_RETURN:
            fprintf(gen->output, "return");
            if (instr->src1) {
                fprintf(gen->output, " ");
                gen_operand(gen, instr->src1);
            }
            fprintf(gen->output, ";\n");
            break;
            
        default:
            fprintf(gen->output, "/* unknown opcode */\n");
            break;
    }
}

// Generate function
static void gen_function(CodeGenerator *gen, IRFunction *func) {
    // Function signature
    const char *ret_type = (func->return_type && func->return_type[0]) ? func->return_type : "long";
    fprintf(gen->output, "%s %s(", ret_type, func->name);
    
    // Parameters (add restrict for pointer types to enable better optimization)
    for (size_t i = 0; i < func->param_count; i++) {
        if (i > 0) fprintf(gen->output, ", ");
        if (func->param_types && func->param_types[i]) {
            const char *type = func->param_types[i];
            // Add restrict keyword for pointer types
            if (strstr(type, "*") != NULL) {
                fprintf(gen->output, "%s restrict %s", type, func->params[i]);
            } else {
                fprintf(gen->output, "%s %s", type, func->params[i]);
            }
        } else {
            fprintf(gen->output, "long %s", func->params[i]);
        }
    }
    
    fprintf(gen->output, ") {\n");
    gen->indent_level++;
    
    // Declare all temporaries
    if (func->temp_count > 0) {
        for (size_t i = 0; i < func->temp_count; i++) {
            print_indent(gen);
            const char *type = (func->temp_types && func->temp_types[i]) ? func->temp_types[i] : "long";
            fprintf(gen->output, "%s t%zu;\n", type, i);
        }
    }
    
    // Declare all local variables
    if (func->local_var_count > 0) {
        for (size_t i = 0; i < func->local_var_count; i++) {
             print_indent(gen);
             if (func->local_var_types && func->local_var_types[i]) {
                 fprintf(gen->output, "%s %s;\n", func->local_var_types[i], func->local_vars[i]);
             } else {
                 fprintf(gen->output, "long %s;\n", func->local_vars[i]);
             }
        }
    }
    
    // Generate instructions
    for (size_t i = 0; i < func->instruction_count; i++) {
        gen_instruction(gen, func, func->instructions[i]);
    }
    
    gen->indent_level--;
    fprintf(gen->output, "}\n\n");
}

// Convert Virex type to C type string
static char *type_to_c_string(Type *type) {
    if (!type) return strdup("void");
    
    switch (type->kind) {
        case TYPE_PRIMITIVE:
            switch (type->data.primitive) {
                case TOKEN_I8: return strdup("int8_t");
                case TOKEN_U8: return strdup("uint8_t");
                case TOKEN_I16: return strdup("int16_t");
                case TOKEN_U16: return strdup("uint16_t");
                case TOKEN_I32: return strdup("int32_t");
                case TOKEN_U32: return strdup("uint32_t");
                case TOKEN_I64: return strdup("int64_t");
                case TOKEN_U64: return strdup("uint64_t");
                case TOKEN_F32: return strdup("float");
                case TOKEN_F64: return strdup("double");
                case TOKEN_BOOL: return strdup("int");
                case TOKEN_VOID: return strdup("void");
                case TOKEN_CSTRING: return strdup("char*");
                default: return strdup("long");
            }
        case TYPE_POINTER: {
            char *base = type_to_c_string(type->data.pointer.base);
            char *result = malloc(strlen(base) + 2);
            sprintf(result, "%s*", base);
            free(base);
            return result;
        }
        case TYPE_ARRAY:
            return strdup("long*");  // Arrays decay to pointers
        case TYPE_STRUCT: {
            char buf[256];
            snprintf(buf, 256, "struct %s", type->data.name);
            return strdup(buf);
        }
        case TYPE_ENUM: {
            char buf[256];
            snprintf(buf, 256, "enum %s", type->data.name);
            return strdup(buf);
        }
        default:
            return strdup("long");
    }
}

static void gen_struct_decl(CodeGenerator *gen, ASTDecl *decl) {
    if (decl->type != AST_STRUCT_DECL) return;
    
    fprintf(gen->output, "struct %s {\n", decl->data.struct_decl.name);
    gen->indent_level++;
    
    for (size_t i = 0; i < decl->data.struct_decl.field_count; i++) {
        print_indent(gen);
        Type *field_type = decl->data.struct_decl.fields[i].field_type;
        
        if (field_type->kind == TYPE_ARRAY) {
            // Handle array field: Type name[Size];
            char *elem_type_str = type_to_c_string(field_type->data.array.element);
            fprintf(gen->output, "%s %s[%zu];\n", elem_type_str, decl->data.struct_decl.fields[i].name, field_type->data.array.size);
            free(elem_type_str);
        } else {
            char *type_str = type_to_c_string(field_type);
            fprintf(gen->output, "%s %s;\n", type_str, decl->data.struct_decl.fields[i].name);
            free(type_str);
        }
    }
    
    gen->indent_level--;
    fprintf(gen->output, "}");
    if (decl->data.struct_decl.is_packed) {
        fprintf(gen->output, " __attribute__((packed))");
    }
    fprintf(gen->output, ";\n\n");
    fprintf(gen->output, ";\n\n");
}

static void gen_enum_decl(CodeGenerator *gen, ASTDecl *decl) {
    if (decl->type != AST_ENUM_DECL) return;
    
    // Generate C enum
    fprintf(gen->output, "enum %s {\n", decl->data.enum_decl.name);
    gen->indent_level++;
    
    for (size_t i = 0; i < decl->data.enum_decl.variant_count; i++) {
        print_indent(gen);
        // C enums are comma separated. 
        fprintf(gen->output, "%s", decl->data.enum_decl.variants[i].name);
        // Add explicit values if needed? AST doesn't store explicit values yet, assumes 0-indexed.
        // But if semantic analysis assigned values, we could use them.
        // For now, rely on standard C auto-increment matches Virex logic (0, 1, 2...).
        
        if (i < decl->data.enum_decl.variant_count - 1) {
            fprintf(gen->output, ",\n");
        } else {
            fprintf(gen->output, "\n");
        }
    }
    
    gen->indent_level--;
    fprintf(gen->output, "};\n\n");
}

// Main code generation function
void codegen_generate_c(CodeGenerator *gen, Project *project, FILE *output) {
    if (!gen || !project || !output) return;
    
    gen->output = output;
    gen->indent_level = 0;
    gen->project = project;
    
    // Header
    fprintf(output, "/* Generated by Virex Compiler */\n");
    fprintf(gen->output, "#include <stdio.h>\n");
    fprintf(gen->output, "#include <stdlib.h>\n");
    fprintf(gen->output, "#include <string.h>\n");
    fprintf(gen->output, "#include <stdint.h>\n\n");
    
    // Result type definition
    fprintf(output, "// Result type\n");
    fprintf(output, "struct Result {\n");
    fprintf(output, "    long is_ok;\n"); // 1 for ok, 0 for err
    fprintf(output, "    union {\n");
    fprintf(output, "        long ok_val;\n"); // Simplified: all values treated as long
    fprintf(output, "        long err_val;\n");
    fprintf(output, "    } data;\n");
    fprintf(output, "};\n\n");
    
    // Generate struct definitions
    fprintf(output, "// Struct definitions\n");
    for (size_t m_idx = 0; m_idx < project->module_count; m_idx++) {
        Module *m = project->modules[m_idx];
        if (m->ast) {
             for (size_t i = 0; i < m->ast->decl_count; i++) {
                 ASTDecl *decl = m->ast->declarations[i];
                 if (decl->type == AST_STRUCT_DECL) {
                     gen_struct_decl(gen, decl);
                 } else if (decl->type == AST_ENUM_DECL) {
                     gen_enum_decl(gen, decl);
                 }
             }
        }
    }
    fprintf(output, "\n");
    
    // Runtime library declarations
    fprintf(output, "// Virex Runtime Library\n");
    fprintf(output, "void* virex_alloc(long long size, long long count);\n");
    fprintf(output, "void virex_free(void* ptr);\n");
    fprintf(output, "void virex_copy(void* dst, const void* src, long long count);\n");
    fprintf(output, "void virex_set(void* dst, int value, long long count);\n");
    fprintf(output, "void virex_print_i32(int value);\n");
    fprintf(output, "void virex_println_i32(int value);\n");
    fprintf(output, "void virex_print_bool(int value);\n");
    fprintf(output, "void virex_println_bool(int value);\n");
    fprintf(output, "void virex_print_str(const char* str);\n");
    fprintf(output, "void virex_println_str(const char* str);\n");
    fprintf(output, "void virex_exit(int code);\n");
    fprintf(output, "void virex_init_args(int argc, char** argv);\n");
    // Result helpers
    fprintf(output, "long virex_result_ok(long val) {\n");
    fprintf(output, "    struct Result* res = malloc(sizeof(struct Result));\n");
    fprintf(output, "    res->is_ok = 1;\n");
    fprintf(output, "    res->data.ok_val = val;\n");
    fprintf(output, "    return (long)res;\n");
    fprintf(output, "}\n");
    fprintf(output, "long virex_result_err(long val) {\n");
    fprintf(output, "    struct Result* res = malloc(sizeof(struct Result));\n");
    fprintf(output, "    res->is_ok = 0;\n");
    fprintf(output, "    res->data.err_val = val;\n");
    fprintf(output, "    return (long)res;\n");
    fprintf(output, "}\n\n");
    fprintf(output, "void virex_init_args(int argc, char** argv);\n\n");
    
    // Extern function declarations (collected from all modules)
    fprintf(output, "// Extern function declarations\n");
    for (size_t m_idx = 0; m_idx < project->module_count; m_idx++) {
        Module *m = project->modules[m_idx];
        ASTProgram *program = m->ast;
        if (program) {
            for (size_t i = 0; i < program->decl_count; i++) {
                ASTDecl *decl = program->declarations[i];
                if (decl->type == AST_FUNCTION_DECL && decl->data.function.is_extern) {
                    // Skip standard C library functions that are already declared in headers
                    const char *name = decl->data.function.name;
                    if (strcmp(name, "printf") == 0 || strcmp(name, "puts") == 0 ||
                        strcmp(name, "malloc") == 0 || strcmp(name, "free") == 0 ||
                        strcmp(name, "exit") == 0 || strcmp(name, "sprintf") == 0 ||
                        strcmp(name, "snprintf") == 0 || strcmp(name, "fprintf") == 0 ||
                        strcmp(name, "strlen") == 0 || strcmp(name, "strcmp") == 0) {
                        continue; // Already declared in stdio.h/stdlib.h
                    }
                    
                    // For extern functions, we don't mangle names and use proper C types
                    // Generate return type
                    char *ret_type_str = type_to_c_string(decl->data.function.return_type);
                    fprintf(output, "%s %s(", ret_type_str, decl->data.function.name);
                    free(ret_type_str);
                    
                    // Generate parameters
                    for (size_t j = 0; j < decl->data.function.param_count; j++) {
                        if (j > 0) fprintf(output, ", ");
                        char *param_type_str = type_to_c_string(decl->data.function.params[j].param_type);
                        fprintf(output, "%s", param_type_str);
                        free(param_type_str);
                    }
                    
                    // Add variadic marker if needed
                    if (decl->data.function.is_variadic) {
                        if (decl->data.function.param_count > 0) fprintf(output, ", ");
                        fprintf(output, "...");
                    }
                    
                    fprintf(output, ");\n");
                }
            }
        }
    }
    fprintf(output, "\n");
    
    // Forward declarations (collected from all modules, mangled names)
    // Global variables and Forward declarations
    fprintf(output, "// Global variables and Forward declarations\n");
    IRGenerator *irgen_decl = irgen_create();
    for (size_t m_idx = 0; m_idx < project->module_count; m_idx++) {
        Module *m = project->modules[m_idx];
        IRModule *ir_module = irgen_generate(irgen_decl, m->ast, m->name, m->symtable, m == project->main_module);
        
        if (!ir_module) continue;

        // Emit globals
        for (size_t i = 0; i < ir_module->global_count; i++) {
            IRGlobal *g = ir_module->globals[i];
            // For now assuming primitive initialization
            fprintf(output, "%s %s = %ld;\n", g->c_type, g->name, g->init_value);
        }
        
        for (size_t i = 0; i < ir_module->function_count; i++) {
            IRFunction *f = ir_module->functions[i];
            const char *ret_type = (f->return_type && f->return_type[0]) ? f->return_type : "long";
            fprintf(output, "%s %s(", ret_type, f->name);
            for (size_t j = 0; j < f->param_count; j++) {
                if (j > 0) fprintf(output, ", ");
                if (f->param_types && f->param_types[j]) {
                    fprintf(output, "%s", f->param_types[j]);
                } else {
                    fprintf(output, "long");
                }
            }
            fprintf(output, ");\n");
        }
        ir_module_free(ir_module);
    }
    irgen_free(irgen_decl);
    fprintf(output, "\n");
    
    // Generate actual functions
    IRGenerator *irgen_body = irgen_create();
    for (size_t m_idx = 0; m_idx < project->module_count; m_idx++) {
        Module *m = project->modules[m_idx];
        fprintf(output, "/* Module: %s */\n", m->name);
        
        IRModule *ir_module = irgen_generate(irgen_body, m->ast, m->name, m->symtable, m == project->main_module);
        for (size_t i = 0; i < ir_module->function_count; i++) {
            gen_function(gen, ir_module->functions[i]);
        }
        ir_module_free(ir_module);
    }
    irgen_free(irgen_body);
}
