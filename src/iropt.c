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
        
        // Remove unreachable instructions and NOPs
        size_t write_idx = 0;
        for (size_t read_idx = 0; read_idx < func->instruction_count; read_idx++) {
            IRInstruction *instr = func->instructions[read_idx]; // temp ptr
            if (reachable[read_idx] && instr->opcode != IR_NOP) {
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

// Helper: Check if operand is loop-invariant (doesn't change in loop)
static bool is_loop_invariant(IROperand *op, IRFunction *func, size_t loop_start, size_t loop_end) {
    if (!op) return true;
    
    // Constants are always invariant
    if (op->kind == IR_OP_CONST) return true;
    
    // Check if temp is modified in loop
    if (op->kind == IR_OP_TEMP) {
        int temp_id = op->data.temp_id;
        for (size_t i = loop_start; i <= loop_end; i++) {
            IRInstruction *instr = func->instructions[i];
            if (instr->dest && instr->dest->kind == IR_OP_TEMP && 
                instr->dest->data.temp_id == temp_id) {
                return false; // Modified in loop
            }
        }
        return true;
    }
    
    // Variables might be modified (conservative)
    if (op->kind == IR_OP_VAR) return false;
    
    return true;
}

// Helper: Check if instruction is loop-invariant
static bool is_instruction_invariant(IRInstruction *instr, IRFunction *func, size_t loop_start, size_t loop_end) {
    // Only hoist pure operations (arithmetic, comparisons)
    switch (instr->opcode) {
        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
        case IR_DIV:
        case IR_MOD:
        case IR_EQ:
        case IR_NE:
        case IR_LT:
        case IR_LE:
        case IR_GT:
        case IR_GE:
        case IR_AND:
        case IR_OR:
        case IR_NOT:
        case IR_NEG:
            break;
        default:
            return false; // Don't hoist memory ops, calls, etc.
    }
    
    // Check if all operands are invariant
    if (!is_loop_invariant(instr->src1, func, loop_start, loop_end)) return false;
    if (!is_loop_invariant(instr->src2, func, loop_start, loop_end)) return false;
    
    return true;
}

// Loop Invariant Code Motion: Hoist invariant expressions outside loops
void iropt_loop_invariant_code_motion(IRModule *module) {
    if (!module) return;
    
    for (size_t f = 0; f < module->function_count; f++) {
        IRFunction *func = module->functions[f];
        
        // Track which instructions to hoist and where
        bool *to_hoist = calloc(func->instruction_count, sizeof(bool));
        size_t *hoist_target = calloc(func->instruction_count, sizeof(size_t));
        
        // Detect loops: look for backward jumps (jump to earlier label)
        for (size_t i = 0; i < func->instruction_count; i++) {
            IRInstruction *instr = func->instructions[i];
            
            // Look for conditional branches that jump backward
            if (instr->opcode == IR_BRANCH && instr->src2 && instr->src2->kind == IR_OP_LABEL) {
                const char *target_label = instr->src2->data.label_name;
                
                // Find the target label (should be before current instruction)
                size_t loop_start = 0;
                bool found_loop = false;
                
                for (size_t j = 0; j < i; j++) {
                    IRInstruction *label_instr = func->instructions[j];
                    if (label_instr->opcode == IR_LABEL && 
                        label_instr->dest && 
                        label_instr->dest->kind == IR_OP_LABEL &&
                        strcmp(label_instr->dest->data.label_name, target_label) == 0) {
                        loop_start = j;
                        found_loop = true;
                        break;
                    }
                }
                
                if (!found_loop) continue;
                
                size_t loop_end = i;
                
                // Find invariant instructions in loop
                for (size_t k = loop_start + 1; k < loop_end; k++) {
                    IRInstruction *loop_instr = func->instructions[k];
                    
                    // Skip if already marked for hoisting
                    if (to_hoist[k]) continue;
                    
                    // Skip labels, jumps, etc.
                    if (!loop_instr->dest) continue;
                    if (loop_instr->dest->kind != IR_OP_TEMP) continue;
                    
                    // Check if this instruction is loop-invariant
                    if (is_instruction_invariant(loop_instr, func, loop_start, loop_end)) {
                        // Mark for hoisting before loop label
                        to_hoist[k] = true;
                        hoist_target[k] = loop_start;
                    }
                }
            }
        }
        
        // Actually move the hoisted instructions
        // Build new instruction array with hoisted instructions moved
        IRInstruction **new_instructions = malloc(func->instruction_count * sizeof(IRInstruction*));
        size_t new_count = 0;
        
        for (size_t i = 0; i < func->instruction_count; i++) {
            // First, insert any instructions that should be hoisted to this position
            for (size_t j = 0; j < func->instruction_count; j++) {
                if (to_hoist[j] && hoist_target[j] == i) {
                    new_instructions[new_count++] = func->instructions[j];
                }
            }
            
            // Then add the current instruction if it wasn't hoisted
            if (!to_hoist[i]) {
                new_instructions[new_count++] = func->instructions[i];
            }
        }
        
        // Replace the instruction array
        free(func->instructions);
        func->instructions = new_instructions;
        func->instruction_count = new_count;
        
        free(to_hoist);
        free(hoist_target);
    }
}

// Run all optimizations
void iropt_optimize(IRModule *module) {
    if (!module) return;
    
    // Run optimization passes
    iropt_constant_folding(module);
    iropt_copy_propagation(module);
    iropt_common_subexpression_elimination(module);  // CSE - eliminate redundant computations
    iropt_loop_invariant_code_motion(module);
    iropt_strength_reduction(module);  // Replace expensive ops with cheaper ones
    iropt_dead_store_elimination_loops(module);  // Remove redundant stores
    iropt_dead_code_elimination(module);
    
    // Could run multiple iterations for better results
    // For now, single pass is sufficient
}

// Common Subexpression Elimination: Reuse already-computed values
void iropt_common_subexpression_elimination(IRModule *module) {
    if (!module) return;
    
    for (size_t f = 0; f < module->function_count; f++) {
        IRFunction *func = module->functions[f];
        
        // For each instruction, check if we've already computed this expression
        for (size_t i = 0; i < func->instruction_count; i++) {
            IRInstruction *instr = func->instructions[i];
            
            // Only optimize pure binary operations
            if (!instr->dest || instr->dest->kind != IR_OP_TEMP) continue;
            if (!instr->src1 || !instr->src2) continue;
            
            // Check if this is a pure operation
            bool is_pure = false;
            switch (instr->opcode) {
                case IR_ADD:
                case IR_SUB:
                case IR_MUL:
                case IR_DIV:
                case IR_MOD:
                case IR_EQ:
                case IR_NE:
                case IR_LT:
                case IR_LE:
                case IR_GT:
                case IR_GE:
                    is_pure = true;
                    break;
                default:
                    break;
            }
            
            if (!is_pure) continue;
            
            // Look backward for a matching expression
            for (size_t j = 0; j < i; j++) {
                IRInstruction *prev = func->instructions[j];
                
                // Must be same opcode
                if (prev->opcode != instr->opcode) continue;
                if (!prev->dest || prev->dest->kind != IR_OP_TEMP) continue;
                if (!prev->src1 || !prev->src2) continue;
                
                // Check if operands match
                bool src1_match = false;
                bool src2_match = false;
                
                // Compare src1
                if (prev->src1->kind == instr->src1->kind) {
                    if (prev->src1->kind == IR_OP_CONST) {
                        src1_match = (prev->src1->data.const_value == instr->src1->data.const_value);
                    } else if (prev->src1->kind == IR_OP_TEMP) {
                        src1_match = (prev->src1->data.temp_id == instr->src1->data.temp_id);
                    } else if (prev->src1->kind == IR_OP_VAR) {
                        src1_match = (strcmp(prev->src1->data.var_name, instr->src1->data.var_name) == 0);
                    }
                }
                
                // Compare src2
                if (prev->src2->kind == instr->src2->kind) {
                    if (prev->src2->kind == IR_OP_CONST) {
                        src2_match = (prev->src2->data.const_value == instr->src2->data.const_value);
                    } else if (prev->src2->kind == IR_OP_TEMP) {
                        src2_match = (prev->src2->data.temp_id == instr->src2->data.temp_id);
                    } else if (prev->src2->kind == IR_OP_VAR) {
                        src2_match = (strcmp(prev->src2->data.var_name, instr->src2->data.var_name) == 0);
                    }
                }
                
                if (src1_match && src2_match) {
                    // Check that the previous result hasn't been redefined
                    int prev_temp = prev->dest->data.temp_id;
                    bool redefined = false;
                    
                    for (size_t k = j + 1; k < i; k++) {
                        IRInstruction *between = func->instructions[k];
                        if (between->dest && between->dest->kind == IR_OP_TEMP &&
                            between->dest->data.temp_id == prev_temp) {
                            redefined = true;
                            break;
                        }
                    }
                    
                    if (!redefined) {
                        // Replace current instruction with MOVE from previous result
                        ir_operand_free(instr->src1);
                        ir_operand_free(instr->src2);
                        instr->opcode = IR_MOVE;
                        instr->src1 = ir_operand_temp(prev_temp);
                        instr->src2 = NULL;
                        break; // Found a match, stop searching
                    }
                }
            }
        }
    }
}

// Strength Reduction: Replace expensive operations with cheaper ones in loops
void iropt_strength_reduction(IRModule *module) {
    if (!module) return;
    
    for (size_t f = 0; f < module->function_count; f++) {
        IRFunction *func = module->functions[f];
        
        for (size_t i = 0; i < func->instruction_count; i++) {
            IRInstruction *instr = func->instructions[i];
            
            // Look for multiplication: t = var * const
            // or t = const * var
            if (instr->opcode == IR_MUL && instr->dest && instr->dest->kind == IR_OP_TEMP) {
                IROperand *const_op = NULL;
                IROperand *var_op = NULL;
                
                if (instr->src2 && instr->src2->kind == IR_OP_CONST) {
                    const_op = instr->src2;
                    var_op = instr->src1;
                } else if (instr->src1 && instr->src1->kind == IR_OP_CONST) {
                    const_op = instr->src1;
                    var_op = instr->src2;
                }
                
                if (const_op && var_op) {
                    long val = const_op->data.const_value;
                    
                    if (val == 0) {
                        // x * 0 = 0
                        // Replace with MOVE 0
                        ir_operand_free(instr->src1);
                        ir_operand_free(instr->src2);
                        instr->opcode = IR_MOVE;
                        instr->src1 = ir_operand_const(0);
                        instr->src2 = NULL;
                    } else if (val == 1) {
                         // x * 1 = x
                         // Replace with MOVE var
                         // Need to copy var_op because we are freeing src1/src2
                         IROperand *new_src = NULL;
                         if (var_op->kind == IR_OP_TEMP) new_src = ir_operand_temp(var_op->data.temp_id);
                         else if (var_op->kind == IR_OP_VAR) new_src = ir_operand_var(var_op->data.var_name);
                         else if (var_op->kind == IR_OP_CONST) new_src = ir_operand_const(var_op->data.const_value);
                         
                         ir_operand_free(instr->src1);
                         ir_operand_free(instr->src2);
                         instr->opcode = IR_MOVE;
                         instr->src1 = new_src;
                         instr->src2 = NULL;
                    } else {
                        // Check if power of 2
                        // x * 2^k = x << k
                        // Only for positive values to be safe with shifts
                        if (val > 0 && (val & (val - 1)) == 0) {
                             int shift = 0;
                             while ((val >> shift) > 1) shift++;
                             
                             // We don't have SHIFT opcode yet in IR! 
                             // IR_ADD x, x if val is 2 is simpler and supported.
                             if (val == 2) {
                                 // Replace with ADD x, x
                                 // Make copies of var_op
                                 IROperand *op1 = NULL;
                                 if (var_op->kind == IR_OP_TEMP) op1 = ir_operand_temp(var_op->data.temp_id);
                                 else if (var_op->kind == IR_OP_VAR) op1 = ir_operand_var(var_op->data.var_name);
                                 
                                 if (op1) {
                                     ir_operand_free(instr->src1);
                                     ir_operand_free(instr->src2);
                                     instr->opcode = IR_ADD;
                                     instr->src1 = op1;
                                     // Duplicate for src2
                                     if (op1->kind == IR_OP_TEMP) instr->src2 = ir_operand_temp(op1->data.temp_id);
                                     else if (op1->kind == IR_OP_VAR) instr->src2 = ir_operand_var(op1->data.var_name);
                                 }
                             }
                             // If we add IR_SHL/IR_SHR later, we can use shift here for any power of 2
                        }
                    }
                }
            }
        }
    }
}

// Dead Store Elimination in Loops: Remove stores that are immediately overwritten
void iropt_dead_store_elimination_loops(IRModule *module) {
    if (!module) return;
    
    for (size_t f = 0; f < module->function_count; f++) {
        IRFunction *func = module->functions[f];
        
        // Look for stores that are overwritten before being read
        for (size_t i = 0; i < func->instruction_count; i++) {
            IRInstruction *instr = func->instructions[i];
            
            // Skip if not a store or assignment
            if (!instr->dest || instr->dest->kind != IR_OP_TEMP) continue;
            
            int dest_temp = instr->dest->data.temp_id;
            
            // Look ahead to see if this value is used before being redefined
            bool is_used = false;
            bool is_redefined = false;
            
            for (size_t j = i + 1; j < func->instruction_count && j < i + 20; j++) {
                IRInstruction *next = func->instructions[j];
                
                // Check if used
                if (next->src1 && next->src1->kind == IR_OP_TEMP && next->src1->data.temp_id == dest_temp) {
                    is_used = true;
                    break;
                }
                if (next->src2 && next->src2->kind == IR_OP_TEMP && next->src2->data.temp_id == dest_temp) {
                    is_used = true;
                    break;
                }
                
                // Check if redefined
                if (next->dest && next->dest->kind == IR_OP_TEMP && next->dest->data.temp_id == dest_temp) {
                    is_redefined = true;
                    break;
                }
                
                // Stop at labels (control flow boundary)
                if (next->opcode == IR_LABEL) break;
            }
            
            // If redefined before use, this is a dead store
            if (is_redefined && !is_used) {
                 // Safe to remove!
                 // Replace with NOP
                 if (instr->dest) ir_operand_free(instr->dest);
                 if (instr->src1) ir_operand_free(instr->src1);
                 if (instr->src2) ir_operand_free(instr->src2);
                 instr->opcode = IR_NOP;
                 instr->dest = NULL;
                 instr->src1 = NULL;
                 instr->src2 = NULL;
            }
        }
    }
}
