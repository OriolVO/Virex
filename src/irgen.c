#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/irgen.h"


// Convert Virex type to C type string
static char *type_to_c_string(Type *type) {
    if (!type) return strdup("long");
    
    switch (type->kind) {
        case TYPE_PRIMITIVE:
            if (type->data.primitive == TOKEN_F64) return strdup("double");
            if (type->data.primitive == TOKEN_VOID) return strdup("void");
            if (type->data.primitive == TOKEN_CSTRING) return strdup("const char*");
            return strdup("long"); // Default all ints/bools to long
            
        case TYPE_STRUCT: {
            // If it's a single uppercase letter, it's likely a type parameter
            // and we don't have monomorphization yet, so treat as long.
            if (strlen(type->data.name) == 1 && type->data.name[0] >= 'A' && type->data.name[0] <= 'Z') {
                return strdup("long");
            }
            char buf[256];
            snprintf(buf, 256, "struct %s", type->data.name);
            return strdup(buf);
        }
        
        case TYPE_ENUM:
            return strdup("long");

        case TYPE_RESULT:
            return strdup("long"); // Treat as pointer cast to long
            
        case TYPE_POINTER: {
             char *base = type_to_c_string(type->data.pointer.base);
             char buf[256];
             snprintf(buf, 256, "%s*", base);
             free(base);
             return strdup(buf);
        }
        
        case TYPE_ARRAY: {
             char *elem = type_to_c_string(type->data.array.element);
             char buf[256];
             snprintf(buf, 256, "%s*", elem);
             free(elem);
             return strdup(buf);
        }
        
        default: return strdup("long");
    }
}

typedef struct IRScope {
    struct IRScope *parent;
    char **names;      // Key (original name)
    char **ir_names;   // Value (unique IR name)
    size_t count;
    size_t capacity;
} IRScope;

struct IRGenerator {
    IRModule *module;
    IRFunction *current_function;
    const char *module_name;
    SymbolTable *symtable;
    int temp_counter;
    int label_counter;
    IRScope *current_scope;
    int var_counter; // For generating unique variable names
    bool is_main;
    
    // Loop stack for break/continue
    struct {
        char *continue_label;
        char *break_label;
    } *loop_stack;
    size_t loop_stack_size;
    size_t loop_stack_capacity;
};

// Stack helpers
static void push_loop(IRGenerator *gen, char *continue_label, char *break_label) {
    if (gen->loop_stack_size >= gen->loop_stack_capacity) {
        gen->loop_stack_capacity = (gen->loop_stack_capacity == 0) ? 8 : gen->loop_stack_capacity * 2;
        gen->loop_stack = realloc(gen->loop_stack, sizeof(*gen->loop_stack) * gen->loop_stack_capacity);
    }
    gen->loop_stack[gen->loop_stack_size].continue_label = continue_label;
    gen->loop_stack[gen->loop_stack_size].break_label = break_label;
    gen->loop_stack_size++;
}

static void pop_loop(IRGenerator *gen) {
    if (gen->loop_stack_size > 0) gen->loop_stack_size--;
}

static char *current_continue_label(IRGenerator *gen) {
    if (gen->loop_stack_size == 0) return NULL;
    return gen->loop_stack[gen->loop_stack_size - 1].continue_label;
}

static char *current_break_label(IRGenerator *gen) {
    if (gen->loop_stack_size == 0) return NULL;
    return gen->loop_stack[gen->loop_stack_size - 1].break_label;
}

// Forward declarations
static IROperand *lower_expr(IRGenerator *gen, ASTExpr *expr);
static void lower_stmt(IRGenerator *gen, ASTStmt *stmt);
static void add_local_variable(IRGenerator *gen, const char *name, Type *type);
static void lower_function(IRGenerator *gen, ASTDecl *decl);
static void lower_match_stmt(IRGenerator *gen, ASTStmt *stmt);
static void lower_fail_stmt(IRGenerator *gen, ASTStmt *stmt);

// Helper functions
static int new_temp(IRGenerator *gen, Type *type) {
    int id = gen->temp_counter++;
    if (gen->current_function) {
        gen->current_function->temp_count = gen->temp_counter;
        gen->current_function->temp_types = realloc(gen->current_function->temp_types, sizeof(char*) * gen->current_function->temp_count);
        gen->current_function->temp_types[id] = type_to_c_string(type);
    }
    return id;
}

static void sanitize_name(char *name) {
    for (char *p = name; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) {
            *p = '_';
        }
    }
}

static char *new_label(IRGenerator *gen, const char *prefix) {
    char *label = malloc(64);
    snprintf(label, 64, "%s%d", prefix, gen->label_counter++);
    return label;
}

static void emit(IRGenerator *gen, IRInstruction *instr) {
    ir_function_add_instruction(gen->current_function, instr);
}

// Scope management functions
static IRScope *scope_create(IRScope *parent) {
    IRScope *scope = malloc(sizeof(IRScope));
    scope->parent = parent;
    scope->names = NULL;
    scope->ir_names = NULL;
    scope->count = 0;
    scope->capacity = 0;
    return scope;
}

static void scope_free(IRScope *scope) {
    if (!scope) return;
    for (size_t i = 0; i < scope->count; i++) {
        free(scope->names[i]);
        free(scope->ir_names[i]);
    }
    free(scope->names);
    free(scope->ir_names);
    free(scope);
}

static void scope_enter(IRGenerator *gen) {
    gen->current_scope = scope_create(gen->current_scope);
}

