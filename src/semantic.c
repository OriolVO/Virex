#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/semantic.h"
#include "../include/error.h"

// Forward declarations
static void analyze_stmt(SemanticAnalyzer *sa, ASTStmt *stmt);
static Type *analyze_expr(SemanticAnalyzer *sa, ASTExpr *expr);
static bool check_return_paths(ASTStmt *stmt);

static void semantic_error(SemanticAnalyzer *sa, size_t line, size_t column, const char *message);
static bool types_equal(Type *a, Type *b);
static bool types_compatible(Type *a, Type *b);
static bool is_numeric_type(Type *type);
static bool is_integer_type(Type *type);
static bool infer_type(Type *param_type, Type *arg_type, char **type_params, size_t count, Type **inferred);
static void resolve_type(SemanticAnalyzer *sa, Type *type);
static Symbol *find_type_symbol(SemanticAnalyzer *sa, const char *name);

// Create semantic analyzer
SemanticAnalyzer *semantic_create(void) {
    SemanticAnalyzer *sa = malloc(sizeof(SemanticAnalyzer));
    sa->symtable = symtable_create();
    sa->current_function_return_type = NULL;
    sa->in_unsafe_block = false;
    sa->loop_depth = 0;
    sa->scope_depth = 0;
    sa->had_error = false;
    sa->strict_unsafe_mode = false;
    sa->current_block_has_unsafe_op = false;
    sa->current_filename = NULL;
    
    // Initialize instantiation registry
    sa->instantiation_registry = malloc(sizeof(InstantiationRegistry));
    sa->instantiation_registry->instantiations = NULL;
    sa->instantiation_registry->count = 0;
    sa->instantiation_registry->capacity = 0;
    
    return sa;
}

void semantic_free(SemanticAnalyzer *sa) {
    if (!sa) return;
    
    // Free instantiation registry
    if (sa->instantiation_registry) {
        for (size_t i = 0; i < sa->instantiation_registry->count; i++) {
            GenericInstantiation *inst = &sa->instantiation_registry->instantiations[i];
            free(inst->base_name);
            free(inst->mangled_name);
            for (size_t j = 0; j < inst->type_arg_count; j++) {
                type_free(inst->type_args[j]);
            }
            free(inst->type_args);
            // Note: original_symbol and monomorphized_symbol are owned by symbol table
        }
        free(sa->instantiation_registry->instantiations);
        free(sa->instantiation_registry);
    }
    
    symtable_free(sa->symtable);
    free(sa);
}

static void semantic_error(SemanticAnalyzer *sa, size_t line, size_t column, const char *message) {
    sa->had_error = true;
    error_report_ex(LEVEL_ERROR, NULL, sa->current_filename ? sa->current_filename : "", line, column, message, NULL);
}

static void semantic_error_ex(SemanticAnalyzer *sa, const char *code, size_t line, size_t column, const char *message, const char *suggestion) {
    sa->had_error = true;
    error_report_ex(LEVEL_ERROR, code, sa->current_filename ? sa->current_filename : "", line, column, message, suggestion);
}

// Type comparison
static bool types_equal(Type *a, Type *b) {
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    
    switch (a->kind) {
        case TYPE_PRIMITIVE:
            return a->data.primitive == b->data.primitive;
        case TYPE_POINTER:
            return a->data.pointer.non_null == b->data.pointer.non_null &&
                   types_equal(a->data.pointer.base, b->data.pointer.base);
        case TYPE_ARRAY:
            return a->data.array.size == b->data.array.size &&
                   types_equal(a->data.array.element, b->data.array.element);
        case TYPE_SLICE:
             return types_equal(a->data.slice.element, b->data.slice.element);
        case TYPE_STRUCT:
        case TYPE_ENUM:
            if (!a->data.struct_enum.name || !b->data.struct_enum.name) return false;
            if (strcmp(a->data.struct_enum.name, b->data.struct_enum.name) != 0) return false;
            if (a->data.struct_enum.type_arg_count != b->data.struct_enum.type_arg_count) return false;
            for (size_t i = 0; i < a->data.struct_enum.type_arg_count; i++) {
                if (!types_equal(a->data.struct_enum.type_args[i], b->data.struct_enum.type_args[i])) return false;
            }
            return true;
        case TYPE_FUNCTION:
            if (a->data.function.param_count != b->data.function.param_count) return false;
            if (!types_equal(a->data.function.return_type, b->data.function.return_type)) return false;
            for (size_t i = 0; i < a->data.function.param_count; i++) {
                if (!types_equal(a->data.function.param_types[i], b->data.function.param_types[i])) return false;
            }
            return true;
        case TYPE_RESULT:
            return types_equal(a->data.result.ok_type, b->data.result.ok_type) &&
                   types_equal(a->data.result.err_type, b->data.result.err_type);
    }
    return false;
}

static bool types_compatible(Type *a, Type *b) {
    if (!a || !b) return false;
    
    if (a->kind == TYPE_POINTER && b->kind == TYPE_POINTER) {
        if (a->data.pointer.non_null && !b->data.pointer.non_null) {
            return false;
        }
        if (b->data.pointer.base->kind == TYPE_PRIMITIVE && b->data.pointer.base->data.primitive == TOKEN_VOID) {
            return true;
        }
        return types_equal(a->data.pointer.base, b->data.pointer.base);
    }

    if (a->kind == TYPE_RESULT && b->kind == TYPE_RESULT) {
        // Allow void as bottom type for result matching
        // e.g. result<T, void> is compatible with result<T, E>
        bool ok_compat = types_compatible(a->data.result.ok_type, b->data.result.ok_type);
        if (b->data.result.ok_type->kind == TYPE_PRIMITIVE && b->data.result.ok_type->data.primitive == TOKEN_VOID) {
            ok_compat = true;
        }
        
        bool err_compat = types_compatible(a->data.result.err_type, b->data.result.err_type);
        if (b->data.result.err_type->kind == TYPE_PRIMITIVE && b->data.result.err_type->data.primitive == TOKEN_VOID) {
            err_compat = true;
        }
        
        return ok_compat && err_compat;
    }

    if (is_integer_type(a) && is_integer_type(b)) return true;
    
    return types_equal(a, b);
}

static bool is_numeric_type(Type *type) {
    if (!type || type->kind != TYPE_PRIMITIVE) return false;
    TokenType t = type->data.primitive;
    return (t >= TOKEN_I8 && t <= TOKEN_U64) || t == TOKEN_F32 || t == TOKEN_F64;
}

static bool is_integer_type(Type *type) {
    if (!type || type->kind != TYPE_PRIMITIVE) return false;
    TokenType t = type->data.primitive;
    return t >= TOKEN_I8 && t <= TOKEN_U64;
}

