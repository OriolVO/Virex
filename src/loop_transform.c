#include "../include/loop_transform.h"
#include "../include/ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Detect if a sequence starting at idx forms a simple counting loop
LoopInfo detect_simple_loop(IRFunction *func, size_t start_idx) {
    LoopInfo info = {0};
    info.is_simple_loop = false;
    
    if (start_idx >= func->instruction_count) return info;
    
    IRInstruction *instr = func->instructions[start_idx];
    
    // Must start with a label
    if (instr->opcode != IR_LABEL) return info;
    if (!instr->src1 || instr->src1->kind != IR_OP_LABEL) return info;
    
    const char *loop_label = instr->src1->data.label_name;
    info.loop_start_idx = start_idx;
    
    // Next should be comparison
    if (start_idx + 1 >= func->instruction_count) return info;
    IRInstruction *cmp = func->instructions[start_idx + 1];
    
    // Check if it's a comparison operation
    if (cmp->opcode != IR_LT && cmp->opcode != IR_LE && 
        cmp->opcode != IR_GT && cmp->opcode != IR_GE) return info;
    
    info.comparison_op = cmp->opcode;
    
    // Next should be conditional branch
    if (start_idx + 2 >= func->instruction_count) return info;
    IRInstruction *branch = func->instructions[start_idx + 2];
    
    if (branch->opcode != IR_BRANCH) return info;
    if (!branch->src2 || branch->src2->kind != IR_OP_LABEL) return info;
    
    // Find the backward jump (goto loop_label)
    for (size_t i = start_idx + 3; i < func->instruction_count && i < start_idx + 100; i++) {
        IRInstruction *jmp = func->instructions[i];
        if (jmp->opcode == IR_JUMP && jmp->src1 && jmp->src1->kind == IR_OP_LABEL) {
            if (strcmp(jmp->src1->data.label_name, loop_label) == 0) {
                // Found backward jump - this looks like a loop!
                info.is_simple_loop = true;
                info.loop_end_idx = i;
                return info;
            }
        }
    }
    
    return info;
}