static void scope_exit(IRGenerator *gen) {
    if (gen->current_scope) {
        IRScope *old = gen->current_scope;
        gen->current_scope = old->parent;
        scope_free(old);
    }
}

static char *scope_define(IRGenerator *gen, const char *name) {
    IRScope *scope = gen->current_scope;
    if (!scope) return NULL;
    
    // Check if check if already defined in current scope (should generally be unique per var decl, but AST might not guarantee)
    // Actually for shadowing we *want* to allow redefining in *nested* scopes, but within same scope it's just an update if AST allows re-decl (which it shouldn't usually).
    // But AST_VAR_DECL means a NEW variable.
    
    // Resize if needed
    if (scope->count >= scope->capacity) {
        scope->capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
        scope->names = realloc(scope->names, sizeof(char*) * scope->capacity);
        scope->ir_names = realloc(scope->ir_names, sizeof(char*) * scope->capacity);
    }
    
    // Generate unique IR name
    char *ir_name = malloc(64);
    // Use original name + unique suffix to keep it readable but unique
    // Sanitize source name first in case it has weird chars, though AST should differ.
    // Actually let's just use v_counter.
    // Better: name_v<counter>
    snprintf(ir_name, 64, "%s_v%d", name, gen->var_counter++);
    
    scope->names[scope->count] = strdup(name);
    scope->ir_names[scope->count] = strdup(ir_name);
    scope->count++;
    
    free(ir_name); // strdup made a copy
    return scope->ir_names[scope->count - 1]; // Return the stored copy
}

static const char *scope_lookup(IRGenerator *gen, const char *name) {
    IRScope *scope = gen->current_scope;
    while (scope) {
        for (size_t i = 0; i < scope->count; i++) {
            if (strcmp(scope->names[i], name) == 0) {
                return scope->ir_names[i];
            }
        }
        scope = scope->parent;
    }
    return name;
}

// Helper to build member access string recursively
static void get_member_access_string(IRGenerator *gen, ASTExpr *expr, char *buf, size_t size) {
    if (expr->type == AST_VARIABLE_EXPR) {
        const char *unique_name = scope_lookup(gen, expr->data.variable.name);
        strncpy(buf, unique_name, size);
    } else if (expr->type == AST_MEMBER_EXPR) {
        char base[256];
        get_member_access_string(gen, expr->data.member.object, base, sizeof(base));
        snprintf(buf, size, "%s%s%s", base, expr->data.member.is_arrow ? "->" : ".", expr->data.member.member);
    } else if (expr->type == AST_INDEX_EXPR) {
        char base[256];
        get_member_access_string(gen, expr->data.index.array, base, sizeof(base));
        
        IROperand *index_op = lower_expr(gen, expr->data.index.index);
        char index_str[64];
        if (index_op->kind == IR_OP_CONST) {
            snprintf(index_str, 64, "%ld", index_op->data.const_value);
        } else if (index_op->kind == IR_OP_VAR) {
             snprintf(index_str, 64, "%s", index_op->data.var_name);
        } else if (index_op->kind == IR_OP_TEMP) {
             snprintf(index_str, 64, "t%d", index_op->data.temp_id);
        } else if (index_op->kind == IR_OP_STRING) {
             snprintf(index_str, 64, "0"); // Invalid index
        }
        
        snprintf(buf, size, "%s[%s]", base, index_str);
    } else {
        strncpy(buf, "unknown", size);
    }
}

// Create/destroy
IRGenerator *irgen_create(void) {
    IRGenerator *gen = malloc(sizeof(IRGenerator));
    gen->module = NULL;
    gen->current_function = NULL;
    gen->module_name = NULL;
    gen->symtable = NULL;
    gen->temp_counter = 0;
    gen->label_counter = 0;
    gen->current_scope = NULL; // Will be created per function
    gen->var_counter = 0;
    return gen;
}

void irgen_free(IRGenerator *gen) {
    if (!gen) return;
    free(gen);
}

