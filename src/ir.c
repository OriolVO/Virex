#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ir.h"

// Operand creation
IROperand *ir_operand_temp(int temp_id) {
    IROperand *op = malloc(sizeof(IROperand));
    op->kind = IR_OP_TEMP;
    op->data.temp_id = temp_id;
    return op;
}

IROperand *ir_operand_const(long value) {
    IROperand *op = malloc(sizeof(IROperand));
    op->kind = IR_OP_CONST;
    op->data.const_value = value;
    return op;
}

IROperand *ir_operand_float(double value) {
    IROperand *op = malloc(sizeof(IROperand));
    op->kind = IR_OP_FLOAT;
    op->data.float_value = value;
    return op;
}

IROperand *ir_operand_var(const char *name) {
    IROperand *op = malloc(sizeof(IROperand));
    op->kind = IR_OP_VAR;
    op->data.var_name = strdup(name);
    return op;
}

IROperand *ir_operand_label(const char *name) {
    IROperand *op = malloc(sizeof(IROperand));
    op->kind = IR_OP_LABEL;
    op->data.label_name = strdup(name);
    return op;
}

IROperand *ir_operand_string(const char *value) {
    IROperand *op = malloc(sizeof(IROperand));
    op->kind = IR_OP_STRING;
    op->data.string_value = strdup(value);
    return op;
}

void ir_operand_free(IROperand *op) {
    if (!op) return;
    if (op->kind == IR_OP_VAR) {
        free(op->data.var_name);
    } else if (op->kind == IR_OP_LABEL) {
        free(op->data.label_name);
    } else if (op->kind == IR_OP_STRING) {
        free(op->data.string_value);
    }
    free(op);
}

IROperand *ir_operand_clone(IROperand *op) {
    if (!op) return NULL;
    switch (op->kind) {
        case IR_OP_TEMP: return ir_operand_temp(op->data.temp_id);
        case IR_OP_CONST: return ir_operand_const(op->data.const_value);
        case IR_OP_VAR: return ir_operand_var(op->data.var_name);
        case IR_OP_LABEL: return ir_operand_label(op->data.label_name);
        case IR_OP_STRING: return ir_operand_string(op->data.string_value);
        case IR_OP_FLOAT: return ir_operand_float(op->data.float_value);
        default: return NULL;
    }
}

// Instruction creation
// Instruction creation
IRInstruction *ir_instruction_create(IROpcode opcode, IROperand *dest, IROperand *src1, IROperand *src2) {
    IRInstruction *instr = malloc(sizeof(IRInstruction));
    instr->opcode = opcode;
    instr->dest = dest;
    instr->src1 = src1;
    instr->src2 = src2;
    instr->args = NULL;
    instr->arg_count = 0;
    return instr;
}

IRInstruction *ir_instruction_create_call(IROperand *dest, IROperand *func, IROperand **args, size_t arg_count) {
    IRInstruction *instr = malloc(sizeof(IRInstruction));
    instr->opcode = IR_CALL;
    instr->dest = dest;
    instr->src1 = func;
    instr->src2 = NULL;
    instr->args = args; // Takes ownership
    instr->arg_count = arg_count;
    return instr;
}

void ir_instruction_free(IRInstruction *instr) {
    if (!instr) return;
    ir_operand_free(instr->dest);
    ir_operand_free(instr->src1);
    ir_operand_free(instr->src2);
    
    if (instr->args) {
        for (size_t i = 0; i < instr->arg_count; i++) {
            ir_operand_free(instr->args[i]);
        }
        free(instr->args);
    }
    
    free(instr);
}

// Function creation
IRFunction *ir_function_create(const char *name) {
    IRFunction *func = malloc(sizeof(IRFunction));
    func->name = strdup(name);
    func->params = NULL;
    func->param_types = NULL;
    func->param_count = 0;
    func->return_type = NULL;
    func->local_vars = NULL;
    func->local_var_types = NULL;
    func->local_var_count = 0;
    func->instructions = NULL;
    func->instruction_count = 0;
    func->instruction_capacity = 0;
    func->temp_types = NULL;
    func->temp_count = 0;
    func->label_count = 0;
    return func;
}

void ir_function_add_instruction(IRFunction *func, IRInstruction *instr) {
    if (func->instruction_count >= func->instruction_capacity) {
        func->instruction_capacity = func->instruction_capacity == 0 ? 8 : func->instruction_capacity * 2;
        func->instructions = realloc(func->instructions, sizeof(IRInstruction*) * func->instruction_capacity);
    }
    func->instructions[func->instruction_count++] = instr;
}

void ir_function_free(IRFunction *func) {
    if (!func) return;
    free(func->name);
    
    for (size_t i = 0; i < func->param_count; i++) {
        free(func->params[i]);
        if (func->param_types && func->param_types[i]) free(func->param_types[i]);
    }
    free(func->params);
    if (func->param_types) free(func->param_types);
    if (func->return_type) free(func->return_type);
    
    for (size_t i = 0; i < func->local_var_count; i++) {
        free(func->local_vars[i]);
        if (func->local_var_types && func->local_var_types[i]) {
            free(func->local_var_types[i]);
        }
    }
    free(func->local_vars);
    if (func->local_var_types) free(func->local_var_types);
    
    if (func->temp_types) {
        for (size_t i = 0; i < func->temp_count; i++) {
            if (func->temp_types[i]) free(func->temp_types[i]);
        }
        free(func->temp_types);
    }
    
    for (size_t i = 0; i < func->instruction_count; i++) {
        ir_instruction_free(func->instructions[i]);
    }
    free(func->instructions);
    free(func);
}

