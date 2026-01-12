#ifndef IR_H
#define IR_H

#include <stddef.h>
#include <stdbool.h>

// IR Instruction opcodes
typedef enum {
    // Arithmetic
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    
    // Comparison
    IR_EQ,
    IR_NE,
    IR_LT,
    IR_LE,
    IR_GT,
    IR_GE,
    
    // Logical
    IR_AND,
    IR_OR,
    IR_NOT,
    
    // Memory
    IR_LOAD,        // Load from variable
    IR_STORE,       // Store to variable
    IR_ALLOCA,      // Stack allocation
    
    // Control flow
    IR_LABEL,       // Label for jumps
    IR_JUMP,        // Unconditional jump
    IR_BRANCH,      // Conditional branch (if src1 then src2 else dest)
    IR_FAIL,
    
    // Function calls
    IR_CALL,        // Function call
    IR_RETURN,      // Return from function
    
    // Misc
    IR_MOVE,        // Move/copy value
    IR_NEG,         // Unary negation
    IR_ADDR,        // Address of (&)
    IR_DEREF        // Dereference (*)
} IROpcode;

// IR Operand types
typedef enum {
    IR_OP_TEMP,      // Temporary variable (t0, t1, ...)
    IR_OP_CONST,     // Constant value
    IR_OP_VAR,       // Named variable
    IR_OP_LABEL,     // Jump label
    IR_OP_STRING     // String literal
} IROperandKind;

// IR Operand
typedef struct {
    IROperandKind kind;
    union {
        int temp_id;
        long const_value;
        char *var_name;
        char *label_name;
        char *string_value;
    } data;
} IROperand;

// IR Instruction
typedef struct {
    IROpcode opcode;
    IROperand *dest;      // Destination (can be NULL)
    IROperand *src1;      // First source (can be NULL)
    IROperand *src2;      // Second source (can be NULL)
    IROperand **args;     // Additional arguments (for calls)
    size_t arg_count;
} IRInstruction;

// IR Function
typedef struct {
    char *name;
    char **params;      // Parameter names
    char **param_types; // Parameter types (C string representation)
    size_t param_count;
    char *return_type;  // Return type (C string representation)
    char **local_vars;  // Local variable names
    char **local_var_types; // Local variable types (C string representation)
    size_t local_var_count;
    IRInstruction **instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    char **temp_types;  // Type of each temporary
    size_t temp_count;  // Number of temporaries used
    size_t label_count; // Number of labels used
} IRFunction;

// IR Global Variable
typedef struct {
    char *name;
    char *c_type;
    long init_value; // For now only simple integer initialization supported
} IRGlobal;

// IR Module
typedef struct {
    IRFunction **functions;
    size_t function_count;
    size_t function_capacity;
    IRGlobal **globals;
    size_t global_count;
    size_t global_capacity;
} IRModule;

// IR Operand creation
IROperand *ir_operand_temp(int temp_id);
IROperand *ir_operand_const(long value);
IROperand *ir_operand_var(const char *name);
IROperand *ir_operand_label(const char *name);
IROperand *ir_operand_string(const char *value);
void ir_operand_free(IROperand *op);
IROperand *ir_operand_clone(IROperand *op);

// IR Instruction creation
IRInstruction *ir_instruction_create(IROpcode opcode, IROperand *dest, IROperand *src1, IROperand *src2);
IRInstruction *ir_instruction_create_call(IROperand *dest, IROperand *func, IROperand **args, size_t arg_count);
void ir_instruction_free(IRInstruction *instr);

// IR Function creation
IRFunction *ir_function_create(const char *name);
void ir_function_add_instruction(IRFunction *func, IRInstruction *instr);
void ir_function_free(IRFunction *func);

// IR Module creation
IRModule *ir_module_create(void);
void ir_module_add_function(IRModule *module, IRFunction *func);
void ir_module_add_global(IRModule *module, const char *name, const char *c_type, long init_value);
void ir_module_free(IRModule *module);

// IR Printing
void ir_operand_print(IROperand *op);
void ir_instruction_print(IRInstruction *instr);
void ir_function_print(IRFunction *func);
void ir_module_print(IRModule *module);

// Opcode name
const char *ir_opcode_name(IROpcode opcode);

#endif // IR_H