// Expression lowering
static IROperand *lower_expr(IRGenerator *gen, ASTExpr *expr) {
    if (!expr) return NULL;
    
    switch (expr->type) {
        case AST_LITERAL_EXPR: {
            // Return constant operand
            if (expr->data.literal.token->type == TOKEN_INTEGER) {
                return ir_operand_const(expr->data.literal.token->value.int_value);
            } else if (expr->data.literal.token->type == TOKEN_TRUE) {
                return ir_operand_const(1);
            } else if (expr->data.literal.token->type == TOKEN_FALSE) {
                return ir_operand_const(0);
            } else if (expr->data.literal.token->type == TOKEN_STRING) {
                return ir_operand_string(expr->data.literal.token->lexeme);
            } else if (expr->data.literal.token->type == TOKEN_NULL) {
                return ir_operand_const(0);
            }
            return ir_operand_const(0);
        }
        
        case AST_VARIABLE_EXPR: {
            // Check if it's a constant (enum)
            Symbol *sym = symtable_lookup(gen->symtable, expr->data.variable.name);
            if (sym && sym->kind == SYMBOL_CONSTANT) {
                return ir_operand_const(sym->enum_value);
            }
            
            // Return variable operand directly
            const char *unique_name = scope_lookup(gen, expr->data.variable.name);
            
            // If scope lookup returned the same name, it might be a global variable that needs mangling
            if (strcmp(unique_name, expr->data.variable.name) == 0 && gen->symtable) {
                Symbol *sym = symtable_lookup(gen->symtable, expr->data.variable.name);
                if (sym && (sym->kind == SYMBOL_VARIABLE || sym->kind == SYMBOL_CONSTANT)) {
                    // It's a global/constant, we need to mangle it: Module__Name
                    static char mangled_global[512]; // Static buffer warning: not thread safe but ok here
                    char mod_name_buf[256];
                    strncpy(mod_name_buf, gen->module_name, 255);
                    mod_name_buf[255] = '\0';
                    sanitize_name(mod_name_buf);
                    
                    snprintf(mangled_global, 512, "%s__%s", mod_name_buf, expr->data.variable.name);
                    return ir_operand_var(mangled_global);
                }
            }
            
            return ir_operand_var(unique_name);
        }
        
        case AST_BINARY_EXPR: {
            if (expr->data.binary.op == TOKEN_EQ) {
                IROperand *right = lower_expr(gen, expr->data.binary.right);
                
                if (expr->data.binary.left->type == AST_VARIABLE_EXPR) {
                    const char *unique_name = scope_lookup(gen, expr->data.binary.left->data.variable.name);
                    IROperand *left = ir_operand_var(unique_name);
                    emit(gen, ir_instruction_create(IR_STORE, NULL, left, right));
                    return right;
                } else if (expr->data.binary.left->type == AST_MEMBER_EXPR || expr->data.binary.left->type == AST_INDEX_EXPR) {
                    char access[256];
                    get_member_access_string(gen, expr->data.binary.left, access, sizeof(access));
                    emit(gen, ir_instruction_create(IR_STORE, NULL, ir_operand_var(access), right));
                } else if (expr->data.binary.left->type == AST_UNARY_EXPR && expr->data.binary.left->data.unary.op == TOKEN_STAR) {
                    // Handle *ptr = val
                    IROperand *ptr = lower_expr(gen, expr->data.binary.left->data.unary.operand);
                    char hack[256];
                    if (ptr->kind == IR_OP_TEMP) {
                        snprintf(hack, 256, "(*t%d)", ptr->data.temp_id);
                    } else if (ptr->kind == IR_OP_VAR) {
                        snprintf(hack, 256, "(*%s)", ptr->data.var_name);
                    }
                    emit(gen, ir_instruction_create(IR_STORE, NULL, ir_operand_var(hack), right));
                }
                return right;
            }

            IROperand *left = lower_expr(gen, expr->data.binary.left);
            IROperand *right = lower_expr(gen, expr->data.binary.right);
            
            IROpcode opcode;
            switch (expr->data.binary.op) {
                case TOKEN_PLUS: opcode = IR_ADD; break;
                case TOKEN_MINUS: opcode = IR_SUB; break;
                case TOKEN_STAR: opcode = IR_MUL; break;
                case TOKEN_SLASH: opcode = IR_DIV; break;
                case TOKEN_PERCENT: opcode = IR_MOD; break;
                case TOKEN_EQ_EQ: opcode = IR_EQ; break;
                case TOKEN_BANG_EQ: opcode = IR_NE; break;
                case TOKEN_LT: opcode = IR_LT; break;
                case TOKEN_LT_EQ: opcode = IR_LE; break;
                case TOKEN_GT: opcode = IR_GT; break;
                case TOKEN_GT_EQ: opcode = IR_GE; break;
                case TOKEN_AMP_AMP: opcode = IR_AND; break;
                case TOKEN_PIPE_PIPE: opcode = IR_OR; break;
                default: opcode = IR_ADD; break;
            }

            int temp = new_temp(gen, expr->expr_type);
            emit(gen, ir_instruction_create(opcode, ir_operand_temp(temp), left, right));
            return ir_operand_temp(temp);
        }
        
        case AST_UNARY_EXPR: {
            IROperand *operand = lower_expr(gen, expr->data.unary.operand);
            int temp = new_temp(gen, expr->expr_type);
            // IROperand *dest = ir_operand_temp(temp); // Don't allow reuse
            
            if (expr->data.unary.op == TOKEN_MINUS) {
                emit(gen, ir_instruction_create(IR_NEG, ir_operand_temp(temp), operand, NULL));
            } else if (expr->data.unary.op == TOKEN_BANG) {
                emit(gen, ir_instruction_create(IR_NOT, ir_operand_temp(temp), operand, NULL));
            } else if (expr->data.unary.op == TOKEN_AMP) {
                emit(gen, ir_instruction_create(IR_ADDR, ir_operand_temp(temp), operand, NULL));
            } else if (expr->data.unary.op == TOKEN_STAR) {
                emit(gen, ir_instruction_create(IR_DEREF, ir_operand_temp(temp), operand, NULL));
            }
            
            return ir_operand_temp(temp);
        }
        
        case AST_CALL_EXPR: {
            IROperand **args = NULL;
            if (expr->data.call.arg_count > 0) {
                args = malloc(sizeof(IROperand*) * expr->data.call.arg_count);
                for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                    args[i] = lower_expr(gen, expr->data.call.arguments[i]);
                }
            }
            
            char mangled_func_name[512];
            bool found_name = false;
            bool is_extern = false;
            
            // Handle qualified call: math.add()
            if (expr->data.call.callee->type == AST_MEMBER_EXPR && !expr->data.call.callee->data.member.is_arrow) {
                ASTExpr *obj = expr->data.call.callee->data.member.object;
                if (obj->type == AST_VARIABLE_EXPR) {
                    const char *target_module_name = obj->data.variable.name;
                    char *member_name = expr->data.call.callee->data.member.member;
                    
                    // Resolve alias and check for extern
                    if (gen->symtable) {
                        Symbol *mod_sym = symtable_lookup(gen->symtable, target_module_name);
                        if (mod_sym && mod_sym->kind == SYMBOL_MODULE) {
                            if (mod_sym->module_table->name) {
                                target_module_name = mod_sym->module_table->name;
                            }
                            
                            Symbol *member_sym = symtable_lookup(mod_sym->module_table, member_name);
                            if (member_sym && member_sym->kind == SYMBOL_FUNCTION && member_sym->is_extern) {
                                is_extern = true;
                            }
                        }
                    }
                    
                    if (is_extern && strcmp(target_module_name, "io") != 0 && strcmp(target_module_name, "std::io") != 0) {
                        // Use name as-is (for builtins/externs), but NOT for io/std::io which are builtins
                        strncpy(mangled_func_name, member_name, 511);
                    } else if ((strcmp(target_module_name, "io") == 0 || strcmp(target_module_name, "std::io") == 0) && 
                               (strcmp(member_name, "print") == 0 || strcmp(member_name, "println") == 0)) {
                        // Special handling for io.print and io.println - use virex_ prefix
                        snprintf(mangled_func_name, 512, "virex_%s", member_name);
                        is_extern = false; // Treat as internal for heuristic
                    } else if (strcmp(target_module_name, "result") == 0 || strcmp(target_module_name, "std::result") == 0) {
                        if (strcmp(member_name, "ok") == 0) {
                            strncpy(mangled_func_name, "virex_result_ok", 511);
                        } else if (strcmp(member_name, "err") == 0) {
                            strncpy(mangled_func_name, "virex_result_err", 511);
                        } else {
                            snprintf(mangled_func_name, 512, "%s__%s", target_module_name, member_name);
                        }
                        is_extern = false;
                    } else {
                        char mod_name_buf[256];
                        strncpy(mod_name_buf, target_module_name, 255);
                        mod_name_buf[255] = '\0';
                        sanitize_name(mod_name_buf);
                        
                        snprintf(mangled_func_name, 512, "%s__%s", mod_name_buf, member_name);
                    }
                    found_name = true;
                }
            }
            
            // Handle simple call: add()
            if (!found_name && expr->data.call.callee->type == AST_VARIABLE_EXPR) {
                const char *name = expr->data.call.callee->data.variable.name;
                
                // Check if this is an extern function
                Symbol *func_sym = symtable_lookup(gen->symtable, name);
                if (func_sym && func_sym->kind == SYMBOL_FUNCTION && func_sym->is_extern) {
                    // Don't mangle extern functions
                    strncpy(mangled_func_name, name, 511);
                    mangled_func_name[511] = '\0';
                    found_name = true;
                    is_extern = true;
                } else if (strcmp(name, "result::ok") == 0) {
                    strncpy(mangled_func_name, "virex_result_ok", 511);
                    found_name = true;
                } else if (strcmp(name, "result::err") == 0) {
                    strncpy(mangled_func_name, "virex_result_err", 511);
                    found_name = true;
                }
                else if (strcmp(name, "main") == 0 || strncmp(name, "virex_", 6) == 0) {
                    strncpy(mangled_func_name, name, 511);
                    found_name = true;
                } else {
                    snprintf(mangled_func_name, 511, "%s__%s", gen->module_name, name);
                    found_name = true;
                }
            }
            
            if (!found_name) {
                // Fallback for complex callees
                strncpy(mangled_func_name, "unknown_call", 511);
            }
            mangled_func_name[511] = '\0';
            
            // Apply builtin heuristic (print, println, exit -> virex_print, etc.) - but skip extern functions
            if (!is_extern && found_name && strncmp(mangled_func_name, "virex_", 6) != 0) {
                if (strncmp(mangled_func_name, "print", 5) == 0 || strcmp(mangled_func_name, "exit") == 0) {
                    char tmp[1024];
                    snprintf(tmp, 1023, "virex_%s", mangled_func_name);
                    strncpy(mangled_func_name, tmp, 511);
                } else if (strcmp(mangled_func_name, "result::ok") == 0) {
                    strncpy(mangled_func_name, "virex_result_ok", 511);
                } else if (strcmp(mangled_func_name, "result::err") == 0) {
                    strncpy(mangled_func_name, "virex_result_err", 511);
                }
            }
            
            // Specialized dispatch for generic print/println
            if (found_name && (strcmp(mangled_func_name, "virex_print") == 0 || strcmp(mangled_func_name, "virex_println") == 0)) {
                if (expr->data.call.arg_count > 0) {
                    ASTExpr *arg0 = expr->data.call.arguments[0];
                    if (arg0->expr_type) {
                        const char *suffix = NULL;
                        Type *t = arg0->expr_type;
                        if (t->kind == TYPE_PRIMITIVE) {
                            switch (t->data.primitive) {
                                case TOKEN_I32: suffix = "_i32"; break;
                                case TOKEN_I64: suffix = "_i64"; break;
                                case TOKEN_BOOL: suffix = "_bool"; break;
                                case TOKEN_CSTRING: suffix = "_str"; break;
                                default: break;
                            }
                        } else if (t->kind == TYPE_ENUM) {
                            suffix = "_i32";
                        }
                        if (suffix) {
                            strncat(mangled_func_name, suffix, 511 - strlen(mangled_func_name));
                        }
                    }
                }
            }
            
            int temp = -1;
            bool is_void = false;
            if (expr->expr_type && expr->expr_type->kind == TYPE_PRIMITIVE && expr->expr_type->data.primitive == TOKEN_VOID) {
                is_void = true;
            }
            
            if (!is_void) {
                temp = new_temp(gen, expr->expr_type);
            }
            
            IROperand *func_op = ir_operand_var(mangled_func_name);
            emit(gen, ir_instruction_create_call(is_void ? NULL : ir_operand_temp(temp), func_op, args, expr->data.call.arg_count));
            
            return is_void ? NULL : ir_operand_temp(temp);
        }
        
        case AST_MEMBER_EXPR: {
            // Check if it's module access first
            if (expr->data.member.object->type == AST_VARIABLE_EXPR) {
                Symbol *sym = symtable_lookup(gen->symtable, expr->data.member.object->data.variable.name);
                if (sym && sym->kind == SYMBOL_MODULE) {
                     char mod_name_buf[256];
                     strncpy(mod_name_buf, sym->module_table->name ? sym->module_table->name : sym->name, 255);
                     mod_name_buf[255] = '\0';
                     sanitize_name(mod_name_buf);
                     char mangled_name[512];
                     snprintf(mangled_name, 512, "%s__%s", mod_name_buf, expr->data.member.member);
                     return ir_operand_var(mangled_name);
                }
            }
            
            char access[256];
            get_member_access_string(gen, expr, access, sizeof(access));
            return ir_operand_var(access);
        }
        
        case AST_INDEX_EXPR: {
            // Re-use logic for getting access string
            char access[256];
            get_member_access_string(gen, expr, access, sizeof(access));
            return ir_operand_var(access);
        }

        default:
            return NULL;
    }
}