// Expression type checking
static Type *analyze_expr_internal(SemanticAnalyzer *sa, ASTExpr *expr) {
    if (!expr) return NULL;
    
    switch (expr->type) {
        case AST_LITERAL_EXPR: {
            // Infer type from literal
            switch (expr->data.literal.token->type) {
                case TOKEN_INTEGER:
                    return type_create_primitive(TOKEN_I32); // Default to i32
                case TOKEN_FLOAT:
                    return type_create_primitive(TOKEN_F64); // Default to f64
                case TOKEN_TRUE:
                case TOKEN_FALSE:
                    return type_create_primitive(TOKEN_BOOL);
                case TOKEN_STRING:
                    // String literals are []u8 slices
                    return type_create_slice(type_create_primitive(TOKEN_U8));
                case TOKEN_NULL:
                    // Universal null pointer: *void
                    return type_create_pointer(type_create_primitive(TOKEN_VOID), false);
                default:
                    return NULL;
            }
        }
        
        case AST_VARIABLE_EXPR: {
            Symbol *symbol = symtable_lookup(sa->symtable, expr->data.variable.name);
            if (!symbol) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "undefined variable '%s'", expr->data.variable.name);
                semantic_error_ex(sa, "E0002", expr->line, expr->column, error_msg, "check for spelling mistakes or ensure the variable is declared in an accessible scope");
                return NULL;
            }
            return symbol->type;
        }
        
        case AST_BINARY_EXPR: {
            Type *left_type = analyze_expr(sa, expr->data.binary.left);
            Type *right_type = analyze_expr(sa, expr->data.binary.right);
            
            if (!left_type || !right_type) return NULL;
            
            TokenType op = expr->data.binary.op;
            
            // Arithmetic operators
            if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR || 
                op == TOKEN_SLASH || op == TOKEN_PERCENT) {
                
                // Check for pointer arithmetic
                bool is_ptr_arith = false;
                Type *result_type = NULL;
                
                if (left_type->kind == TYPE_POINTER && is_integer_type(right_type)) {
                    if (op == TOKEN_PLUS || op == TOKEN_MINUS) {
                        is_ptr_arith = true;
                        result_type = left_type;
                    }
                } else if (is_integer_type(left_type) && right_type->kind == TYPE_POINTER) {
                    if (op == TOKEN_PLUS) {
                        is_ptr_arith = true;
                        result_type = right_type;
                    }
                } else if (left_type->kind == TYPE_POINTER && right_type->kind == TYPE_POINTER) {
                     // Ptr - Ptr = Int
                     if (op == TOKEN_MINUS) {
                         if (!types_compatible(left_type, right_type)) {
                             semantic_error(sa, expr->line, expr->column, "pointer subtraction requires compatible pointer types");
                             return NULL;
                         }
                         is_ptr_arith = true;
                         result_type = type_create_primitive(TOKEN_I64); // Result is size/offset (long)
                     }
                }
                
                if (is_ptr_arith) {
                    if (!sa->in_unsafe_block) {
                        semantic_error(sa, expr->line, expr->column, "pointer arithmetic requires unsafe block");
                    }
                    sa->current_block_has_unsafe_op = true;
                    return result_type;
                }

                if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                    semantic_error(sa, expr->line, expr->column, "arithmetic operators require numeric operands");
                    return NULL;
                }
                if (!types_compatible(left_type, right_type)) {
                    semantic_error(sa, expr->line, expr->column, "operand types must match");
                    return NULL;
                }
                return left_type;
            }
            
            // Comparison operators
            if (op == TOKEN_LT || op == TOKEN_GT || op == TOKEN_LT_EQ || op == TOKEN_GT_EQ) {
                if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
                    semantic_error(sa, expr->line, expr->column, "comparison operators require numeric operands");
                    return NULL;
                }
                return type_create_primitive(TOKEN_BOOL);
            }
            
            // Equality operators
            if (op == TOKEN_EQ_EQ || op == TOKEN_BANG_EQ) {
                if (!types_compatible(left_type, right_type)) {
                    semantic_error(sa, expr->line, expr->column, "equality comparison requires compatible types");
                    return NULL;
                }
                return type_create_primitive(TOKEN_BOOL);
            }
            
            // Logical operators
            if (op == TOKEN_AMP_AMP || op == TOKEN_PIPE_PIPE) {
                if (left_type->kind != TYPE_PRIMITIVE || left_type->data.primitive != TOKEN_BOOL ||
                    right_type->kind != TYPE_PRIMITIVE || right_type->data.primitive != TOKEN_BOOL) {
                    semantic_error(sa, expr->line, expr->column, "logical operators require bool operands");
                    return NULL;
                }
                return type_create_primitive(TOKEN_BOOL);
            }
            
            // Assignment
            if (op == TOKEN_EQ) {
                if (!types_compatible(left_type, right_type)) {
                    semantic_error_ex(sa, "E0001", expr->line, expr->column, "assignment type mismatch", "ensure the value's type matches the variable's declared type");
                    return NULL;
                }
                
                // Lifetime check: Preventing pointer escape
                // If LHS is a variable, we can check its declared scope.
                if (expr->data.binary.left->type == AST_VARIABLE_EXPR && 
                    left_type->kind == TYPE_POINTER && 
                    right_type->kind == TYPE_POINTER) {
                        
                    Symbol *sym = symtable_lookup(sa->symtable, expr->data.binary.left->data.variable.name);
                    if (sym) {
                        // TODO: Re-implement pointer scope tracking
                        /*
                        int lhs_depth = sym->scope_depth;
                        int rhs_depth = right_type->data.pointer.scope_depth;
                        
                        // Rule: Cannot assign a pointer with deeper scope (shorter lifetime)
                        // to a variable with shallower scope (longer lifetime).
                        if (rhs_depth > lhs_depth) {
                            semantic_error(sa, expr->line, expr->column, "cannot assign pointer to value with shorter lifetime");
                            return NULL;
                        }
                        */
                    }
                }
                
                return left_type;
            }
            
            return NULL;
        }
        
        case AST_UNARY_EXPR: {
            Type *operand_type = analyze_expr(sa, expr->data.unary.operand);
            if (!operand_type) return NULL;
            
            TokenType op = expr->data.unary.op;
            
            if (op == TOKEN_MINUS) {
                if (!is_numeric_type(operand_type)) {
                    semantic_error(sa, expr->line, expr->column, "unary minus requires numeric operand");
                    return NULL;
                }
                return operand_type;
            }
            
            if (op == TOKEN_BANG) {
                if (operand_type->kind != TYPE_PRIMITIVE || operand_type->data.primitive != TOKEN_BOOL) {
                    semantic_error(sa, expr->line, expr->column, "logical not requires bool operand");
                    return NULL;
                }
                return operand_type;
            }
            
            if (op == TOKEN_AMP) {
                // Address-of: returns non-null pointer
                Type *ptr_type = type_create_pointer(operand_type, true);
                
                // Tag with lifetime if it's a variable
                if (expr->data.unary.operand->type == AST_VARIABLE_EXPR) {
                    Symbol *sym = symtable_lookup(sa->symtable, expr->data.unary.operand->data.variable.name);
                    if (sym) {
                        // TODO: Re-implement scope tracking
                        // ptr_type->data.pointer.scope_depth = sym->scope_depth;
                    }
                } else {
                     // TODO: Re-implement scope tracking
                     // ptr_type->data.pointer.scope_depth = sa->scope_depth;
                }
                
                return ptr_type;
            }
            
            if (op == TOKEN_STAR) {
                // Dereference
                if (operand_type->kind != TYPE_POINTER) {
                    semantic_error(sa, expr->line, expr->column, "dereference requires pointer operand");
                    return NULL;
                }
                
                // Check nullable pointer safety
                if (!operand_type->data.pointer.non_null) {
                    if (!sa->in_unsafe_block) {
                        semantic_error(sa, expr->line, expr->column, "dereferencing nullable pointer requires unsafe block");
                        return NULL;
                    }
                    sa->current_block_has_unsafe_op = true;
                }
                
                return operand_type->data.pointer.base;
            }
            
            return NULL;
        }
        
        case AST_CALL_EXPR: {
            // Check for result::ok and result::err special forms
            if (expr->data.call.callee->type == AST_VARIABLE_EXPR) {
                if (strcmp(expr->data.call.callee->data.variable.name, "result::ok") == 0) {
                    if (expr->data.call.arg_count != 1) {
                         semantic_error(sa, expr->line, expr->column, "result::ok expects exactly 1 argument");
                         return NULL;
                    }
                    Type *val_type = analyze_expr(sa, expr->data.call.arguments[0]);
                    if (!val_type) return NULL;
                    
                    // ok(val) -> result<typeof(val), void>
                    return type_create_result(type_clone(val_type), type_create_primitive(TOKEN_VOID));
                } else if (strcmp(expr->data.call.callee->data.variable.name, "result::err") == 0) {
                    if (expr->data.call.arg_count != 1) {
                         semantic_error(sa, expr->line, expr->column, "result::err expects exactly 1 argument");
                         return NULL;
                    }
                    Type *err_type = analyze_expr(sa, expr->data.call.arguments[0]);
                    if (!err_type) return NULL;
                    
                    // err(val) -> result<void, typeof(val)>
                    return type_create_result(type_create_primitive(TOKEN_VOID), type_clone(err_type));
                }
            }

            // Get function type
            Symbol *func_symbol = NULL;
            char *func_name = NULL;
            const char *module_name = NULL;
            
            if (expr->data.call.callee->type == AST_VARIABLE_EXPR) {
                func_name = expr->data.call.callee->data.variable.name;
                func_symbol = symtable_lookup(sa->symtable, func_name);
            } else if (expr->data.call.callee->type == AST_MEMBER_EXPR && !expr->data.call.callee->data.member.is_arrow) {
                // Qualified call: module.func()
                ASTExpr *obj = expr->data.call.callee->data.member.object;
                if (obj->type == AST_VARIABLE_EXPR) {
                    Symbol *mod_sym = symtable_lookup(sa->symtable, obj->data.variable.name);
                    if (mod_sym && mod_sym->kind == SYMBOL_MODULE) {
                        func_name = expr->data.call.callee->data.member.member;
                        module_name = mod_sym->name;
                        func_symbol = symtable_lookup(mod_sym->module_table, func_name);
                        
                        if (func_symbol && !func_symbol->is_public) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg), "function '%s' is private to module '%s'", 
                                    func_name, mod_sym->name);
                            semantic_error(sa, expr->line, expr->column, error_msg);
                            return NULL;
                        }
                    }
                }
            }
            
            if (!func_symbol) {
                semantic_error(sa, expr->line, expr->column, "could not resolve function call");
                return NULL;
            }
            
            if (func_symbol->kind != SYMBOL_FUNCTION) {
                semantic_error(sa, expr->line, expr->column, "not a function");
                return NULL;
            }

            // Check if unsafe block is required
            if (func_symbol->is_extern || func_symbol->is_variadic) {
                // Whitelist known safe standard library intrinsics
                bool is_safe_intrinsic = false;
                const char *name = func_symbol->name;
                
                // std::io::print/println (names might be namespaced or raw depending on import)
                // Also check for 'exit', 'assert'
                if (strcmp(name, "print") == 0 || 
                    strcmp(name, "exit") == 0 || strcmp(name, "assert") == 0 ||
                    strstr(name, "print") != NULL ||
                    (module_name && (strcmp(module_name, "math") == 0 || strcmp(module_name, "std::math") == 0)) ||
                    (module_name && (strcmp(module_name, "result") == 0 || strcmp(module_name, "std::result") == 0)) ||
                    strstr(name, "math") != NULL ||
                    strstr(name, "result") != NULL) {
                     is_safe_intrinsic = true;
                }
                
                if (!is_safe_intrinsic) {
                    if (!sa->in_unsafe_block) {
                        semantic_error(sa, expr->line, expr->column, "call to extern/variadic function requires unsafe block");
                    }
                    sa->current_block_has_unsafe_op = true;
                }
            }
            
            // Check argument count (allow extra args for variadic functions)
            bool is_variadic = false;
            if (func_symbol && func_symbol->kind == SYMBOL_FUNCTION) {
                is_variadic = func_symbol->is_variadic;
            }
            
            Type *func_type = func_symbol->type; // Get the function type here
            
            if (!is_variadic && expr->data.call.arg_count != func_type->data.function.param_count) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "expected %zu arguments, got %zu", 
                        func_type->data.function.param_count, expr->data.call.arg_count);
                semantic_error(sa, expr->line, expr->column, error_msg);
                return NULL;
            }
            
            // For variadic functions, check minimum argument count
            if (is_variadic && expr->data.call.arg_count < func_type->data.function.param_count) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "expected at least %zu arguments, got %zu", 
                        func_type->data.function.param_count, expr->data.call.arg_count);
                semantic_error(sa, expr->line, expr->column, error_msg);
                return NULL;
            }
            
            // Type check arguments
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                analyze_expr(sa, expr->data.call.arguments[i]);
            }
            
            // Handle generics
            if (func_symbol->type_param_count > 0) {
                if (expr->data.call.generic_count == 0) {
                    // Attempt inference
                    Type **inferred = calloc(func_symbol->type_param_count, sizeof(Type*));
                    bool success = true;
                    
                    for (size_t i = 0; i < func_symbol->param_count && i < expr->data.call.arg_count; i++) {
                         // Need access to function param types directly.
                         Type *func_type = func_symbol->type;
                         if (func_type->kind == TYPE_FUNCTION && i < func_type->data.function.param_count) {
                             if (!infer_type(func_type->data.function.param_types[i], expr->data.call.arguments[i]->expr_type, 
                                            func_symbol->type_params, func_symbol->type_param_count, inferred)) {
                                 success = false;
                                 break;
                             }
                         }
                    }
                    
                    // Check if all inferred
                    if (success) {
                        for (size_t k = 0; k < func_symbol->type_param_count; k++) {
                            if (!inferred[k]) {
                                success = false;
                                break;
                            }
                        }
                    }
                    
                    if (success) {
                        // Update AST
                        expr->data.call.generic_count = func_symbol->type_param_count;
                        expr->data.call.generic_args = inferred;
                        // Continue to substitution
                    } else {
                        // Cleanup
                        for(size_t k=0; k<func_symbol->type_param_count; k++) type_free(inferred[k]);
                        free(inferred);
                        
                        semantic_error(sa, expr->line, expr->column, "cannot infer generic type arguments");
                        return NULL;
                    }
                } else if (expr->data.call.generic_count != func_symbol->type_param_count) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg), "expected %zu generic arguments, got %zu", 
                            func_symbol->type_param_count, expr->data.call.generic_count);
                    semantic_error(sa, expr->line, expr->column, error_msg);
                    return NULL;
                }
                
                // Substitute generic type parameters in return type
                // printf("Substituting generics for %s: count=%zu\n", func_symbol->name, func_symbol->type_param_count);
                // for(size_t k=0; k<func_symbol->type_param_count; k++) printf("Param %zu: %s\n", k, func_symbol->type_params[k]);
                // for(size_t k=0; k<expr->data.call.generic_count; k++) printf("Arg %zu: %p\n", k, (void*)expr->data.call.generic_args[k]);
                
                return type_substitute(func_symbol->type->data.function.return_type, func_symbol->type_params, 
                                      expr->data.call.generic_args, expr->data.call.generic_count);
            } else if (expr->data.call.generic_count > 0) {
                 semantic_error(sa, expr->line, expr->column, "function is not generic but generic arguments provided");
                 return NULL;
            }
            
            return func_symbol->type->data.function.return_type; // Return type
        }
        
        case AST_INDEX_EXPR: {
            Type *array_type = analyze_expr(sa, expr->data.index.array);
            Type *index_type = analyze_expr(sa, expr->data.index.index);
            
            if (!array_type || !index_type) return NULL;
            
            if (array_type->kind != TYPE_ARRAY && array_type->kind != TYPE_POINTER && array_type->kind != TYPE_SLICE) {
                semantic_error(sa, expr->line, expr->column, "indexing requires array, slice, or pointer");
                return NULL;
            }
            
            if (!is_integer_type(index_type)) {
                semantic_error(sa, expr->line, expr->column, "array index must be integer");
                return NULL;
            }
            
            // Array bounds checking for constant indices
            if (array_type->kind == TYPE_ARRAY && expr->data.index.index->type == AST_LITERAL_EXPR) {
                if (expr->data.index.index->data.literal.token->type == TOKEN_INTEGER) {
                    size_t index = expr->data.index.index->data.literal.token->value.int_value;
                    if (index >= array_type->data.array.size) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg), "array index %zu out of bounds (size %zu)", 
                                index, array_type->data.array.size);
                        semantic_error(sa, expr->line, expr->column, error_msg);
                    }
                }
            }
            
            if (array_type->kind == TYPE_ARRAY) {
                return array_type->data.array.element;
            } else if (array_type->kind == TYPE_SLICE) {
                return array_type->data.slice.element;
            } else {
                return array_type->data.pointer.base;
            }
        }
        

        
        case AST_SLICE_EXPR: {
            Type *array_type = analyze_expr(sa, expr->data.slice.array);
            
            if (expr->data.slice.start) {
                Type *start_type = analyze_expr(sa, expr->data.slice.start);
                if (!is_integer_type(start_type)) {
                    semantic_error(sa, expr->line, expr->column, "slice start index must be integer");
                }
            }
            
            if (expr->data.slice.end) {
                Type *end_type = analyze_expr(sa, expr->data.slice.end);
                if (!is_integer_type(end_type)) {
                    semantic_error(sa, expr->line, expr->column, "slice end index must be integer");
                }
            }
            
            if (!array_type) return NULL;
            
            Type *elem_type = NULL;
            if (array_type->kind == TYPE_ARRAY) {
                elem_type = array_type->data.array.element;
            } else if (array_type->kind == TYPE_POINTER) {
                elem_type = array_type->data.pointer.base;
            } else if (array_type->kind == TYPE_SLICE) {
                elem_type = array_type->data.slice.element;
            } else {
                semantic_error(sa, expr->line, expr->column, "slicing requires array, slice, or pointer");
                return NULL;
            }
            
            return type_create_slice(type_clone(elem_type));
        }

        case AST_MEMBER_EXPR: {
            // First check if it's a module access (e.g., math.add)
            if (expr->data.member.object->type == AST_VARIABLE_EXPR && !expr->data.member.is_arrow) {
                Symbol *sym = symtable_lookup(sa->symtable, expr->data.member.object->data.variable.name);
                if (sym && sym->kind == SYMBOL_MODULE) {
                    Symbol *member_sym = symtable_lookup(sym->module_table, expr->data.member.member);
                    if (!member_sym) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg), "module '%s' has no member '%s'", sym->name, expr->data.member.member);
                        semantic_error(sa, expr->line, expr->column, error_msg);
                        return NULL;
                    }
                    
                    // Enforce visibility
                    if (!member_sym->is_public) {
                        char error_msg[256];
                        snprintf(error_msg, sizeof(error_msg), "member '%s' of module '%s' is private", expr->data.member.member, sym->name);
                        semantic_error(sa, expr->line, expr->column, error_msg);
                        return NULL;
                    }
                    
                    return member_sym->type;
                }
            }

            Type *object_type = analyze_expr(sa, expr->data.member.object);
            if (!object_type) return NULL;
            
            // Handle pointer member access (->)
            if (expr->data.member.is_arrow) {
                if (object_type->kind != TYPE_POINTER) {
                    semantic_error(sa, expr->line, expr->column, "arrow operator requires pointer type");
                    return NULL;
                }
                object_type = object_type->data.pointer.base;
            }
            
            // Handle slice members
            if (object_type->kind == TYPE_SLICE) {
                 if (strcmp(expr->data.member.member, "len") == 0) {
                      return type_create_primitive(TOKEN_I64);
                 }
                 if (strcmp(expr->data.member.member, "data") == 0) {
                      return type_create_pointer(type_clone(object_type->data.slice.element), false);
                 }
                 char error_msg[256];
                 snprintf(error_msg, sizeof(error_msg), "slice has no member '%s'", expr->data.member.member);
                 semantic_error(sa, expr->line, expr->column, error_msg);
                 return NULL;
            }
            
            // Check if it's a struct type
            if (object_type->kind != TYPE_STRUCT) {
                semantic_error(sa, expr->line, expr->column, "member access requires struct type");
                return NULL;
            }
            
            // Look up struct definition to validate field
            Symbol *struct_symbol = find_type_symbol(sa, object_type->data.struct_enum.name);
            if (!struct_symbol || struct_symbol->kind != SYMBOL_TYPE) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "undefined struct '%s'", object_type->data.struct_enum.name);
                semantic_error(sa, expr->line, expr->column, error_msg);
                return NULL;
            }
            
            // Find field in struct
            for (size_t i = 0; i < struct_symbol->field_count; i++) {
                if (strcmp(struct_symbol->fields[i].name, expr->data.member.member) == 0) {
                    return struct_symbol->fields[i].type;
                }
            }
            
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "struct '%s' has no member '%s'", object_type->data.struct_enum.name, expr->data.member.member);
            semantic_error(sa, expr->line, expr->column, error_msg);
            return NULL;
        }

        case AST_CAST_EXPR: {
            resolve_type(sa, expr->data.cast.target_type);
            Type *expr_type = analyze_expr(sa, expr->data.cast.expr);
            if (!expr_type) return NULL;
            return type_clone(expr->data.cast.target_type);
        }
        
        default:
            return NULL;
    }
}