// Module creation
IRModule *ir_module_create(void) {
    IRModule *module = malloc(sizeof(IRModule));
    module->functions = NULL;
    module->function_count = 0;
    module->function_capacity = 0;
    module->globals = NULL;
    module->global_count = 0;
    module->global_capacity = 0;
    return module;
}

void ir_module_add_global(IRModule *module, const char *name, const char *c_type, long init_value) {
    if (module->global_count >= module->global_capacity) {
        module->global_capacity = module->global_capacity == 0 ? 4 : module->global_capacity * 2;
        module->globals = realloc(module->globals, sizeof(IRGlobal*) * module->global_capacity);
    }
    
    IRGlobal *global = malloc(sizeof(IRGlobal));
    global->name = strdup(name);
    global->c_type = strdup(c_type);
    global->init_value = init_value;
    
    module->globals[module->global_count++] = global;
}

void ir_module_add_function(IRModule *module, IRFunction *func) {
    if (module->function_count >= module->function_capacity) {
        module->function_capacity = module->function_capacity == 0 ? 4 : module->function_capacity * 2;
        module->functions = realloc(module->functions, sizeof(IRFunction*) * module->function_capacity);
    }
    module->functions[module->function_count++] = func;
}

void ir_module_free(IRModule *module) {
    if (!module) return;
    // printf("Freeing module %p\n", (void*)module);
    for (size_t i = 0; i < module->function_count; i++) {
        ir_function_free(module->functions[i]);
    }
    free(module->functions);
    
    for (size_t i = 0; i < module->global_count; i++) {
        free(module->globals[i]->name);
        free(module->globals[i]->c_type);
        free(module->globals[i]);
    }
    free(module->globals);
    
    free(module);
}

// Printing
void ir_operand_print(IROperand *op) {
    if (!op) {
        printf("null");
        return;
    }
    
    switch (op->kind) {
        case IR_OP_TEMP:
            printf("t%d", op->data.temp_id);
            break;
        case IR_OP_CONST:
            printf("%ld", op->data.const_value);
            break;
        case IR_OP_VAR:
            printf("%s", op->data.var_name);
            break;
        case IR_OP_LABEL:
            printf("%s", op->data.label_name);
            break;
        case IR_OP_STRING:
            printf("\"%s\"", op->data.string_value);
            break;
        case IR_OP_FLOAT:
            printf("%g", op->data.float_value);
            break;
    }
}

const char *ir_opcode_name(IROpcode opcode) {
    switch (opcode) {
        case IR_ADD: return "ADD";
        case IR_SUB: return "SUB";
        case IR_MUL: return "MUL";
        case IR_DIV: return "DIV";
        case IR_MOD: return "MOD";
        case IR_EQ: return "EQ";
        case IR_NE: return "NE";
        case IR_LT: return "LT";
        case IR_LE: return "LE";
        case IR_GT: return "GT";
        case IR_GE: return "GE";
        case IR_AND: return "AND";
        case IR_OR: return "OR";
        case IR_NOT: return "NOT";
        case IR_LOAD: return "LOAD";
        case IR_STORE: return "STORE";
        case IR_ALLOCA: return "ALLOCA";
        case IR_LABEL: return "LABEL";
        case IR_JUMP: return "JUMP";
        case IR_BRANCH: return "BRANCH";
        case IR_FAIL: return "FAIL";
        case IR_CALL: return "CALL";
        case IR_RETURN: return "RETURN";
        case IR_MOVE: return "MOVE";
        case IR_NEG: return "NEG";
        case IR_ADDR: return "ADDR";
        case IR_DEREF: return "DEREF";
        case IR_CAST: return "CAST";
        default: return "UNKNOWN";
    }
}

void ir_instruction_print(IRInstruction *instr) {
    if (!instr) return;
    
    if (instr->opcode == IR_LABEL) {
        // Labels are printed differently
        printf("  ");
        ir_operand_print(instr->src1);
        printf(":\n");
        return;
    }
    
    printf("    ");
    
    // Print destination
    if (instr->dest) {
        ir_operand_print(instr->dest);
        printf(" = ");
    }
    
    // Print opcode
    printf("%s", ir_opcode_name(instr->opcode));
    
    // Print sources
    if (instr->src1) {
        printf(" ");
        ir_operand_print(instr->src1);
    }
    if (instr->src2) {
        printf(", ");
        ir_operand_print(instr->src2);
    }
    
    printf("\n");
}

void ir_function_print(IRFunction *func) {
    printf("\nFunction: %s\n", func->name);
    for (size_t i = 0; i < func->instruction_count; i++) {
        ir_instruction_print(func->instructions[i]);
    }
}

void ir_module_print(IRModule *module) {
    printf("=== IR Module ===\n");
    for (size_t i = 0; i < module->function_count; i++) {
        ir_function_print(module->functions[i]);
    }
    printf("\n");
}