// Statement lowering
static void lower_stmt(IRGenerator *gen, ASTStmt *stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case AST_EXPR_STMT: {
            lower_expr(gen, stmt->data.expr_stmt.expr);
            break;
        }
        
        case AST_VAR_DECL_STMT: {
            // Define in scope and get unique name
            char *unique_name = scope_define(gen, stmt->data.var_decl.name);
            
            // Register unique name and type in local vars for stack allocation
             // Register local variable declaration for codegen (C backend)
             if (stmt->data.var_decl.name && stmt->data.var_decl.name[0] != '\0') {
                 add_local_variable(gen, unique_name, stmt->data.var_decl.var_type);
             }

            // Generate initialization if present
            if (stmt->data.var_decl.initializer) {
                IROperand *value = lower_expr(gen, stmt->data.var_decl.initializer);
                IROperand *var = ir_operand_var(unique_name);
                emit(gen, ir_instruction_create(IR_STORE, NULL, var, value));
            }
            break;
        }
        
        case AST_IF_STMT: {
            IROperand *cond = lower_expr(gen, stmt->data.if_stmt.condition);
            
            char *then_label = new_label(gen, "L");
            char *else_label = new_label(gen, "L");
            char *end_label = new_label(gen, "L");
            
            // Branch
            emit(gen, ir_instruction_create(IR_BRANCH, NULL, cond, ir_operand_label(then_label)));
            emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(else_label), NULL));
            
            // Then branch
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(then_label), NULL));
            lower_stmt(gen, stmt->data.if_stmt.then_branch);
            emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(end_label), NULL));
            
            // Else branch
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(else_label), NULL));
            if (stmt->data.if_stmt.else_branch) {
                lower_stmt(gen, stmt->data.if_stmt.else_branch);
            }
            
            // End
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(end_label), NULL));
            
            free(then_label);
            free(else_label);
            free(end_label);
            break;
        }
        
        case AST_WHILE_STMT: {
            char *loop_label = new_label(gen, "L");
            char *body_label = new_label(gen, "L");
            char *end_label = new_label(gen, "L");
            
            // Push loop context: continue -> loop_label (check cond), break -> end_label
            push_loop(gen, loop_label, end_label);
            
            // Loop start
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(loop_label), NULL));
            
            // Condition
            IROperand *cond = lower_expr(gen, stmt->data.while_stmt.condition);
            emit(gen, ir_instruction_create(IR_BRANCH, NULL, cond, ir_operand_label(body_label)));
            emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(end_label), NULL));
            
            // Body
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(body_label), NULL));
            lower_stmt(gen, stmt->data.while_stmt.body);
            emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(loop_label), NULL));
            
            // End
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(end_label), NULL));
            
            pop_loop(gen);
            
            free(loop_label);
            free(body_label);
            free(end_label);
            break;
        }

        case AST_FOR_STMT: {
            char *loop_label = new_label(gen, "L");
            char *body_label = new_label(gen, "L");
            char *end_label = new_label(gen, "L");
            
            // Initializer
            lower_stmt(gen, stmt->data.for_stmt.initializer);

            // Loop start
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(loop_label), NULL));
            
            // Condition
            if (stmt->data.for_stmt.condition) {
                IROperand *cond = lower_expr(gen, stmt->data.for_stmt.condition);
                emit(gen, ir_instruction_create(IR_BRANCH, NULL, cond, ir_operand_label(body_label)));
                emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(end_label), NULL));
            } else {
                emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(body_label), NULL));
            }
            
            // Continue label (for increment)
            char *continue_label = new_label(gen, "L_cont");
            
            // Push loop context: continue -> continue_label, break -> end_label
            push_loop(gen, continue_label, end_label);
            
            // Body
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(body_label), NULL));
            lower_stmt(gen, stmt->data.for_stmt.body);
            
            // Increment logic
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(continue_label), NULL));
            lower_expr(gen, stmt->data.for_stmt.increment);
            
            // Loop back
            emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(loop_label), NULL));
            
            // End
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(end_label), NULL));
            
            pop_loop(gen);
            
            free(loop_label);
            free(body_label);
            free(end_label);
            free(continue_label);
            break;
        }
        
        case AST_RETURN_STMT: {
            if (stmt->data.return_stmt.value) {
                IROperand *value = lower_expr(gen, stmt->data.return_stmt.value);
                emit(gen, ir_instruction_create(IR_RETURN, NULL, value, NULL));
            } else {
                emit(gen, ir_instruction_create(IR_RETURN, NULL, NULL, NULL));
            }
            break;
        }
        
        case AST_BLOCK_STMT:
            scope_enter(gen);
            for (size_t i = 0; i < stmt->data.block.stmt_count; i++) {
                lower_stmt(gen, stmt->data.block.statements[i]);
            }
            scope_exit(gen);
            break;

        case AST_MATCH_STMT:
            lower_match_stmt(gen, stmt);
            break;
            
        case AST_FAIL_STMT:
            lower_fail_stmt(gen, stmt);
            break;

        case AST_UNSAFE_STMT:
            lower_stmt(gen, stmt->data.unsafe_stmt.body);
            break;
        case AST_BREAK_STMT: {
            char *lbl = current_break_label(gen);
            if (lbl) {
                emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(lbl), NULL));
            }
            break;
        }

        case AST_CONTINUE_STMT: {
            char *lbl = current_continue_label(gen);
            if (lbl) {
                emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(lbl), NULL));
            }
            break;
        }
        
        default:
            break;
    }
}