static Type *analyze_expr(SemanticAnalyzer *sa, ASTExpr *expr) {
    Type *type = analyze_expr_internal(sa, expr);
    
    if (expr && type) {
        // Only clone if it's different from what we already have
        // (Avoiding use-after-free if it was already set)
        if (expr->expr_type != type) {
            if (expr->expr_type) type_free(expr->expr_type);
            expr->expr_type = type_clone(type);
        }
    }
    
    return type;
}

// Statement analysis
static void analyze_stmt(SemanticAnalyzer *sa, ASTStmt *stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case AST_EXPR_STMT:
            analyze_expr(sa, stmt->data.expr_stmt.expr);
            break;
            
        case AST_VAR_DECL_STMT: {
            // Check for duplicate in current scope
            Symbol *existing = symtable_lookup_current(sa->symtable, stmt->data.var_decl.name);
            if (existing) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "redefinition of '%s'", stmt->data.var_decl.name);
                semantic_error_ex(sa, "E0006", stmt->line, stmt->column, error_msg, "variable names must be unique within the same scope; consider a different name");
                return;
            }
            
            // Type check initializer
            resolve_type(sa, stmt->data.var_decl.var_type);
            
            if (stmt->data.var_decl.initializer) {
                Type *init_type = analyze_expr(sa, stmt->data.var_decl.initializer);
                if (init_type && !types_compatible(stmt->data.var_decl.var_type, init_type)) {
                    semantic_error_ex(sa, "E0001", stmt->data.var_decl.initializer->line, stmt->data.var_decl.initializer->column, "initializer type mismatch", "ensure the value's type matches the variable's declared type");
                }
            }
            
            // Add to symbol table
            Symbol *symbol = symbol_create(stmt->data.var_decl.name, SYMBOL_VARIABLE, 
                                          stmt->data.var_decl.var_type, stmt->line, stmt->column);
            symbol->is_const = stmt->data.var_decl.is_const;
            symbol->is_initialized = stmt->data.var_decl.initializer != NULL;
            symbol->scope_depth = sa->scope_depth;
            symtable_insert(sa->symtable, symbol);
            break;
        }
        
        case AST_IF_STMT: {
            Type *cond_type = analyze_expr(sa, stmt->data.if_stmt.condition);
            if (cond_type && (cond_type->kind != TYPE_PRIMITIVE || cond_type->data.primitive != TOKEN_BOOL)) {
                semantic_error(sa, stmt->line, stmt->column, "if condition must be bool");
            }
            
            analyze_stmt(sa, stmt->data.if_stmt.then_branch);
            if (stmt->data.if_stmt.else_branch) {
                analyze_stmt(sa, stmt->data.if_stmt.else_branch);
            }
            break;
        }
        
        case AST_WHILE_STMT: {
            Type *cond_type = analyze_expr(sa, stmt->data.while_stmt.condition);
            if (cond_type && (cond_type->kind != TYPE_PRIMITIVE || cond_type->data.primitive != TOKEN_BOOL)) {
                semantic_error(sa, stmt->line, stmt->column, "while condition must be bool");
            }
            
            sa->loop_depth++;
            analyze_stmt(sa, stmt->data.while_stmt.body);
            sa->loop_depth--;
            break;
        }
        
        case AST_FOR_STMT:
            analyze_stmt(sa, stmt->data.for_stmt.initializer);
            if (stmt->data.for_stmt.condition) {
                Type *cond_type = analyze_expr(sa, stmt->data.for_stmt.condition);
                if (cond_type && (cond_type->kind != TYPE_PRIMITIVE || cond_type->data.primitive != TOKEN_BOOL)) {
                    semantic_error(sa, stmt->line, stmt->column, "for condition must be bool");
                }
            }
            analyze_expr(sa, stmt->data.for_stmt.increment);
            sa->loop_depth++;
            analyze_stmt(sa, stmt->data.for_stmt.body);
            sa->loop_depth--;
            break;
            
        case AST_RETURN_STMT: {
            Type *return_type = NULL;
            if (stmt->data.return_stmt.value) {
                return_type = analyze_expr(sa, stmt->data.return_stmt.value);
            }
            
            // Check against function return type
            if (sa->current_function_return_type) {
                bool is_void = sa->current_function_return_type->kind == TYPE_PRIMITIVE &&
                              sa->current_function_return_type->data.primitive == TOKEN_VOID;
                
                if (is_void && return_type) {
                    semantic_error(sa, stmt->line, stmt->column, "void function cannot return a value");
                } else if (!is_void && !return_type) {
                    semantic_error(sa, stmt->line, stmt->column, "non-void function must return a value");
                } else if (!is_void && return_type && !types_compatible(sa->current_function_return_type, return_type)) {
                    char *expected = type_to_string(sa->current_function_return_type);
                    char *actual = type_to_string(return_type);
                    char error_msg[512];
                    snprintf(error_msg, sizeof(error_msg), "return type mismatch: expected '%s', got '%s'", expected, actual);
                    semantic_error_ex(sa, "E0001", stmt->line, stmt->column, error_msg, "ensure the returned value matches the function's return type");
                    free(expected);
                    free(actual);
                }
                
                // TODO: Re-implement pointer scope tracking
                /*
                // Escape analysis: Check if returning pointer to local
                if (return_type && return_type->kind == TYPE_POINTER) {
                    if (return_type->data.pointer.scope_depth > 0) {
                        semantic_error(sa, stmt->line, stmt->column, "cannot return pointer to local stack variable");
                    }
                }
                */
            }
            break;
        }
        
        case AST_BLOCK_STMT: {
            symtable_enter_scope(sa->symtable);
            sa->scope_depth++;
            bool unreachable = false;
            for (size_t i = 0; i < stmt->data.block.stmt_count; i++) {
                if (unreachable) {
                    semantic_error_ex(sa, "E0004", stmt->data.block.statements[i]->line, 
                                 stmt->data.block.statements[i]->column, 
                                 "unreachable code detected", "this code will never be executed");
                    break;
                }
                analyze_stmt(sa, stmt->data.block.statements[i]);
                // Mark as unreachable if current statement guarantees return/exit
                if (check_return_paths(stmt->data.block.statements[i])) {
                    unreachable = true;
                }
            }
            sa->scope_depth--;
            symtable_exit_scope(sa->symtable);
            break;
        }

        case AST_MATCH_STMT: {
            Type *expr_type = analyze_expr(sa, stmt->data.match_stmt.expr);
            if (!expr_type) return;

            if (expr_type->kind == TYPE_RESULT) {
                bool seen_ok = false;
                bool seen_err = false;
    
                for (size_t i = 0; i < stmt->data.match_stmt.case_count; i++) {
                    ASTMatchCase *cse = &stmt->data.match_stmt.cases[i];
                    
                    // Validate tag
                    if (strcmp(cse->pattern_tag, "ok") == 0) {
                        seen_ok = true;
                    } else if (strcmp(cse->pattern_tag, "err") == 0) {
                        seen_err = true;
                    } else {
                         char msg[256];
                         snprintf(msg, sizeof(msg), "invalid pattern tag '%s' for result", cse->pattern_tag);
                         semantic_error(sa, stmt->line, stmt->column, msg);
                    }
    
                    symtable_enter_scope(sa->symtable);
                    
                    // Bind capture
                    if (cse->capture_name) {
                        Type *cap_type = NULL;
                        if (strcmp(cse->pattern_tag, "ok") == 0) cap_type = expr_type->data.result.ok_type;
                        else if (strcmp(cse->pattern_tag, "err") == 0) cap_type = expr_type->data.result.err_type;
                        
                        if (cap_type) {
                            Symbol *sym = symbol_create(cse->capture_name, SYMBOL_VARIABLE, type_clone(cap_type), stmt->line, stmt->column);
                            sym->is_initialized = true;
                            symtable_insert(sa->symtable, sym);
                        }
                    }
                    
                    analyze_stmt(sa, cse->body);
                    symtable_exit_scope(sa->symtable);
                }
                
                if (!seen_ok || !seen_err) {
                     semantic_error(sa, stmt->line, stmt->column, "non-exhaustive patterns: result match must handle 'ok' and 'err'");
                }
            } else if (expr_type->kind == TYPE_ENUM) {
                // Enum match
                Symbol *enum_sym = symtable_lookup(sa->symtable, expr_type->data.struct_enum.name);
                if (!enum_sym || enum_sym->kind != SYMBOL_TYPE) {
                    semantic_error(sa, stmt->line, stmt->column, "unknown enum type in match");
                    return;
                }
                
                bool *covered = calloc(enum_sym->variant_count, sizeof(bool));
                bool has_wildcard = false;
                
                for (size_t i = 0; i < stmt->data.match_stmt.case_count; i++) {
                    ASTMatchCase *cse = &stmt->data.match_stmt.cases[i];
                    
                    if (strcmp(cse->pattern_tag, "_") == 0) {
                        has_wildcard = true;
                    } else {
                        // Validate tag
                        bool found = false;
                        for (size_t k = 0; k < enum_sym->variant_count; k++) {
                            if (strcmp(enum_sym->variants[k], cse->pattern_tag) == 0) {
                                found = true;
                                covered[k] = true;
                                break;
                            }
                        }
                        if (!found) {
                             char msg[256];
                             snprintf(msg, sizeof(msg), "invalid pattern variant '%s' for enum '%s'", cse->pattern_tag, expr_type->data.struct_enum.name);
                             semantic_error(sa, stmt->line, stmt->column, msg);
                        }
                    }
                    
                    // Scope and body analysis
                    symtable_enter_scope(sa->symtable);
                    analyze_stmt(sa, cse->body);
                    symtable_exit_scope(sa->symtable);
                }
                
                // Exhaustiveness check
                if (!has_wildcard) {
                    for (size_t k = 0; k < enum_sym->variant_count; k++) {
                        if (!covered[k]) {
                             char msg[256];
                             snprintf(msg, sizeof(msg), "non-exhaustive patterns: enum variant '%s' not covered", enum_sym->variants[k]);
                             semantic_error(sa, stmt->line, stmt->column, msg);
                             break;
                        }
                    }
                }
                free(covered);
            } else {
                semantic_error(sa, stmt->line, stmt->column, "match expression must be a result or enum type");
            }
            break;
        }
            
        case AST_FAIL_STMT: {
            if (stmt->data.fail_stmt.message) {
                analyze_expr(sa, stmt->data.fail_stmt.message);
                // Ideally check if it's a string, but for now allow any expression or rely on runtime printing
            }
            break;
        }

        case AST_UNSAFE_STMT: {
            bool previous_unsafe = sa->in_unsafe_block;
            bool previous_has_op = sa->current_block_has_unsafe_op;
            
            if (previous_unsafe) {
                 // Already in unsafe block - nested unsafe is technically redundant
                 // But we check for operations anyway
            }
            
            sa->in_unsafe_block = true;
            sa->current_block_has_unsafe_op = false;
            
            analyze_stmt(sa, stmt->data.unsafe_stmt.body);
            
            if (!sa->current_block_has_unsafe_op) {
                if (sa->strict_unsafe_mode) {
                    semantic_error(sa, stmt->line, stmt->column, "unnecessary unsafe block (strict mode)");
                } else {
                    // Warning
                    fprintf(stderr, "warning: unnecessary unsafe block\n  --> :%zu:%zu\n", stmt->line, stmt->column);
                }
            }
            
            sa->in_unsafe_block = previous_unsafe;
            // Propagate up: if inner block had unsafe op, then outer block effectively had one too
            if (sa->current_block_has_unsafe_op) {
                sa->current_block_has_unsafe_op = true; 
            } else {
                sa->current_block_has_unsafe_op = previous_has_op;
            }
            break;
        }

        case AST_BREAK_STMT:
            if (sa->loop_depth == 0) {
                semantic_error(sa, stmt->line, stmt->column, "break statement outside of loop");
            }
            break;
            
        case AST_CONTINUE_STMT:
            if (sa->loop_depth == 0) {
                semantic_error(sa, stmt->line, stmt->column, "continue statement outside of loop");
            }
            break;
            
        default:
            break;
    }
}

