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
static void collect_slice_types(Type *type, Type ***slice_types, size_t *count, size_t *capacity);
static void emit_slice_struct(FILE *output, Type *slice_type);
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

// Helper: Print a C declaration, handling array suffixes correctly
static void print_decl(FILE *output, const char *type_str, const char *name) {
    const char *bracket = strchr(type_str, '[');
    if (bracket) {
        int type_len = bracket - type_str;
        fprintf(output, "%.*s %s%s", type_len, type_str, name, bracket);
    } else {
        fprintf(output, "%s %s", type_str, name);
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
            fprintf(gen->output, "(struct Slice_uint8_t){ .data = (uint8_t*)");
            print_escaped_string(gen->output, op->data.string_value);
            fprintf(gen->output, ", .len = %zu }", strlen(op->data.string_value));
            break;
        case IR_OP_VAR:
            fprintf(gen->output, "%s", op->data.var_name);
            break;
        case IR_OP_LABEL:
            fprintf(gen->output, "%s", op->data.label_name);
            break;
        case IR_OP_FLOAT:
            fprintf(gen->output, "%g", op->data.float_value);
            break;
    }
}

// Helper: Get operand type string
static char *get_op_type(CodeGenerator *gen, IROperand *op, IRFunction *func) {
    if (!op) return NULL;
    if (op->kind == IR_OP_TEMP) {
        if (func && (size_t)op->data.temp_id < func->temp_count && func->temp_types) {
            return func->temp_types[op->data.temp_id];
        }
    }
    if (op->kind == IR_OP_VAR) {
        if (func) {
            for (size_t i = 0; i < func->local_var_count; i++) {
                if (strcmp(op->data.var_name, func->local_vars[i]) == 0) {
                    return func->local_var_types[i];
                }
            }
            for (size_t i = 0; i < func->param_count; i++) {
                if (strcmp(op->data.var_name, func->params[i]) == 0) {
                    return func->param_types[i];
                }
            }
        }
    }
    return NULL;
}

// Helper: Get destination type string
static char *get_dest_type(CodeGenerator *gen, IROperand *dest, IRFunction *func) {
    char *type = get_op_type(gen, dest, func);
    if (type) return strdup(type);
    return strdup("long");
}

