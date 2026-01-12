#ifndef IROPT_H
#define IROPT_H

#include "ir.h"

// IR Optimizer
typedef struct IROptimizer IROptimizer;

// Create and destroy optimizer
IROptimizer *iropt_create(void);
void iropt_free(IROptimizer *opt);

// Optimization passes
void iropt_constant_folding(IRModule *module);
void iropt_dead_code_elimination(IRModule *module);
void iropt_copy_propagation(IRModule *module);

// Run all optimizations
void iropt_optimize(IRModule *module);

#endif // IROPT_H