// Helper for type inference
static bool infer_type(Type *param_type, Type *arg_type, char **params, size_t count, Type **inferred) {
    if (!param_type || !arg_type) return false;
    
    // If param is a generic placeholder (struct with param name)
    if (param_type->kind == TYPE_STRUCT || param_type->kind == TYPE_ENUM) {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(param_type->data.struct_enum.name, params[i]) == 0) {
                // Found generic param.
                if (inferred[i] != NULL) {
                    // Already inferred, check compatibility
                    return types_compatible(inferred[i], arg_type);
                }
                inferred[i] = type_clone(arg_type);
                return true;
            }
        }
    }
    
    // Recurse for composite types
    if (param_type->kind == TYPE_POINTER && arg_type->kind == TYPE_POINTER) {
        return infer_type(param_type->data.pointer.base, arg_type->data.pointer.base, params, count, inferred);
    }
    
    if (param_type->kind == TYPE_ARRAY && arg_type->kind == TYPE_ARRAY) {
        return infer_type(param_type->data.array.element, arg_type->data.array.element, params, count, inferred);
    }

    if (param_type->kind == TYPE_RESULT && arg_type->kind == TYPE_RESULT) {
        if (!infer_type(param_type->data.result.ok_type, arg_type->data.result.ok_type, params, count, inferred)) return false;
        return infer_type(param_type->data.result.err_type, arg_type->data.result.err_type, params, count, inferred);
    }
    
    // Function types... simplified for now, skip
    
    return true; // Match non-generic structure
}