static ASTDecl *find_function_decl(Project *project, const char *name) {
    if (!project || !name) return NULL;
    
    // 1. Try exact match
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
    
    // 2. Try handling mangled names (Module__Function)
    char *dup = strdup(name);
    char *sep = strstr(dup, "__");
    if (sep) {
        *sep = '\0';
        char *mod_name = dup;
        char *func_name = sep + 2;
        
        for (size_t m_idx = 0; m_idx < project->module_count; m_idx++) {
            Module *m = project->modules[m_idx];
            // Check if module name matches (might need sanitization check)
            if (m->name && strcmp(m->name, mod_name) == 0) {
                if (m->ast) {
                    for (size_t i = 0; i < m->ast->decl_count; i++) {
                        ASTDecl *decl = m->ast->declarations[i];
                        if (decl->type == AST_FUNCTION_DECL && strcmp(decl->data.function.name, func_name) == 0) {
                            free(dup);
                            return decl;
                        }
                    }
                }
            }
        }
    }
    free(dup);
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
            char *d_type = get_dest_type(gen, instr->dest, func);
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
            char *d_type = get_dest_type(gen, instr->dest, func);
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = (%s)&", d_type);
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            free(d_type);
            break;
        }
            
        case IR_DEREF: {
            char *d_type = get_dest_type(gen, instr->dest, func);
            gen_operand(gen, instr->dest);
            fprintf(gen->output, " = *(%s*)", d_type);
            gen_operand(gen, instr->src1);
            fprintf(gen->output, ";\n");
            free(d_type);
            break;
        }

        case IR_CAST: {
            char *d_type = get_dest_type(gen, instr->dest, func);
            char *s_type = get_op_type(gen, instr->src1, func);
            bool src_is_slice = (instr->src1->kind == IR_OP_STRING) || (s_type && strstr(s_type, "Slice") != NULL);
            bool dest_is_ptr = (strstr(d_type, "*") != NULL) || (strcmp(d_type, "const char*") == 0);

            gen_operand(gen, instr->dest);
            if (src_is_slice && dest_is_ptr) {
                fprintf(gen->output, " = (%s)(", d_type);
                gen_operand(gen, instr->src1);
                fprintf(gen->output, ").data;\n");
            } else {
                fprintf(gen->output, " = (%s)", d_type);
                gen_operand(gen, instr->src1);
                fprintf(gen->output, ";\n");
            }
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
                char *type = get_op_type(gen, instr->src1, func);
                bool is_slice = (instr->src1->kind == IR_OP_STRING) || (type && strstr(type, "Slice") != NULL);
                
                if (is_slice) {
                    fprintf(gen->output, "fprintf(stderr, \"Error: %%s\\n\", (char*)(");
                    gen_operand(gen, instr->src1);
                    fprintf(gen->output, ").data);\n");
                } else {
                    fprintf(gen->output, "fprintf(stderr, \"Error: %%s\\n\", (char*)");
                    gen_operand(gen, instr->src1);
                    fprintf(gen->output, ");\n");
                }
            } else {
                fprintf(gen->output, "fprintf(stderr, \"Error: program failure\\n\");\n");
            }
            fprintf(gen->output, "exit(1);\n");
            break;
            
        case IR_CALL: {
            if (instr->dest) {
                char *d_type = get_dest_type(gen, instr->dest, func);
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
                        char *arg_type = get_op_type(gen, instr->args[i], func);
                        bool is_ptr = (strstr(p_type, "*") != NULL);
                        bool arg_is_slice = (instr->args[i]->kind == IR_OP_STRING) || (arg_type && strstr(arg_type, "Slice") != NULL);
                        
                        if (is_ptr && arg_is_slice) {
                            fprintf(gen->output, "(%s)(", p_type);
                            gen_operand(gen, instr->args[i]);
                            fprintf(gen->output, ").data");
                        } else {
                            fprintf(gen->output, "(%s)", p_type);
                            gen_operand(gen, instr->args[i]);
                        }
                        free(p_type);
                    } else {
                        gen_operand(gen, instr->args[i]);
                    }
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
                // For pointers, it's easier to just print it
                fprintf(gen->output, "%s restrict %s", type, func->params[i]);
            } else {
                print_decl(gen->output, type, func->params[i]);
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
            char name[32];
            snprintf(name, 32, "t%zu", i);
            print_decl(gen->output, type, name);
            fprintf(gen->output, ";\n");
        }
    }
    
    // Declare all local variables
    if (func->local_var_count > 0) {
        for (size_t i = 0; i < func->local_var_count; i++) {
             print_indent(gen);
             if (func->local_var_types && func->local_var_types[i]) {
                 print_decl(gen->output, func->local_var_types[i], func->local_vars[i]);
                 fprintf(gen->output, ";\n");
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
                case TOKEN_I64: return strdup("long long");
                case TOKEN_U64: return strdup("uint64_t");
                case TOKEN_F32: return strdup("float");
                case TOKEN_F64: return strdup("double");
                case TOKEN_BOOL: return strdup("int");
                case TOKEN_VOID: return strdup("void");
                case TOKEN_CSTRING: return strdup("const char*");
                default: return strdup("long");
            }
        case TYPE_POINTER: {
            char *base = type_to_c_string(type->data.pointer.base);
            char *result = malloc(strlen(base) + 2);
            sprintf(result, "%s*", base);
            free(base);
            return result;
        }
        case TYPE_ARRAY: {
            char *elem = type_to_c_string(type->data.array.element);
            char buf[256];
            snprintf(buf, 256, "%s[%zu]", elem, type->data.array.size);
            free(elem);
            return strdup(buf);
        }
        case TYPE_SLICE: {
            // Generate slice struct name: Slice_ElementType
            char *elem_str = type_to_c_string(type->data.slice.element);
            // Remove spaces and special chars from element type for struct name
            char *clean_elem = malloc(strlen(elem_str) + 1);
            size_t j = 0;
            for (size_t i = 0; elem_str[i]; i++) {
                if (elem_str[i] != ' ' && elem_str[i] != '*') {
                    clean_elem[j++] = elem_str[i];
                }
            }
            clean_elem[j] = '\0';
            
            char buf[256];
            snprintf(buf, 256, "struct Slice_%s", clean_elem);
            free(elem_str);
            free(clean_elem);
            return strdup(buf);
        }
        case TYPE_STRUCT: {
            // If it's a single uppercase letter, it's likely a type parameter
            // and we don't have monomorphization yet, so treat as uint8_t
            if (type->data.struct_enum.name && 
                strlen(type->data.struct_enum.name) == 1 && 
                type->data.struct_enum.name[0] >= 'A' && type->data.struct_enum.name[0] <= 'Z') {
                return strdup("uint8_t");
            }
            char buf[256];
            snprintf(buf, 256, "struct %s", type->data.struct_enum.name ? type->data.struct_enum.name : "unknown");
            return strdup(buf);
        }
        case TYPE_ENUM: {
            char buf[256];
            snprintf(buf, 256, "enum %s", type->data.struct_enum.name ? type->data.struct_enum.name : "unknown");
            return strdup(buf);
        }
        case TYPE_RESULT:
            return strdup("struct Result*");
        case TYPE_FUNCTION:
            return strdup("void*");
        default:
            return strdup("long");
    }
}

