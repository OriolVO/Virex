// Loop Pattern Detection and For-Loop Generation
// 
// This module detects simple counting loops in the IR and generates
// C for-loops instead of goto-based loops for better GCC optimization.
//
// Pattern to detect:
//   var = init_value
//   L_start:
//     t_cond = var < limit
//     if (t_cond) goto L_body
//     goto L_end
//   L_body:
//     ... loop body ...
//   L_cont:
//     t_inc = var + step
//     var = t_inc
//     goto L_start
//   L_end:
//
// Transforms to:
//   for (var = init_value; var < limit; var += step) {
//     ... loop body ...
//   }

#ifndef LOOP_TRANSFORM_H
#define LOOP_TRANSFORM_H

#include "../include/ir.h"
#include <stdbool.h>

typedef struct {
    bool is_simple_loop;
    size_t loop_start_idx;      // Index of loop start label
    size_t loop_end_idx;        // Index of loop end label
    size_t body_start_idx;      // Index of loop body start
    size_t continue_label_idx;  // Index of continue label
    
    char *loop_var;             // Loop variable name
    IROperand *init_value;      // Initial value
    IROperand *limit_value;     // Loop limit
    IROperand *step_value;      // Increment step
    IROpcode comparison_op;     // Comparison operator (<, <=, >, >=)
} LoopInfo;

// Detect if a sequence of instructions forms a simple counting loop
LoopInfo detect_simple_loop(IRFunction *func, size_t start_idx);

// Check if instruction sequence matches loop pattern
bool is_loop_pattern(IRFunction *func, size_t idx);

#endif // LOOP_TRANSFORM_H