// Generate mangled name for generic instantiation (e.g., "Pair_i32_i64")
static char *generate_mangled_name(const char *base_name, Type **type_args, size_t type_arg_count) {
    if (type_arg_count == 0) {
        return strdup(base_name);
    }
    
    // Calculate required buffer size
    size_t size = strlen(base_name) + 1;
    for (size_t i = 0; i < type_arg_count; i++) {
        char *type_str = type_to_string(type_args[i]);
        size += strlen(type_str) + 1; // +1 for underscore
        free(type_str);
    }
    
    char *mangled = malloc(size);
    strcpy(mangled, base_name);
    
    for (size_t i = 0; i < type_arg_count; i++) {
        strcat(mangled, "_");
        char *type_str = type_to_string(type_args[i]);
        strcat(mangled, type_str);
        free(type_str);
    }
    
    return mangled;
}

// Check if a generic instantiation already exists
static GenericInstantiation *find_instantiation(InstantiationRegistry *registry, const char *base_name, 
                                                 Type **type_args, size_t type_arg_count) {
    for (size_t i = 0; i < registry->count; i++) {
        GenericInstantiation *inst = &registry->instantiations[i];
        if (strcmp(inst->base_name, base_name) != 0) continue;
        if (inst->type_arg_count != type_arg_count) continue;
        
        bool match = true;
        for (size_t j = 0; j < type_arg_count; j++) {
            if (!types_equal(inst->type_args[j], type_args[j])) {
                match = false;
                break;
            }
        }
        
        if (match) return inst;
    }
    
    return NULL;
}