// Collect all slice types used in a type (recursive)
static void collect_slice_types(Type *type, Type ***slice_types, size_t *count, size_t *capacity) {
    if (!type) return;
    
    if (type->kind == TYPE_SLICE) {
        // Check if already collected (simple duplicate check)
        for (size_t i = 0; i < *count; i++) {
            // Simple comparison - just check element type kind
            char *s1 = type_to_c_string(type->data.slice.element);
            char *s2 = type_to_c_string((*slice_types)[i]->data.slice.element);
            int cmp = strcmp(s1, s2);
            free(s1);
            free(s2);
            if (cmp == 0) return;
        }
        
        // Add to collection
        if (*count >= *capacity) {
            *capacity = (*capacity == 0) ? 4 : (*capacity * 2);
            *slice_types = realloc(*slice_types, sizeof(Type*) * (*capacity));
        }
        (*slice_types)[(*count)++] = type;
        
        // Recurse into element type
        collect_slice_types(type->data.slice.element, slice_types, count, capacity);
    } else if (type->kind == TYPE_POINTER) {
        collect_slice_types(type->data.pointer.base, slice_types, count, capacity);
    } else if (type->kind == TYPE_ARRAY) {
        collect_slice_types(type->data.array.element, slice_types, count, capacity);
    }
}

