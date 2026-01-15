#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdbool.h>
#include "token.h"
#include "type.h"

// Forward declarations
typedef struct ASTNode ASTNode;
typedef struct ASTExpr ASTExpr;
typedef struct ASTStmt ASTStmt;
typedef struct ASTDecl ASTDecl;

// AST node types
typedef enum {
    // Expressions
    AST_LITERAL_EXPR,
    AST_VARIABLE_EXPR,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_INDEX_EXPR,
    AST_SLICE_EXPR,
    AST_MEMBER_EXPR,
    AST_CAST_EXPR,
    
    // Statements
    AST_EXPR_STMT,
    AST_VAR_DECL_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_RETURN_STMT,
    AST_BLOCK_STMT,
    AST_MATCH_STMT,
    AST_FAIL_STMT,
    AST_UNSAFE_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    
    // Declarations
    AST_FUNCTION_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_MODULE_DECL,
    AST_IMPORT_DECL,
    
    // Program
    AST_PROGRAM,
} ASTNodeType;

// Literal expression
typedef struct {
    Token *token;
    union {
        long long int_value;
        double float_value;
        char *string_value;
        bool bool_value;
    } value;
} ASTLiteralExpr;

// Variable expression
typedef struct {
    char *name;
} ASTVariableExpr;

// Binary expression
typedef struct {
    TokenType op;
    ASTExpr *left;
    ASTExpr *right;
} ASTBinaryExpr;

// Unary expression
typedef struct {
    TokenType op;
    ASTExpr *operand;
} ASTUnaryExpr;

// Call expression
typedef struct {
    ASTExpr *callee;
    ASTExpr **arguments;
    size_t arg_count;
    Type **generic_args;    // Generic type arguments (e.g. <i32>)
    size_t generic_count;
} ASTCallExpr;

// Index expression (array[index])
typedef struct {
    ASTExpr *array;
    ASTExpr *index;
} ASTIndexExpr;

// Slice expression (array[start..end])
typedef struct {
    ASTExpr *array;
    ASTExpr *start;
    ASTExpr *end;
} ASTSliceExpr;

// Member expression (struct.field or ptr->field)
typedef struct {
    ASTExpr *object;
    char *member;
    bool is_arrow;  // true for ->, false for .
} ASTMemberExpr;

typedef struct {
    Type *target_type;
    ASTExpr *expr;
} ASTCastExpr;

// Expression node
struct ASTExpr {
    ASTNodeType type;
    size_t line;
    size_t column;
    Type *expr_type;  // Result type after semantic analysis
    union {
        ASTLiteralExpr literal;
        ASTVariableExpr variable;
        ASTBinaryExpr binary;
        ASTUnaryExpr unary;
        ASTCallExpr call;
        ASTIndexExpr index;
        ASTSliceExpr slice;
        ASTMemberExpr member;
        ASTCastExpr cast;
    } data;
};

// Expression statement
typedef struct {
    ASTExpr *expr;
} ASTExprStmt;

// Variable declaration statement
typedef struct {
    bool is_const;
    Type *var_type;
    char *name;
    ASTExpr *initializer;
} ASTVarDeclStmt;

// If statement
typedef struct {
    ASTExpr *condition;
    ASTStmt *then_branch;
    ASTStmt *else_branch;  // NULL if no else
} ASTIfStmt;

// While statement
typedef struct {
    ASTExpr *condition;
    ASTStmt *body;
} ASTWhileStmt;

// For statement
typedef struct {
    ASTStmt *initializer;  // Can be var decl or expr stmt
    ASTExpr *condition;
    ASTExpr *increment;
    ASTStmt *body;
} ASTForStmt;

// Return statement
typedef struct {
    ASTExpr *value;  // NULL for void return
} ASTReturnStmt;

// Block statement
typedef struct {
    ASTStmt **statements;
    size_t stmt_count;
} ASTBlockStmt;