// Register a new generic instantiation
static GenericInstantiation *register_instantiation(SemanticAnalyzer *sa, const char *base_name,
                                                     Type **type_args, size_t type_arg_count,
                                                     Symbol *original_symbol) {
    InstantiationRegistry *registry = sa->instantiation_registry;
    
    // Check if already exists
    GenericInstantiation *existing = find_instantiation(registry, base_name, type_args, type_arg_count);
    if (existing) return existing;
    
    // Expand registry if needed
    if (registry->count >= registry->capacity) {
        registry->capacity = registry->capacity == 0 ? 8 : registry->capacity * 2;
        registry->instantiations = realloc(registry->instantiations, 
                                          sizeof(GenericInstantiation) * registry->capacity);
    }
    
    // Create new instantiation
    GenericInstantiation *inst = &registry->instantiations[registry->count++];
    inst->base_name = strdup(base_name);
    inst->type_arg_count = type_arg_count;
    inst->type_args = malloc(sizeof(Type*) * type_arg_count);
    for (size_t i = 0; i < type_arg_count; i++) {
        inst->type_args[i] = type_clone(type_args[i]);
    }
    inst->mangled_name = generate_mangled_name(base_name, type_args, type_arg_count);
    inst->original_symbol = original_symbol;
    inst->monomorphized_symbol = NULL; // Will be created later
    
    return inst;
}

static void resolve_type(SemanticAnalyzer *sa, Type *type) {
    if (!type) return;
    
    switch (type->kind) {
        case TYPE_POINTER:
            resolve_type(sa, type->data.pointer.base);
            break;
        case TYPE_ARRAY:
            resolve_type(sa, type->data.array.element);
            break;
        case TYPE_FUNCTION:
            resolve_type(sa, type->data.function.return_type);
            for (size_t i = 0; i < type->data.function.param_count; i++) {
                resolve_type(sa, type->data.function.param_types[i]);
            }
            break;
        case TYPE_STRUCT:
        case TYPE_ENUM: {
            // Look up type symbol (handles qualified names too)
            Symbol *sym = find_type_symbol(sa, type->data.struct_enum.name);
            if (sym && sym->kind == SYMBOL_TYPE) {
                // Update the type name to the mangled one found in the symbol
                // but ONLY if it's different to avoid redundant free/strdup
                if (sym->type->data.struct_enum.name && 
                    strcmp(type->data.struct_enum.name, sym->type->data.struct_enum.name) != 0) {
                    free(type->data.struct_enum.name);
                    type->data.struct_enum.name = strdup(sym->type->data.struct_enum.name);
                }
            }
            
            if (sym && sym->kind == SYMBOL_TYPE && sym->type->kind == TYPE_ENUM && type->kind == TYPE_STRUCT) {
                // Convert to ENUM
                type->kind = TYPE_ENUM;
            }
            
            // Resolve generic arguments first
            for (size_t i = 0; i < type->data.struct_enum.type_arg_count; i++) {
                resolve_type(sa, type->data.struct_enum.type_args[i]);
            }
            
            // Handle generic instantiation
            if (type->data.struct_enum.type_arg_count > 0 && sym && sym->kind == SYMBOL_TYPE) {
                // Validate type argument count
                if (sym->type_param_count != type->data.struct_enum.type_arg_count) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg), 
                            "type '%s' expects %zu type arguments, got %zu",
                            type->data.struct_enum.name, sym->type_param_count, 
                            type->data.struct_enum.type_arg_count);
                    semantic_error(sa, 0, 0, error_msg);
                    return;
                }
                
                // Register instantiation
                GenericInstantiation *inst = register_instantiation(sa, type->data.struct_enum.name,
                                                                    type->data.struct_enum.type_args,
                                                                    type->data.struct_enum.type_arg_count,
                                                                    sym);
                
                // Create monomorphized symbol if not already done
                if (!inst->monomorphized_symbol) {
                    if (type->kind == TYPE_STRUCT) {
                        // Create monomorphized struct
                        Symbol *mono_sym = symbol_create(inst->mangled_name, SYMBOL_TYPE,
                                                        type_create_struct(inst->mangled_name, NULL, 0),
                                                        sym->line, sym->column);
                        mono_sym->is_public = sym->is_public;
                        mono_sym->field_count = sym->field_count;
                        mono_sym->fields = malloc(sizeof(StructField) * mono_sym->field_count);
                        
                        // Substitute type parameters in fields
                        for (size_t i = 0; i < sym->field_count; i++) {
                            mono_sym->fields[i].name = strdup(sym->fields[i].name);
                            mono_sym->fields[i].type = type_substitute(sym->fields[i].type,
                                                                       sym->type_params,
                                                                       inst->type_args,
                                                                       inst->type_arg_count);
                        }
                        
                        inst->monomorphized_symbol = mono_sym;
                        
                        // Insert into global scope (not current scope)
                        // Save current scope and temporarily switch to global
                        Scope *saved_scope = sa->symtable->current_scope;
                        sa->symtable->current_scope = sa->symtable->global_scope;
                        symtable_insert(sa->symtable, mono_sym);
                        sa->symtable->current_scope = saved_scope;
                    } else if (type->kind == TYPE_ENUM) {
                        // Create monomorphized enum
                        Symbol *mono_sym = symbol_create(inst->mangled_name, SYMBOL_TYPE,
                                                        type_create_enum(inst->mangled_name, NULL, 0),
                                                        sym->line, sym->column);
                        mono_sym->is_public = sym->is_public;
                        mono_sym->variant_count = sym->variant_count;
                        mono_sym->variants = malloc(sizeof(char*) * mono_sym->variant_count);
                        
                        // Copy variants (enums don't have associated data yet)
                        for (size_t i = 0; i < sym->variant_count; i++) {
                            mono_sym->variants[i] = strdup(sym->variants[i]);
                        }
                        
                        inst->monomorphized_symbol = mono_sym;
                        
                        // Insert into global scope (not current scope)
                        Scope *saved_scope = sa->symtable->current_scope;
                        sa->symtable->current_scope = sa->symtable->global_scope;
                        symtable_insert(sa->symtable, mono_sym);
                        sa->symtable->current_scope = saved_scope;
                    }
                }
                
                // Update type name to use mangled name
                free(type->data.struct_enum.name);
                type->data.struct_enum.name = strdup(inst->mangled_name);
            }
            break;
        }
        default:
            break;
    }
}

// Main analysis function
bool semantic_analyze(SemanticAnalyzer *sa, ASTProgram *program) {
    if (!sa || !program) return false;
    
    // Pass 1: Declarations
    if (!semantic_analyze_declarations(sa, program)) return false;
    
    // Pass 2: Bodies
    if (!semantic_analyze_bodies(sa, program)) return false;
    
    return !sa->had_error;
}