// Helper to add local variable for C declaration
static void add_local_variable(IRGenerator *gen, const char *name, Type *type) {
    if (!gen->current_function || !name) return;
    IRFunction *func = gen->current_function;
    
    // Check if max vars reached? Just realloc.
    func->local_vars = realloc(func->local_vars, sizeof(char*) * (func->local_var_count + 1));
    func->local_var_types = realloc(func->local_var_types, sizeof(char*) * (func->local_var_count + 1));
    
    func->local_vars[func->local_var_count] = strdup(name);
    if (type) {
        func->local_var_types[func->local_var_count] = type_to_c_string(type);
    } else {
        // Default to "long" if type is not provided, as per instruction's comment
        func->local_var_types[func->local_var_count] = strdup("long"); // Assuming "long" is the default for Result data
    }
    func->local_var_count++;
}

static void lower_fail_stmt(IRGenerator *gen, ASTStmt *stmt) {
    IROperand *msg = NULL;
    if (stmt->data.fail_stmt.message) {
        msg = lower_expr(gen, stmt->data.fail_stmt.message);
    }
    emit(gen, ir_instruction_create(IR_FAIL, NULL, msg, NULL));
}

static void lower_match_stmt(IRGenerator *gen, ASTStmt *stmt) {
    ASTExpr *expr = stmt->data.match_stmt.expr;
    Type *expr_type = expr->expr_type;
    
    // Evaluate expression
    IROperand *result_ptr = lower_expr(gen, expr);

    if (expr_type && expr_type->kind == TYPE_ENUM) {
        // Enum Match Logic
        char *label_end = new_label(gen, "match_end");
        
        for (size_t i = 0; i < stmt->data.match_stmt.case_count; i++) {
            ASTMatchCase *cse = &stmt->data.match_stmt.cases[i];
            char *label_next = new_label(gen, "match_next");
            
            if (strcmp(cse->pattern_tag, "_") == 0) {
                // Wildcard matches everything
                scope_enter(gen);
                lower_stmt(gen, cse->body);
                scope_exit(gen);
                emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(label_end), NULL));
            } else {
                // Lookup enum value
                Symbol *sym = symtable_lookup(gen->symtable, cse->pattern_tag);
                long enum_val = 0;
                if (sym && sym->kind == SYMBOL_CONSTANT) {
                    enum_val = sym->enum_value;
                } else {
                    // Start of function scope might not see variants? 
                    // They should be in symtable.
                    // Fallback: This relies on valid semantic analysis.
                }
                
                int cond_temp = new_temp(gen, type_create_primitive(TOKEN_BOOL));
                IROperand *cond = ir_operand_temp(cond_temp);
                
                emit(gen, ir_instruction_create(IR_EQ, cond, ir_operand_clone(result_ptr), ir_operand_const(enum_val)));
                
                char *label_case = new_label(gen, "case");
                emit(gen, ir_instruction_create(IR_BRANCH, NULL, ir_operand_clone(cond), ir_operand_label(label_case)));
                emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(label_next), NULL));
                
                emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(label_case), NULL));
                scope_enter(gen);
                lower_stmt(gen, cse->body);
                scope_exit(gen);
                emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(label_end), NULL));
            }
            
            emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(label_next), NULL));
        }
        
        emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(label_end), NULL));
        return;
    }

    // Default: Result Match Logic (legacy hardcoded)
    
    // 1. Access tag (is_ok)
    // Assume result is a struct value, so access .tag
    // We assume the struct layout has 'tag' as first member (offset 0)
    int is_ok_temp = new_temp(gen, type_create_primitive(TOKEN_I64));
    IROperand *is_ok_op = ir_operand_temp(is_ok_temp);
    
    // Construct access string: ((struct Result*)var)->is_ok
    char tag_access[256];
    if (result_ptr->kind == IR_OP_VAR) {
        snprintf(tag_access, 256, "((struct Result*)%s)->is_ok", result_ptr->data.var_name);
    } else if (result_ptr->kind == IR_OP_TEMP) {
        snprintf(tag_access, 256, "((struct Result*)t%d)->is_ok", result_ptr->data.temp_id);
    } else {
        snprintf(tag_access, 256, "((struct Result*)0)->is_ok"); 
    }
    
    // Use MOVE instead of DEREF since we have the value in the operand string
    emit(gen, ir_instruction_create(IR_MOVE, is_ok_op, ir_operand_var(tag_access), NULL));
    
    // 2. Generate labels
    char *label_ok = new_label(gen, "match_ok");
    char *label_err = new_label(gen, "match_err");
    char *label_end = new_label(gen, "match_end");
    
    // 3. Find OK and ERR cases
    ASTMatchCase *case_ok = NULL;
    ASTMatchCase *case_err = NULL;
    
    for (size_t i = 0; i < stmt->data.match_stmt.case_count; i++) {
        if (strcmp(stmt->data.match_stmt.cases[i].pattern_tag, "ok") == 0) {
            case_ok = &stmt->data.match_stmt.cases[i];
        } else if (strcmp(stmt->data.match_stmt.cases[i].pattern_tag, "err") == 0) {
            case_err = &stmt->data.match_stmt.cases[i];
        }
    }
    
    // 4. Branch on is_ok
    // if (is_ok == 1) goto label_ok;
    // goto label_err;
    int cond_temp = new_temp(gen, type_create_primitive(TOKEN_BOOL));
    IROperand *cond = ir_operand_temp(cond_temp);
    
    emit(gen, ir_instruction_create(IR_EQ, cond, ir_operand_clone(is_ok_op), ir_operand_const(1)));
    
    // Branch true -> label_ok
    emit(gen, ir_instruction_create(IR_BRANCH, NULL, ir_operand_clone(cond), ir_operand_label(label_ok)));
    // Branch false (fallthrough replacement) -> label_err
    emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(label_err), NULL));
    
    // --- OK Case ---
    emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(label_ok), NULL));
    if (case_ok) {
        scope_enter(gen);
        
        if (case_ok->capture_name) {
            // Access data field
            int val_temp = new_temp(gen, type_create_primitive(TOKEN_I64));
            
            char data_access[256];
            if (result_ptr->kind == IR_OP_VAR) {
                snprintf(data_access, 256, "((struct Result*)%s)->data.ok_val", result_ptr->data.var_name);
            } else if (result_ptr->kind == IR_OP_TEMP) {
                snprintf(data_access, 256, "((struct Result*)t%d)->data.ok_val", result_ptr->data.temp_id);
            }
            
            emit(gen, ir_instruction_create(IR_MOVE, ir_operand_temp(val_temp), ir_operand_var(data_access), NULL));
            
            // Create user variable
            char *unique_name = scope_define(gen, case_ok->capture_name);
            
            // Register for C declaration (assuming i32 for now, or fetch from symtable?)
            // Wait, we generate C code "type var;".
            // If we don't know type, we can't declare it correctly.
            // FIX: We need type info.
            
            // Assuming 'long' for generic slots as simplified error handling Phase 12 plan?
            // "type_to_c_string" uses AST Type.
            // If I use "long" (generic slot) in C, it works for primitive integers.
            // But strictly, we need true type.
            // The AST doesn't have it on the case.
            // Ideally semantic analysis annotates the case with the type.
            // I'll use "long" for now as 'data' union member is likely 'long ok_val'.
            // struct Result { long is_ok; union { long ok_val; long err_val; } data; };
            // So captures are longs.
            add_local_variable(gen, unique_name, NULL); // NULL -> "long" special handling?
            
            // Or creating a dummy type primitive long?
            // I'll modify add_local_variable to handle specialized or default "long".
            
            emit(gen, ir_instruction_create(IR_MOVE, ir_operand_var(unique_name), ir_operand_temp(val_temp), NULL));
        }
        
        lower_stmt(gen, case_ok->body);
        scope_exit(gen);
    }
    emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(label_end), NULL));
    
    // --- ERR Case ---
    emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(label_err), NULL));
    if (case_err) {
        scope_enter(gen);
        
        if (case_err->capture_name) {
            // Access data field
            int val_temp = new_temp(gen, type_create_primitive(TOKEN_I64));
            
            char data_access[256];
            if (result_ptr->kind == IR_OP_VAR) {
                snprintf(data_access, 256, "((struct Result*)%s)->data.ok_val", result_ptr->data.var_name);
            } else if (result_ptr->kind == IR_OP_TEMP) {
                snprintf(data_access, 256, "((struct Result*)t%d)->data.ok_val", result_ptr->data.temp_id);
            }
            
            emit(gen, ir_instruction_create(IR_MOVE, ir_operand_temp(val_temp), ir_operand_var(data_access), NULL));
            
            char *unique_name = scope_define(gen, case_err->capture_name);
            add_local_variable(gen, unique_name, NULL);
            emit(gen, ir_instruction_create(IR_MOVE, ir_operand_var(unique_name), ir_operand_temp(val_temp), NULL));
        }
        
        lower_stmt(gen, case_err->body);
        scope_exit(gen);
    }
    emit(gen, ir_instruction_create(IR_JUMP, NULL, ir_operand_label(label_end), NULL));
    
    // End Label
    emit(gen, ir_instruction_create(IR_LABEL, NULL, ir_operand_label(label_end), NULL));
}

