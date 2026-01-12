#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../include/iropt.h"

struct IROptimizer {
    int dummy; // Placeholder
};

IROptimizer *iropt_create(void) {
    IROptimizer *opt = malloc(sizeof(IROptimizer));
    opt->dummy = 0;
    return opt;
}

void iropt_free(IROptimizer *opt) {
    free(opt);
}

// Helper: Check if operand is constant
static bool is_const(IROperand *op) {
    return op && op->kind == IR_OP_CONST;
}

// Helper: Get constant value
static long get_const(IROperand *op) {
    return op->data.const_value;
}

// Constant folding: Evaluate constant expressions at compile time
void iropt_constant_folding(IRModule *module) {
    if (!module) return;
    
    for (size_t f = 0; f < module->function_count; f++) {
        IRFunction *func = module->functions[f];
        
        for (size_t i = 0; i < func->instruction_count; i++) {
            IRInstruction *instr = func->instructions[i];
            
            // Skip if not a binary operation with constant operands
            if (!instr->src1 || !instr->src2) continue;
            if (!is_const(instr->src1) || !is_const(instr->src2)) continue;
            
            long left = get_const(instr->src1);
            long right = get_const(instr->src2);
            long result = 0;
            bool can_fold = true;
            
            switch (instr->opcode) {
                case IR_ADD: result = left + right; break;
                case IR_SUB: result = left - right; break;
                case IR_MUL: result = left * right; break;
                case IR_DIV: 
                    if (right != 0) result = left / right;
                    else can_fold = false;
                    break;
                case IR_MOD:
                    if (right != 0) result = left % right;
                    else can_fold = false;
                    break;
                case IR_EQ: result = (left == right) ? 1 : 0; break;
                case IR_NE: result = (left != right) ? 1 : 0; break;
                case IR_LT: result = (left < right) ? 1 : 0; break;
                case IR_LE: result = (left <= right) ? 1 : 0; break;
                case IR_GT: result = (left > right) ? 1 : 0; break;
                case IR_GE: result = (left >= right) ? 1 : 0; break;
                case IR_AND: result = (left && right) ? 1 : 0; break;
                case IR_OR: result = (left || right) ? 1 : 0; break;
                default: can_fold = false; break;
            }
            
            if (can_fold) {
                // Replace with MOVE constant
                ir_operand_free(instr->src1);
                ir_operand_free(instr->src2);
                instr->opcode = IR_MOVE;
                instr->src1 = ir_operand_const(result);
                instr->src2 = NULL;
            }
        }
    }
}

// Dead code elimination: Remove unreachable code and unused instructions
void iropt_dead_code_elimination(IRModule *module) {
    if (!module) return;
    
    for (size_t f = 0; f < module->function_count; f++) {
        IRFunction *func = module->functions[f];
        bool *reachable = calloc(func->instruction_count, sizeof(bool));
        
        // Mark all instructions as reachable initially (simplified)
        // A full implementation would do control flow analysis
        for (size_t i = 0; i < func->instruction_count; i++) {
            reachable[i] = true;
            
            // Mark code after unconditional return as unreachable
            if (func->instructions[i]->opcode == IR_RETURN) {
                // Mark subsequent non-label instructions as unreachable
                for (size_t j = i + 1; j < func->instruction_count; j++) {
                    if (func->instructions[j]->opcode == IR_LABEL) {
                        break; // Stop at next label
                    }
                    reachable[j] = false;
                }
            }
        }
        
        // Remove unreachable instructions
        size_t write_idx = 0;
        for (size_t read_idx = 0; read_idx < func->instruction_count; read_idx++) {
            if (reachable[read_idx]) {
                func->instructions[write_idx++] = func->instructions[read_idx];
            } else {
                ir_instruction_free(func->instructions[read_idx]);
            }
        }
        func->instruction_count = write_idx;
        
        free(reachable);
    }
}

// Copy propagation: Replace uses of copied values with the original
void iropt_copy_propagation(IRModule *module) {
    if (!module) return;
    
    for (size_t f = 0; f < module->function_count; f++) {
        IRFunction *func = module->functions[f];
        
        // Simple copy propagation: t1 = t0; use(t1) -> use(t0)
        for (size_t i = 0; i < func->instruction_count; i++) {
            IRInstruction *instr = func->instructions[i];
            
            // Look for MOVE instructions: dest = src
            if (instr->opcode != IR_MOVE) continue;
            if (!instr->dest || !instr->src1) continue;
            if (instr->dest->kind != IR_OP_TEMP) continue;
            if (instr->src1->kind != IR_OP_TEMP) continue;
            
            int dest_temp = instr->dest->data.temp_id;
            int src_temp = instr->src1->data.temp_id;
            
            // Replace uses of dest_temp with src_temp in subsequent instructions
            for (size_t j = i + 1; j < func->instruction_count; j++) {
                IRInstruction *use = func->instructions[j];
                
                // Check src1
                if (use->src1 && use->src1->kind == IR_OP_TEMP && 
                    use->src1->data.temp_id == dest_temp) {
                    use->src1->data.temp_id = src_temp;
                }
                
                // Check src2
                if (use->src2 && use->src2->kind == IR_OP_TEMP && 
                    use->src2->data.temp_id == dest_temp) {
                    use->src2->data.temp_id = src_temp;
                }
                
                // Stop if dest_temp is redefined
                if (use->dest && use->dest->kind == IR_OP_TEMP && 
                    use->dest->data.temp_id == dest_temp) {
                    break;
                }
            }
        }
    }
}

// Run all optimizations
void iropt_optimize(IRModule *module) {
    if (!module) return;
    
    // Run optimization passes
    iropt_constant_folding(module);
    iropt_copy_propagation(module);
    iropt_dead_code_elimination(module);
    
    // Could run multiple iterations for better results
    // For now, single pass is sufficient
}