// Helper to check if a statement guarantees a return
static bool check_return_paths(ASTStmt *stmt) {
    if (!stmt) return false;
    
    switch (stmt->type) {
        case AST_RETURN_STMT:
        case AST_FAIL_STMT:
            return true;
            
        case AST_BLOCK_STMT: {
            // A block returns if any of its statements returns (and is reachable)
            // But strict reaching check: if stmt returns, next ones are unreachable.
            // For now, simpler check: find *some* returning statement.
            for (size_t i = 0; i < stmt->data.block.stmt_count; i++) {
                if (check_return_paths(stmt->data.block.statements[i])) {
                    return true;
                }
            }
            return false;
        }
        
        case AST_IF_STMT: {
            // Must return in BOTH branches
            if (!stmt->data.if_stmt.else_branch) return false;
            return check_return_paths(stmt->data.if_stmt.then_branch) &&
                   check_return_paths(stmt->data.if_stmt.else_branch);
        }
        
        // Loops do not guarantee return in general analysis without const folding
        case AST_WHILE_STMT:
        case AST_FOR_STMT:
            return false;
            
        case AST_MATCH_STMT: {
            // Must return in ALL cases
            if (stmt->data.match_stmt.case_count == 0) return false;
            
            for (size_t i = 0; i < stmt->data.match_stmt.case_count; i++) {
                if (!check_return_paths(stmt->data.match_stmt.cases[i].body)) {
                    return false;
                }
            }
            return true;
        }
            
        case AST_UNSAFE_STMT:
             return check_return_paths(stmt->data.unsafe_stmt.body);
             
        default:
            return false;
    }
}

bool semantic_analyze_declarations(SemanticAnalyzer *sa, ASTProgram *program) {
    if (!sa || !program) return false;
    
    // Pass 1: Forward-declare all types (structs and enums)
    for (size_t i = 0; i < program->decl_count; i++) {
        ASTDecl *decl = program->declarations[i];
        
        if (decl->type == AST_STRUCT_DECL) {
            char mangled_name[512];
            if (sa->symtable->name) {
                snprintf(mangled_name, 512, "%s__%s", sa->symtable->name, decl->data.struct_decl.name);
                for (char *p = mangled_name; p < mangled_name + strlen(sa->symtable->name); p++) {
                    if (*p == '.') *p = '_';
                    if (*p == ':') *p = '_';
                }
            } else {
                strncpy(mangled_name, decl->data.struct_decl.name, 511);
                mangled_name[511] = '\0';
            }
            
            Symbol *struct_symbol = symbol_create(decl->data.struct_decl.name, SYMBOL_TYPE,
                                                 type_create_struct(mangled_name, NULL, 0),
                                                 decl->line, decl->column);
            struct_symbol->is_public = decl->data.struct_decl.is_public;
            struct_symbol->is_packed = decl->data.struct_decl.is_packed;
            
            if (!symtable_insert(sa->symtable, struct_symbol)) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "duplicate declaration of struct '%s'", decl->data.struct_decl.name);
                semantic_error(sa, decl->line, decl->column, error_msg);
                continue;
            }
            
            if (strcmp(decl->data.struct_decl.name, mangled_name) != 0) {
                Symbol *mangled_symbol = symbol_create(mangled_name, SYMBOL_TYPE,
                                                     type_create_struct(mangled_name, NULL, 0),
                                                     decl->line, decl->column);
                mangled_symbol->is_public = decl->data.struct_decl.is_public;
                mangled_symbol->is_packed = decl->data.struct_decl.is_packed;
                symtable_insert(sa->symtable, mangled_symbol);
            }
        } 
        else if (decl->type == AST_ENUM_DECL) {
            char mangled_name[512];
            if (sa->symtable->name) {
                snprintf(mangled_name, 512, "%s__%s", sa->symtable->name, decl->data.enum_decl.name);
                for (char *p = mangled_name; p < mangled_name + strlen(sa->symtable->name); p++) {
                    if (*p == '.') *p = '_';
                    if (*p == ':') *p = '_';
                }
            } else {
                strncpy(mangled_name, decl->data.enum_decl.name, 511);
                mangled_name[511] = '\0';
            }
            
            Symbol *enum_symbol = symbol_create(decl->data.enum_decl.name, SYMBOL_TYPE,
                                               type_create_enum(mangled_name, NULL, 0),
                                               decl->line, decl->column);
            enum_symbol->is_public = decl->data.enum_decl.is_public;
            
            if (!symtable_insert(sa->symtable, enum_symbol)) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "duplicate declaration of enum '%s'", decl->data.enum_decl.name);
                semantic_error(sa, decl->line, decl->column, error_msg);
                continue;
            }
            
            if (strcmp(decl->data.enum_decl.name, mangled_name) != 0) {
                Symbol *mangled_symbol = symbol_create(mangled_name, SYMBOL_TYPE,
                                                     type_create_enum(mangled_name, NULL, 0),
                                                     decl->line, decl->column);
                mangled_symbol->is_public = decl->data.enum_decl.is_public;
                symtable_insert(sa->symtable, mangled_symbol);
            }
        }
    }
    
    // Pass 2: Populate fields/variants (can now refer to types from Pass 1)
    for (size_t i = 0; i < program->decl_count; i++) {
        ASTDecl *decl = program->declarations[i];
        
        if (decl->type == AST_STRUCT_DECL) {
            Symbol *struct_symbol = symtable_lookup_current(sa->symtable, decl->data.struct_decl.name);
            if (!struct_symbol) continue;
            
            if (decl->data.struct_decl.type_param_count > 0) {
                struct_symbol->type_param_count = decl->data.struct_decl.type_param_count;
                struct_symbol->type_params = malloc(sizeof(char*) * struct_symbol->type_param_count);
                for (size_t k = 0; k < struct_symbol->type_param_count; k++) {
                    struct_symbol->type_params[k] = strdup(decl->data.struct_decl.type_params[k]);
                }
            }
            
            struct_symbol->field_count = decl->data.struct_decl.field_count;
            struct_symbol->fields = malloc(sizeof(StructField) * struct_symbol->field_count);
            for (size_t j = 0; j < struct_symbol->field_count; j++) {
                resolve_type(sa, decl->data.struct_decl.fields[j].field_type);
                struct_symbol->fields[j].name = strdup(decl->data.struct_decl.fields[j].name);
                struct_symbol->fields[j].type = type_clone(decl->data.struct_decl.fields[j].field_type);
            }
            
            // Sync with mangled symbol if exists
            char mangled_name[512];
            if (sa->symtable->name) {
                snprintf(mangled_name, 512, "%s__%s", sa->symtable->name, decl->data.struct_decl.name);
                for (char *p = mangled_name; p < mangled_name + strlen(sa->symtable->name); p++) {
                    if (*p == '.') *p = '_';
                    if (*p == ':') *p = '_';
                }
            } else {
                strncpy(mangled_name, decl->data.struct_decl.name, 511);
                mangled_name[511] = '\0';
            }
            if (strcmp(decl->data.struct_decl.name, mangled_name) != 0) {
                Symbol *mangled_symbol = symtable_lookup_current(sa->symtable, mangled_name);
                if (mangled_symbol) {
                    mangled_symbol->field_count = struct_symbol->field_count;
                    mangled_symbol->fields = malloc(sizeof(StructField) * mangled_symbol->field_count);
                    for (size_t j = 0; j < mangled_symbol->field_count; j++) {
                        mangled_symbol->fields[j].name = strdup(struct_symbol->fields[j].name);
                        mangled_symbol->fields[j].type = type_clone(struct_symbol->fields[j].type);
                    }
                }
            }
        } 
        else if (decl->type == AST_ENUM_DECL) {
            Symbol *enum_symbol = symtable_lookup_current(sa->symtable, decl->data.enum_decl.name);
            if (!enum_symbol) continue;
            
            if (decl->data.enum_decl.type_param_count > 0) {
                enum_symbol->type_param_count = decl->data.enum_decl.type_param_count;
                enum_symbol->type_params = malloc(sizeof(char*) * enum_symbol->type_param_count);
                for (size_t k = 0; k < enum_symbol->type_param_count; k++) {
                    enum_symbol->type_params[k] = strdup(decl->data.enum_decl.type_params[k]);
                }
            }
            
            enum_symbol->variant_count = decl->data.enum_decl.variant_count;
            enum_symbol->variants = malloc(sizeof(char*) * enum_symbol->variant_count);
            for (size_t k = 0; k < decl->data.enum_decl.variant_count; k++) {
                char *variant_name = decl->data.enum_decl.variants[k].name;
                enum_symbol->variants[k] = strdup(variant_name);
                Symbol *variant_sym = symbol_create(variant_name, SYMBOL_CONSTANT, type_clone(enum_symbol->type), decl->line, decl->column);
                variant_sym->is_initialized = true;
                variant_sym->is_public = decl->data.enum_decl.is_public;
                variant_sym->enum_value = k;
                symtable_insert(sa->symtable, variant_sym);
            }
            
            // Sync with mangled symbol
            char mangled_name[512];
            if (sa->symtable->name) {
                snprintf(mangled_name, 512, "%s__%s", sa->symtable->name, decl->data.enum_decl.name);
                for (char *p = mangled_name; p < mangled_name + strlen(sa->symtable->name); p++) {
                    if (*p == '.') *p = '_';
                    if (*p == ':') *p = '_';
                }
            } else {
                strncpy(mangled_name, decl->data.enum_decl.name, 511);
                mangled_name[511] = '\0';
            }
            if (strcmp(decl->data.enum_decl.name, mangled_name) != 0) {
                Symbol *mangled_symbol = symtable_lookup_current(sa->symtable, mangled_name);
                if (mangled_symbol) {
                    mangled_symbol->variant_count = enum_symbol->variant_count;
                    mangled_symbol->variants = malloc(sizeof(char*) * mangled_symbol->variant_count);
                    for (size_t k = 0; k < mangled_symbol->variant_count; k++) {
                        mangled_symbol->variants[k] = strdup(enum_symbol->variants[k]);
                    }
                }
            }
        }
    }
    
    // Pass 3: Resolve functions and variables
    for (size_t i = 0; i < program->decl_count; i++) {
        ASTDecl *decl = program->declarations[i];
        
        if (decl->type == AST_FUNCTION_DECL) {
            Symbol *existing = symtable_lookup_current(sa->symtable, decl->data.function.name);
            if (existing) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "duplicate declaration of function '%s'", decl->data.function.name);
                semantic_error(sa, decl->line, decl->column, error_msg);
                continue;
            }
            
            resolve_type(sa, decl->data.function.return_type);
            for (size_t k = 0; k < decl->data.function.param_count; k++) {
                resolve_type(sa, decl->data.function.params[k].param_type);
            }
            
            Type **param_types = malloc(sizeof(Type*) * decl->data.function.param_count);
            for (size_t k = 0; k < decl->data.function.param_count; k++) {
                param_types[k] = type_clone(decl->data.function.params[k].param_type);
            }
            Type *func_type = type_create_function(type_clone(decl->data.function.return_type), param_types, decl->data.function.param_count);
            Symbol *func_symbol = symbol_create(decl->data.function.name, SYMBOL_FUNCTION, func_type, decl->line, decl->column);
            func_symbol->param_count = decl->data.function.param_count;
            func_symbol->is_public = decl->data.function.is_public;
            func_symbol->is_extern = decl->data.function.is_extern;
            func_symbol->is_variadic = decl->data.function.is_variadic;
            
            if (decl->data.function.type_param_count > 0) {
                func_symbol->type_param_count = decl->data.function.type_param_count;
                func_symbol->type_params = malloc(sizeof(char*) * func_symbol->type_param_count);
                for (size_t k = 0; k < func_symbol->type_param_count; k++) {
                    func_symbol->type_params[k] = strdup(decl->data.function.type_params[k]);
                }
            }
            symtable_insert(sa->symtable, func_symbol);
        } 
        else if (decl->type == AST_VAR_DECL_STMT) {
            ASTGlobalVarDecl *var = &decl->data.var_decl;
            
            Symbol *existing = symtable_lookup_current(sa->symtable, var->name);
            if (existing) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "duplicate declaration of variable '%s'", var->name);
                semantic_error(sa, decl->line, decl->column, error_msg);
                continue;
            }
            
            resolve_type(sa, var->var_type);
            Symbol *var_symbol = symbol_create(var->name, SYMBOL_VARIABLE, type_clone(var->var_type), decl->line, decl->column);
            var_symbol->is_public = var->is_public;
            var_symbol->is_const = var->is_const;
            symtable_insert(sa->symtable, var_symbol);
        }
    }
    
    return !sa->had_error;    return !sa->had_error;
}