// Function lowering
static void lower_function(IRGenerator *gen, ASTDecl *decl) {
    if (decl->type != AST_FUNCTION_DECL) return;
    
    // Skip extern functions - they don't have bodies to lower
    if (decl->data.function.is_extern) return;
    
    // Mangle name: Module__Function (but only for 'main' in entry module)
    char mangled_name[512];
    char mod_name_buf[256];
    strncpy(mod_name_buf, gen->module_name, 255);
    mod_name_buf[255] = '\0';
    sanitize_name(mod_name_buf);

    if (strcmp(decl->data.function.name, "main") == 0) {
        if (gen->is_main) {
            strncpy(mangled_name, "main", 511);
        } else {
            snprintf(mangled_name, 511, "%s__main", mod_name_buf);
        }
    } else {
        snprintf(mangled_name, 511, "%s__%s", mod_name_buf, decl->data.function.name);
    }
    mangled_name[511] = '\0';

    // Create IR function
    IRFunction *ir_func = ir_function_create(mangled_name);
    
    gen->current_function = ir_func;
    gen->temp_counter = 0;
    gen->label_counter = 0;
    
    // Initialize function scope
    gen->current_scope = NULL; 
    scope_enter(gen); // Function top-level scope

    // Add parameters
    if (decl->data.function.param_count > 0) {
        ir_func->params = malloc(sizeof(char*) * decl->data.function.param_count);
        ir_func->param_types = malloc(sizeof(char*) * decl->data.function.param_count);
        ir_func->param_count = decl->data.function.param_count;
        
        for (size_t i = 0; i < decl->data.function.param_count; i++) {
            char *pname = decl->data.function.params[i].name;
            char *unique_param_name = scope_define(gen, pname);
            
            // Update the stored param name in IR function signature
            ir_func->params[i] = strdup(unique_param_name);
            ir_func->param_types[i] = type_to_c_string(decl->data.function.params[i].param_type);
        }
    }
    
    // Set return type
    if (decl->data.function.return_type) {
        ir_func->return_type = type_to_c_string(decl->data.function.return_type);
    } else {
        ir_func->return_type = strdup("void");
    }
    
    // Lower function body
    if (decl->data.function.body) {
        // Function body is a block, but we already have a scope (parameters).
        // If AST_FUNCTION_DECL body is a BLOCK_STMT, `lower_stmt` will create ANOTHER scope.
        // Usually fine, just an extra level.
        // But to be cleaner, maybe we shouldn't create a new scope for the top-level block?
        // Or just let it be.
        // Since parameters are in "function scope" and body block is "block scope", 
        // shadowing param in body is allowed in many langs.
        // Let's treat the body block as a nested scope.
        
        lower_stmt(gen, decl->data.function.body);
    }
    
    // Clean up function scope
    scope_exit(gen);
    
    // Update counts
    ir_func->temp_count = gen->temp_counter;
    ir_func->label_count = gen->label_counter;
    
    ir_module_add_function(gen->module, ir_func);
}

