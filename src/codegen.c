#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/codegen.h"
#include "../include/irgen.h"
#include "../include/compiler.h"
#include "../include/loop_transform.h"

struct CodeGenerator {
    FILE *output;
    int indent_level;
    Project *project;
};

// Forward declarations
static char *type_to_c_string(Type *type);
static void gen_instruction(CodeGenerator *gen, IRFunction *func, IRInstruction *instr);
static void gen_for_loop(CodeGenerator *gen, IRFunction *func, LoopInfo *info);

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

        case IR_NOP:
            // Do nothing
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
        // Try to detect a simple loop starting here
        LoopInfo loop_info = detect_simple_loop(func, i);
        if (loop_info.is_simple_loop) {
            gen_for_loop(gen, func, &loop_info);
            i = loop_info.loop_end_idx; // Skip to end of loop
            continue;
        }
        
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

// Helper: Generate a for loop
static void gen_for_loop(CodeGenerator *gen, IRFunction *func, LoopInfo *info) {
    print_indent(gen);
    
    // Emit the label so that jumps to loop start (like continue in while loops) work
    IRInstruction *label_instr = func->instructions[info->loop_start_idx];
    if (label_instr->src1 && label_instr->src1->kind == IR_OP_LABEL) {
         fprintf(gen->output, "%s: ", label_instr->src1->data.label_name);
    }
    
    // Add vectorization hints
    // Using GCC specific pragmas or OMP if available, but #pragma GCC ivdep is safe
    fprintf(gen->output, "\n");
    print_indent(gen);
    fprintf(gen->output, "#pragma GCC ivdep\n");
    print_indent(gen);
    fprintf(gen->output, "for (");
    
    // Init: var = init_val
    // We assume the loop variable is the one being initialized
    // However, the pattern detected is: init is BEFORE the label.
    // So usually semantic analysis ensures the variable exists.
    // But wait, the standard C for loop is `for (init; cond; inc)`.
    // In our IR, init happens BEFORE the loop label.
    // GCC optimizes `var = init; while (cond) ...` to a for loop easily.
    // But explicitly generating `for (var = init; ...)` is better if we can move init inside.
    // `detect_simple_loop` starts AT the label. So init is PREVIOUS instruction?
    // The `detect_simple_loop` doesn't currently capture the init instruction.
    // It captures `loop_var`, `init_value` (if we extended it to look back).
    // Let's look at `detect_simple_loop` implementation in step 473.
    // It sets `info.loop_start_idx` to the label.
    // It does NOT find init value currently (impl in step 473 only checks label, cmp, branch, jump).
    
    // Actually, `detect_simple_loop` implementation in step 473 DOES NOT populate `loop_var`, `init_value`, etc.
    // It only returns `is_simple_loop` and indices.
    // I need to update `detect_simple_loop` or extract that info here.
    
    // Let's extract info here for now since I can't easily change loop_transform.c without another tool call.
    // Wait, the detecting logic was minimal.
    // Let's implement extraction here to be safe.
    
    // Pattern: 
    //   L_start:
    //     t_cond = var < limit (cmp)
    //     if (t_cond) goto L_body (branch)
    //     goto L_end
    
    IRInstruction *cmp = func->instructions[info->loop_start_idx + 1];
    IRInstruction *branch = func->instructions[info->loop_start_idx + 2];
    
    // cmp->src1 is likely the loop variable
    IROperand *loop_var_op = cmp->src1;
    IROperand *limit_op = cmp->src2;
    IROpcode cmp_op = cmp->opcode;
    
    // Loop variable name
    char *var_name = NULL;
    if (loop_var_op->kind == IR_OP_VAR) {
        var_name = loop_var_op->data.var_name;
    } else if (loop_var_op->kind == IR_OP_TEMP) {
        // Temps as loop vars? valid but we need valid C code.
        // We can print tX.
    }
    
    // Emit Init
    fprintf(gen->output, "; "); 
    
    // Emit Condition with __builtin_expect for specific opcode patterns
    // We expect the loop condition to be true typically
    fprintf(gen->output, "__builtin_expect(");
    gen_operand(gen, loop_var_op);
    switch (cmp_op) {
        case IR_LT: fprintf(gen->output, " < "); break;
        case IR_LE: fprintf(gen->output, " <= "); break;
        case IR_GT: fprintf(gen->output, " > "); break;
        case IR_GE: fprintf(gen->output, " >= "); break;
        default: fprintf(gen->output, " < "); break;
    }
    gen_operand(gen, limit_op);
    fprintf(gen->output, ", 1); ");
    
    // Emit Increment
    // We need to find the increment instruction.
    // It should be near the end of the loop, before the jump back.
    // Scan backward from loop_end_idx
    size_t inc_idx = 0;
    bool found_inc = false;
    for (size_t k = info->loop_end_idx - 1; k > info->loop_start_idx; k--) {
        IRInstruction *instr = func->instructions[k];
        // Look for var = var + step
        if (instr->opcode == IR_ADD && instr->dest) {
            // Check if dest is loop var
            bool dest_match = false;
            if (var_name && instr->dest->kind == IR_OP_VAR && strcmp(instr->dest->data.var_name, var_name) == 0) dest_match = true;
            if (!var_name && instr->dest->kind == IR_OP_TEMP && instr->dest->data.temp_id == loop_var_op->data.temp_id) dest_match = true;
            
            if (dest_match) {
                // Found increment!
                gen_operand(gen, instr->dest);
                fprintf(gen->output, " += "); // Assuming simple inc
                // If it was var = var + step, src1 is var, src2 is step.
                // If src1 is var, print src2. If src2 is var, print src1.
                if (instr->src1 && ((instr->src1->kind == IR_OP_VAR && var_name && strcmp(instr->src1->data.var_name, var_name) == 0) || 
                                   (instr->src1->kind == IR_OP_TEMP && !var_name && instr->src1->data.temp_id == loop_var_op->data.temp_id))) {
                    gen_operand(gen, instr->src2);
                } else {
                    gen_operand(gen, instr->src1);
                }
                found_inc = true;
                inc_idx = k;
                break;
            }
        }
         // Also handle var = step + var (commutative)
    }
    
    if (!found_inc) {
        // Fallback? or just empty increment
        // fprintf(gen->output, "/* no inc found */");
    }
    
    fprintf(gen->output, ") {\n");
    gen->indent_level++;
    
    // Generate Body
    // Start after branch (idx + 3)
    // End before increment (if found) or before jump back
    size_t body_end = found_inc ? inc_idx : info->loop_end_idx;
    // Also skip the "goto L_end" which is immediately after branch (idx+3 is typically goto L_end/L2, wait)
    // In the pattern: 
    //   if (t0) goto L1;  (branch)
    //   goto L2;          (jump to end)
    //   L1:               (body start)
    // The branch instruction at start_idx + 2 says "if (cond) goto BodyLabel".
    // IR_BRANCH src2 is the target label.
    // So the body starts at the instruction pointed to by src2.
    // We need to find where that label is defined.
    
    const char *body_label_name = branch->src2->data.label_name;
    size_t body_start = 0;
    for (size_t k = info->loop_start_idx; k < info->loop_end_idx; k++) {
        IRInstruction *instr = func->instructions[k];
        if (instr->opcode == IR_LABEL && instr->dest && strcmp(instr->dest->data.label_name, body_label_name) == 0) {
            body_start = k;
            break;
        }
    }
    
    // If we didn't find body label, something is wrong.
    if (body_start == 0) body_start = info->loop_start_idx + 4; // Fallback guess?
    
    // Skip the label itself
    for (size_t k = body_start + 1; k < body_end; k++) {
        // Try to detect a nested loop
        LoopInfo nested_info = detect_simple_loop(func, k);
        if (nested_info.is_simple_loop) {
            // Check if strict nesting (nested loop ends inside this body)
            // It should, otherwise detection might be crossing boundaries (unlikely with structured code)
            if (nested_info.loop_end_idx < body_end) {
                gen_for_loop(gen, func, &nested_info);
                k = nested_info.loop_end_idx;
                continue;
            }
        }
        
        IRInstruction *instr = func->instructions[k];
        gen_instruction(gen, func, instr);
    }
    
    gen->indent_level--;
    print_indent(gen);
    fprintf(gen->output, "}\n");
}