// Match case pattern: Pattern(Capture) => Body
typedef struct {
    char *pattern_tag;   // e.g. "ok" or "err"
    char *capture_name;  // e.g. "val" (NULL if no capture)
    ASTStmt *body;
} ASTMatchCase;

// Match statement
typedef struct {
    ASTExpr *expr;
    ASTMatchCase *cases;
    size_t case_count;
} ASTMatchStmt;

typedef struct {
    ASTExpr *message; // Optional expression (usually string literal)
} ASTFailStmt;

typedef struct {
    ASTStmt *body;
} ASTUnsafeStmt;

typedef struct {
    char _dummy; // Dummy member to avoid empty struct warning
} ASTBreakStmt;

typedef struct {
    char _dummy; // Dummy member to avoid empty struct warning
} ASTContinueStmt;

// Statement node
struct ASTStmt {
    ASTNodeType type;
    size_t line;
    size_t column;
    union {
        ASTExprStmt expr_stmt;
        ASTVarDeclStmt var_decl;
        ASTIfStmt if_stmt;
        ASTWhileStmt while_stmt;
        ASTForStmt for_stmt;
        ASTReturnStmt return_stmt;
        ASTBlockStmt block;
        ASTMatchStmt match_stmt;
        ASTFailStmt fail_stmt;
        ASTUnsafeStmt unsafe_stmt;
        ASTBreakStmt break_stmt;
        ASTContinueStmt continue_stmt;
    } data;
};

// Function parameter
typedef struct {
    Type *param_type;
    char *name;
    size_t line;
    size_t column;
} ASTParam;

// Function declaration
typedef struct {
    char *name;
    char **type_params;      // Generic type parameters (e.g., "T", "U")
    size_t type_param_count;
    ASTParam *params;
    size_t param_count;
    Type *return_type;
    ASTStmt *body;  // Block statement (NULL for extern functions)
    bool is_variadic; // For C interop (printf, etc.)
    bool is_public;
    bool is_extern;  // True for extern functions
} ASTFunctionDecl;

// Struct field
typedef struct {
    Type *field_type;
    char *name;
} ASTField;

// Struct declaration
typedef struct {
    char *name;
    char **type_params;
    size_t type_param_count;
    ASTField *fields;
    size_t field_count;
    bool is_public;
    bool is_packed;
} ASTStructDecl;

// Enum variant
typedef struct {
    char *name;
} ASTEnumVariant;

// Enum declaration
typedef struct {
    char *name;
    char **type_params;
    size_t type_param_count;
    ASTEnumVariant *variants;
    size_t variant_count;
    bool is_public;
} ASTEnumDecl;

// Module declaration
typedef struct {
    char *module_name;  // Module file name (e.g., "math.vx")
} ASTModuleDecl;

// Import declaration
typedef struct {
    char *import_path;  // Import file path (e.g., "math.vx")
    char *alias;        // Optional alias (NULL if no alias)
} ASTImportDecl;

// Global variable declaration
typedef struct {
    bool is_const;
    bool is_public;
    Type *var_type;
    char *name;
    ASTExpr *initializer;
} ASTGlobalVarDecl;

// Declaration node
struct ASTDecl {
    ASTNodeType type;
    size_t line;
    size_t column;
    union {
        ASTFunctionDecl function;
        ASTStructDecl struct_decl;
        ASTEnumDecl enum_decl;
        ASTModuleDecl module_decl;
        ASTImportDecl import_decl;
        ASTGlobalVarDecl var_decl;
    } data;
};

// Program (root node)
typedef struct {
    char *module_name;          // Module name if declared (NULL otherwise)
    ASTImportDecl **imports;    // Import declarations
    size_t import_count;
    ASTDecl **declarations;     // Function, struct, enum declarations
    size_t decl_count;
} ASTProgram;

// Main AST node (can be expr, stmt, decl, or program)
struct ASTNode {
    ASTNodeType type;
    size_t line;
    size_t column;
    union {
        ASTExpr expr;
        ASTStmt stmt;
        ASTDecl decl;
        ASTProgram program;
    } data;
};