// Main generation function
IRModule *irgen_generate(IRGenerator *gen, ASTProgram *program, const char *module_name, SymbolTable *symtable, bool is_main) {
    if (!gen || !program) return NULL;
    
    gen->module = ir_module_create();
    gen->module_name = module_name;
    gen->symtable = symtable_create(); // Make sure to use own symtable if needed or pass it
    // Actually irgen_generate takes symtable argument.
    // Wait, generated code had `gen->symtable = symtable`.
    // I will just add loop stack init.
    gen->symtable = symtable;
    gen->is_main = is_main;
    gen->loop_stack = NULL;
    gen->loop_stack_size = 0;
    gen->loop_stack_capacity = 0;
    
    // Lower all functions
    for (size_t i = 0; i < program->decl_count; i++) {
        if (program->declarations[i]->type == AST_FUNCTION_DECL) {
            lower_function(gen, program->declarations[i]);
        } else if (program->declarations[i]->type == AST_VAR_DECL_STMT) {
            ASTGlobalVarDecl *var = &program->declarations[i]->data.var_decl;
            
            // Mangle name: Module__Var
            char mangled_name[512];
            char mod_name_buf[256];
            strncpy(mod_name_buf, gen->module_name, 255);
            mod_name_buf[255] = '\0';
            sanitize_name(mod_name_buf);
            
            snprintf(mangled_name, 512, "%s__%s", mod_name_buf, var->name);
            
            long init_val = 0;
            if (var->initializer && var->initializer->type == AST_LITERAL_EXPR) {
                if (var->initializer->data.literal.token->type == TOKEN_INTEGER) {
                    init_val = var->initializer->data.literal.token->value.int_value;
                } else if (var->initializer->data.literal.token->type == TOKEN_TRUE) {
                    init_val = 1;
                }
            }
            
            char *c_type = type_to_c_string(var->var_type);
            ir_module_add_global(gen->module, mangled_name, c_type, init_val);
            free(c_type);
        }
    }
    
    return gen->module;
}