bool semantic_analyze_bodies(SemanticAnalyzer *sa, ASTProgram *program) {
    if (!sa || !program) return false;

    // Pass: Analyze function bodies
    for (size_t i = 0; i < program->decl_count; i++) {
        ASTDecl *decl = program->declarations[i];
        
        if (decl->type == AST_FUNCTION_DECL && decl->data.function.body) {
            // Analyze function body
            symtable_enter_scope(sa->symtable);
            
            // Add parameters to scope
            for (size_t j = 0; j < decl->data.function.param_count; j++) {
                Symbol *param = symbol_create(decl->data.function.params[j].name, SYMBOL_VARIABLE,
                                             decl->data.function.params[j].param_type, decl->line, decl->column);
                param->is_initialized = true;
                symtable_insert(sa->symtable, param);
            }
            
            // Set current function return type
            Type *prev_return_type = sa->current_function_return_type;
            sa->current_function_return_type = decl->data.function.return_type;
            
            // Analyze body
            analyze_stmt(sa, decl->data.function.body);
            
            // Check return paths if not void
            Type *ret_type = decl->data.function.return_type;
            if (ret_type && ret_type->kind == TYPE_PRIMITIVE && ret_type->data.primitive == TOKEN_VOID) {
                // Void functions don't need return
            } else {
                 if (!check_return_paths(decl->data.function.body)) {
                     semantic_error_ex(sa, "E0003", decl->line, decl->column, "missing return statement in non-void function", "all execution paths must return a value");
                 }
            }
            
            // Restore previous return type
            sa->current_function_return_type = prev_return_type;
            
            symtable_exit_scope(sa->symtable);
        } else if (decl->type == AST_VAR_DECL_STMT && decl->data.var_decl.initializer) {
            analyze_expr(sa, decl->data.var_decl.initializer);
            if (decl->data.var_decl.initializer->expr_type) {
                if (!types_compatible(decl->data.var_decl.var_type, decl->data.var_decl.initializer->expr_type)) {
                    char error_msg[256];
                    char *t1 = type_to_string(decl->data.var_decl.var_type);
                    char *t2 = type_to_string(decl->data.var_decl.initializer->expr_type);
                    snprintf(error_msg, sizeof(error_msg), "global variable initializer type mismatch: expected '%s', got '%s'", t1, t2);
                    free(t1);
                    free(t2);
                    semantic_error_ex(sa, "E0001", decl->line, decl->column, error_msg, "global constants/variables must be initialized with compatible types");
                }
            }
        }
    }
    
    return !sa->had_error && error_count() == 0;
}

static Symbol *find_type_symbol(SemanticAnalyzer *sa, const char *name) {
    if (!name) return NULL;
    
    // 0. Handle qualified name (Module.Type)
    if (strchr(name, '.')) {
        char name_copy[512];
        strncpy(name_copy, name, 511);
        name_copy[511] = '\0';
        char *dot = strchr(name_copy, '.');
        if (dot) {
            *dot = '\0';
            char *module_name = name_copy;
            char *type_name = dot + 1;
            
            Symbol *mod_sym = symtable_lookup(sa->symtable, module_name);
            if (mod_sym && mod_sym->kind == SYMBOL_MODULE && mod_sym->module_table) {
                Symbol *found = symtable_lookup(mod_sym->module_table, type_name);
                if (found && found->kind == SYMBOL_TYPE) return found;
            }
        }
    }
    
    // 1. Direct lookup in current symbol table (includes parents/globals)
    Symbol *sym = symtable_lookup(sa->symtable, name);
    if (sym && sym->kind == SYMBOL_TYPE) {
        return sym;
    }
    
    // 2. Search in all imported modules
    Scope *global_scope = sa->symtable->global_scope;
    for (size_t i = 0; i < global_scope->symbol_count; i++) {
        Symbol *s = global_scope->symbols[i];
        if (s->kind == SYMBOL_MODULE && s->module_table) {
            Symbol *found = symtable_lookup(s->module_table, name);
            if (found && found->kind == SYMBOL_TYPE) {
                return found;
            }
        }
    }
    
    return NULL;
}