// AST creation functions
ASTExpr *ast_create_literal(Token *token);
ASTExpr *ast_create_variable(const char *name, size_t line, size_t column);
ASTExpr *ast_create_binary(TokenType op, ASTExpr *left, ASTExpr *right, size_t line, size_t column);
ASTExpr *ast_create_unary(TokenType op, ASTExpr *operand, size_t line, size_t column);
ASTExpr *ast_create_call(ASTExpr *callee, ASTExpr **args, size_t arg_count, Type **generic_args, size_t generic_count, size_t line, size_t column);
ASTExpr *ast_create_index(ASTExpr *array, ASTExpr *index, size_t line, size_t column);
ASTExpr *ast_create_slice_expr(ASTExpr *array, ASTExpr *start, ASTExpr *end, size_t line, size_t column);
ASTExpr *ast_create_member(ASTExpr *object, const char *member, bool is_arrow, size_t line, size_t column);
ASTExpr *ast_create_cast(Type *target_type, ASTExpr *expr, size_t line, size_t column);

ASTStmt *ast_create_expr_stmt(ASTExpr *expr, size_t line, size_t column);
ASTStmt *ast_create_var_decl(bool is_const, Type *type, const char *name, ASTExpr *init, size_t line, size_t column);
ASTStmt *ast_create_if(ASTExpr *cond, ASTStmt *then_branch, ASTStmt *else_branch, size_t line, size_t column);
ASTStmt *ast_create_while(ASTExpr *cond, ASTStmt *body, size_t line, size_t column);
ASTStmt *ast_create_for(ASTStmt *init, ASTExpr *cond, ASTExpr *inc, ASTStmt *body, size_t line, size_t column);
ASTStmt *ast_create_return(ASTExpr *value, size_t line, size_t column);
ASTStmt *ast_create_block(ASTStmt **stmts, size_t count, size_t line, size_t column);
ASTStmt *ast_create_match(ASTExpr *expr, ASTMatchCase *cases, size_t case_count, size_t line, size_t column);
ASTStmt *ast_create_fail(ASTExpr *message, size_t line, size_t column);
ASTStmt *ast_create_unsafe(ASTStmt *body, size_t line, size_t column);
ASTStmt *ast_create_break(size_t line, size_t column);
ASTStmt *ast_create_continue(size_t line, size_t column);

ASTDecl *ast_create_function(const char *name, char **type_params, size_t type_param_count, ASTParam *params, size_t param_count, Type *ret_type, ASTStmt *body, bool is_public, bool is_extern, bool is_variadic, size_t line, size_t column);
ASTDecl *ast_create_struct(const char *name, char **type_params, size_t type_param_count, ASTField *fields, size_t field_count, bool is_public, bool is_packed, size_t line, size_t column);
ASTDecl *ast_create_enum(const char *name, char **type_params, size_t type_param_count, ASTEnumVariant *variants, size_t variant_count, bool is_public, size_t line, size_t column);
ASTDecl *ast_create_module(const char *module_name, size_t line, size_t column);
ASTDecl *ast_create_import(const char *import_path, const char *alias, size_t line, size_t column);
ASTDecl *ast_create_variable_decl(const char *name, Type *type, ASTExpr *initializer, bool is_const, bool is_public, size_t line, size_t column);

ASTProgram *ast_create_program(const char *module_name, ASTImportDecl **imports, size_t import_count, ASTDecl **decls, size_t count);

// AST destruction
void ast_free_expr(ASTExpr *expr);
void ast_free_stmt(ASTStmt *stmt);
void ast_free_decl(ASTDecl *decl);
void ast_free_program(ASTProgram *program);

// AST printing (for debugging)
void ast_print_expr(const ASTExpr *expr, int indent);
void ast_print_stmt(const ASTStmt *stmt, int indent);
void ast_print_decl(const ASTDecl *decl, int indent);
void ast_print_program(const ASTProgram *program);

#endif // AST_H