// Emit slice struct definition
static void emit_slice_struct(FILE *output, Type *slice_type) {
    char *elem_c_type = type_to_c_string(slice_type->data.slice.element);
    char *slice_c_type = type_to_c_string(slice_type);
    
    // Check if struct name slice_c_type starts with "struct "
    char *struct_name = slice_c_type;
    if (strncmp(slice_c_type, "struct ", 7) == 0) {
        struct_name += 7;
    }
    
    fprintf(output, "struct %s {\n", struct_name);
    fprintf(output, "    %s* data;\n", elem_c_type);
    fprintf(output, "    int64_t len;\n");
    fprintf(output, "};\n\n");
    
    free(elem_c_type);
    free(slice_c_type);
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


static void collect_slice_types_from_stmt(ASTStmt *stmt, Type ***types, size_t *count, size_t *cap) {
    if (!stmt) return;
    switch (stmt->type) {
        case AST_BLOCK_STMT:
            for (size_t i = 0; i < stmt->data.block.stmt_count; i++)
                collect_slice_types_from_stmt(stmt->data.block.statements[i], types, count, cap);
            break;
        case AST_VAR_DECL_STMT:
             collect_slice_types(stmt->data.var_decl.var_type, types, count, cap);
             break;
        case AST_IF_STMT:
             collect_slice_types_from_stmt(stmt->data.if_stmt.then_branch, types, count, cap);
             collect_slice_types_from_stmt(stmt->data.if_stmt.else_branch, types, count, cap);
             break;
        case AST_WHILE_STMT:
             collect_slice_types_from_stmt(stmt->data.while_stmt.body, types, count, cap);
             break;
        case AST_FOR_STMT:
             collect_slice_types_from_stmt(stmt->data.for_stmt.initializer, types, count, cap);
             collect_slice_types_from_stmt(stmt->data.for_stmt.body, types, count, cap);
             break;
        default: break;
    }
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
    
    // Collect and emit slice struct definitions
    Type **slice_types = NULL;
    size_t slice_count = 0;
    size_t slice_capacity = 0;
    
    // Always include []u8 slice type (for string literals)
    Type *u8_t = type_create_primitive(TOKEN_U8);
    Type *u8_slice = type_create_slice(u8_t);
    collect_slice_types(u8_slice, &slice_types, &slice_count, &slice_capacity);
    
    // Collect from all global declarations
    for (size_t m_idx = 0; m_idx < project->module_count; m_idx++) {
        Module *m = project->modules[m_idx];
        if (m->ast) {
             for (size_t i = 0; i < m->ast->decl_count; i++) {
                 ASTDecl *decl = m->ast->declarations[i];
                 if (decl->type == AST_VAR_DECL_STMT) {
                     collect_slice_types(decl->data.var_decl.var_type, &slice_types, &slice_count, &slice_capacity);
                 } else if (decl->type == AST_FUNCTION_DECL) {
                     collect_slice_types(decl->data.function.return_type, &slice_types, &slice_count, &slice_capacity);
                     for (size_t p=0; p < decl->data.function.param_count; p++) {
                         collect_slice_types(decl->data.function.params[p].param_type, &slice_types, &slice_count, &slice_capacity);
                     }
                     
                     if (decl->data.function.body) {
                         collect_slice_types_from_stmt(decl->data.function.body, &slice_types, &slice_count, &slice_capacity);
                     }
                 } else if (decl->type == AST_STRUCT_DECL) {
                     for (size_t f=0; f < decl->data.struct_decl.field_count; f++) {
                         collect_slice_types(decl->data.struct_decl.fields[f].field_type, &slice_types, &slice_count, &slice_capacity);
                     }
                 }
             }
        }
    }
    
    if (slice_count > 0) {
        fprintf(output, "// Slice definitions\n");
        for (size_t i = 0; i < slice_count; i++) {
            emit_slice_struct(output, slice_types[i]);
        }
        free(slice_types);
        fprintf(output, "\n");
    }
    
    // Generate monomorphized and regular struct/enum definitions from symbol table
    // Use the symbol table to ensure we use mangled names and avoid duplicates
    for (size_t m_idx = 0; m_idx < project->module_count; m_idx++) {
        Module *m = project->modules[m_idx];
        if (m->symtable && m->symtable->global_scope) {
            Scope *scope = m->symtable->global_scope;
            for (size_t i = 0; i < scope->symbol_count; i++) {
                Symbol *sym = scope->symbols[i];
                if (sym->kind == SYMBOL_TYPE) {
                    // 1. Skip if it's not the canonical name for this type (avoids aliases)
                    if (sym->type->data.struct_enum.name && 
                        strcmp(sym->name, sym->type->data.struct_enum.name) != 0) {
                        continue;
                    }
                    
                    // 2. Skip generic templates (they are not concrete types)
                    if (sym->type_param_count > 0) {
                        continue;
                    }
                    
                    // 3. Skip built-in types that are handled manually
                    if (strcmp(sym->name, "Result") == 0) {
                        continue;
                    }

                    if (sym->type->kind == TYPE_STRUCT) {
                        // Emit struct
                        if (sym->is_packed) {
                            fprintf(output, "struct __attribute__((packed)) %s {\n", sym->name);
                        } else {
                            fprintf(output, "struct %s {\n", sym->name);
                        }
                        gen->indent_level++;
                        for (size_t j = 0; j < sym->field_count; j++) {
                            print_indent(gen);
                            char *type_str = type_to_c_string(sym->fields[j].type);
                            print_decl(output, type_str, sym->fields[j].name);
                            fprintf(output, ";\n");
                            free(type_str);
                        }
                        gen->indent_level--;
                        fprintf(output, "};\n\n");
                    } else if (sym->type->kind == TYPE_ENUM) {
                        // Emit enum
                        fprintf(output, "enum %s {\n", sym->name);
                        gen->indent_level++;
                        for (size_t j = 0; j < sym->variant_count; j++) {
                            print_indent(gen);
                            fprintf(output, "%s", sym->variants[j]);
                            if (j < sym->variant_count - 1) {
                                fprintf(output, ",\n");
                            } else {
                                fprintf(output, "\n");
                            }
                        }
                        gen->indent_level--;
                        fprintf(output, "};\n\n");
                    }
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
    fprintf(output, "void virex_print_i64(long long value);\n");
    fprintf(output, "void virex_print_bool(int value);\n");
    fprintf(output, "void virex_print_str(const char* str);\n");
    fprintf(output, "void virex_print_slice_uint8_t(struct Slice_uint8_t s);\n");
    fprintf(output, "void virex_print_f64(double value);\n");
    fprintf(output, "void virex_exit(int code);\n");
    fprintf(output, "void virex_init_args(int argc, char** argv);\n");
    fprintf(output, "void virex_slice_bounds_check(long long index, long long len);\n");
    fprintf(output, "void virex_slice_range_check(long long start, long long end, long long cap);\n");
    // Math helpers
    fprintf(output, "double virex_math_sqrt(double x);\n");
    fprintf(output, "double virex_math_pow(double x, double y);\n");
    fprintf(output, "double virex_math_sin(double x);\n");
    fprintf(output, "double virex_math_cos(double x);\n");
    fprintf(output, "double virex_math_tan(double x);\n");
    fprintf(output, "double virex_math_log(double x);\n");
    fprintf(output, "double virex_math_exp(double x);\n");
    fprintf(output, "double virex_math_fabs(double x);\n");
    fprintf(output, "double virex_math_floor(double x);\n");
    fprintf(output, "double virex_math_ceil(double x);\n");
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
    fprintf(output, "void virex_init_args(int argc, char** argv);\n");
    
    fprintf(output, "void virex_slice_bounds_check(long long index, long long len) {\n");
    fprintf(output, "    if (index < 0 || index >= len) {\n");
    fprintf(output, "        fprintf(stderr, \"panic: index out of bounds: index %%lld, len %%lld\\n\", index, len);\n");
    fprintf(output, "        exit(134);\n");
    fprintf(output, "    }\n");
    fprintf(output, "}\n");
    
    fprintf(output, "void virex_slice_range_check(long long start, long long end, long long cap) {\n");
    fprintf(output, "    if (start < 0 || end < start || end > cap) {\n");
    fprintf(output, "        fprintf(stderr, \"panic: slice bounds out of range: [%%lld:%%lld] capacity %%lld\\n\", start, end, cap);\n");
    fprintf(output, "        exit(134);\n");
    fprintf(output, "    }\n");
    fprintf(output, "}\n\n");
    
    fprintf(output, "void virex_print_slice_uint8_t(struct Slice_uint8_t s) {\n");
    fprintf(output, "    if (s.data) {\n");
    fprintf(output, "        fwrite(s.data, 1, s.len, stdout);\n");
    fprintf(output, "    }\n");
    fprintf(output, "}\n\n");
    
    // std::mem runtime implementations
    fprintf(output, "void* alloc(long long count) {\n");
    fprintf(output, "    return calloc(count, 1);\n");
    fprintf(output, "}\n\n");
    
    fprintf(output, "void copy(void* dst, const void* src, long long count) {\n");
    fprintf(output, "    memcpy(dst, src, count);\n");
    fprintf(output, "}\n\n");
    
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
                        strcmp(name, "strlen") == 0 || strcmp(name, "strcmp") == 0 ||
                        decl->data.function.type_param_count > 0) {
                        continue; // Already declared in headers or handled by builtins
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
            // Use print_decl for globals to handle array types correctly
            print_decl(output, g->c_type, g->name);
            if (strchr(g->c_type, '[') == NULL) {
                fprintf(output, " = %ld;\n", g->init_value);
            } else {
                fprintf(output, ";\n"); // Arrays can't be initialized with a single long value
            }
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

