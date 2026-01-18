# Virex LLVM Backend Roadmap

## Overview

This roadmap details the implementation of an LLVM backend for the Virex compiler. The goal is to replace the current C code generation with native code generation via LLVM, enabling professional-grade optimizations and multi-platform support.

**Target:** Replace `codegen.c` (C backend) with `llvm_codegen.c` (LLVM IR backend)

---

## Phase 0: Infrastructure Setup (Week 1)

### 0.1 LLVM Integration
- [x] Add LLVM-C as build dependency in Makefile
- [x] Create `include/llvm_codegen.h` header
- [x] Create `src/llvm_codegen.c` skeleton
- [x] Verify LLVM library linkage (`llvm-config --cflags --ldflags --libs core`)

### 0.2 Build System Updates
- [x] Update `Makefile` with LLVM compilation flags
- [x] Add `llvm_codegen.c` to build **alongside** `codegen.c` (parallel during development)
- [x] Add `--backend=llvm` flag for testing during development
- [x] Default to C backend until LLVM passes all tests

### 0.3 Test Infrastructure
- [x] Modify `test_runner.sh` to accept `--backend` flag
- [x] Run tests with both backends during transition: `./test_runner.sh --backend=llvm`
- [x] Create comparison report: `./test_runner.sh --compare`

---

## Phase 1: Core LLVM Scaffolding (Weeks 2-3)

### 1.1 LLVM Module Initialization
- [ ] Create LLVM context, module, builder
- [ ] Implement `llvm_codegen_create()` / `llvm_codegen_free()`
- [ ] Implement `llvm_codegen_generate()` entry point
- [ ] Add module verification (`LLVMVerifyModule`)

### 1.2 Type System Mapping
Map Virex types to LLVM types:

| Virex Type | LLVM Type | Implementation |
|------------|-----------|----------------|
| `i8` | `LLVMInt8Type()` | Direct |
| `i16` | `LLVMInt16Type()` | Direct |
| `i32` | `LLVMInt32Type()` | Direct |
| `i64` | `LLVMInt64Type()` | Direct |
| `u8`-`u64` | `LLVMIntNType()` | Same as signed |
| `f32` | `LLVMFloatType()` | Direct |
| `f64` | `LLVMDoubleType()` | Direct |
| `bool` | `LLVMInt1Type()` | Direct |
| `void` | `LLVMVoidType()` | Direct |
| `*T` | `LLVMPointerType(T, 0)` | Pointer to T |
| `*!T` | `LLVMPointerType(T, 0)` | Same (semantics enforced earlier) |
| `[]T` | Slice struct | `{ptr, len}` struct |
| `[N]T` | `LLVMArrayType(T, N)` | Direct |
| `struct` | `LLVMStructType()` | Named struct |
| `enum` | `LLVMInt32Type()` | Integer tag |
| `result<T,E>` | Tagged union struct | `{tag, union{T,E}}` |

#### Implementation Tasks
- [ ] Implement `virex_type_to_llvm()` function
- [ ] Handle primitive types
- [ ] Handle pointer types (nullable and non-null)
- [ ] Handle array types
- [ ] Handle slice types as fat pointers
- [ ] Handle struct types (forward declarations)
- [ ] Handle enum types
- [ ] Handle result types
- [ ] Handle type aliases (resolve to underlying)
- [ ] Handle generic type instantiations

### 1.3 Function Declarations
- [ ] Emit function prototypes for all functions
- [ ] Handle parameter types correctly
- [ ] Handle return types (including void)
- [ ] Handle variadic functions (for `printf`)
- [ ] Handle extern function declarations
- [ ] Set calling convention (C ABI)

### 1.4 Module System Integration
The LLVM codegen must work with the existing module system:

- [ ] Accept `Project*` containing all modules (same as C backend)
- [ ] Iterate over `project->modules` to emit all code
- [ ] Handle module namespacing in function names (already done by monomorphization)
- [ ] Emit all struct/enum definitions before functions

### 1.5 Monomorphization Integration
Generic instantiation happens **before** LLVM codegen (no changes needed):

- [ ] Receive already-monomorphized IR from `irgen.c`
- [ ] Emit specialized functions with mangled names (e.g., `max__i32`)
- [ ] No generic handling in LLVM layer (already resolved)

---

## Phase 2: IR Instruction Translation (Weeks 4-6)

### 2.1 Arithmetic Operations

Implement IR → LLVM for each opcode:

| IR Opcode | LLVM Builder Function |
|-----------|----------------------|
| `IR_ADD` | `LLVMBuildAdd` / `LLVMBuildFAdd` |
| `IR_SUB` | `LLVMBuildSub` / `LLVMBuildFSub` |
| `IR_MUL` | `LLVMBuildMul` / `LLVMBuildFMul` |
| `IR_DIV` | `LLVMBuildSDiv` / `LLVMBuildUDiv` / `LLVMBuildFDiv` |
| `IR_MOD` | `LLVMBuildSRem` / `LLVMBuildURem` / `LLVMBuildFRem` |
| `IR_NEG` | `LLVMBuildNeg` / `LLVMBuildFNeg` |

#### Tasks
- [ ] Implement signed integer arithmetic
- [ ] Implement unsigned integer arithmetic
- [ ] Implement floating-point arithmetic
- [ ] Handle type-dependent operation selection

### 2.2 Comparison Operations

| IR Opcode | LLVM Predicate |
|-----------|----------------|
| `IR_EQ` | `LLVMIntEQ` / `LLVMRealOEQ` |
| `IR_NE` | `LLVMIntNE` / `LLVMRealONE` |
| `IR_LT` | `LLVMIntSLT` / `LLVMRealOLT` |
| `IR_LE` | `LLVMIntSLE` / `LLVMRealOLE` |
| `IR_GT` | `LLVMIntSGT` / `LLVMRealOGT` |
| `IR_GE` | `LLVMIntSGE` / `LLVMRealOGE` |

#### Tasks
- [ ] Implement integer comparisons (signed)
- [ ] Implement integer comparisons (unsigned)
- [ ] Implement floating-point comparisons
- [ ] Return i1 (bool) result

### 2.3 Logical Operations

| IR Opcode | LLVM Implementation |
|-----------|---------------------|
| `IR_AND` | `LLVMBuildAnd` |
| `IR_OR` | `LLVMBuildOr` |
| `IR_NOT` | `LLVMBuildNot` / `LLVMBuildXor` with 1 |

#### Tasks
- [ ] Implement logical AND
- [ ] Implement logical OR
- [ ] Implement logical NOT

### 2.4 Memory Operations

| IR Opcode | LLVM Builder Function |
|-----------|----------------------|
| `IR_ALLOCA` | `LLVMBuildAlloca` |
| `IR_LOAD` | `LLVMBuildLoad2` |
| `IR_STORE` | `LLVMBuildStore` |
| `IR_ADDR` | `LLVMBuildGEP2` (address of) |
| `IR_DEREF` | `LLVMBuildLoad2` |

#### Tasks
- [ ] Implement stack allocation (`alloca`)
- [ ] Implement load from memory
- [ ] Implement store to memory
- [ ] Implement address-of operator
- [ ] Implement dereference operator
- [ ] Handle struct field access via GEP
- [ ] Handle array indexing via GEP

### 2.5 Control Flow

| IR Opcode | LLVM Builder Function |
|-----------|----------------------|
| `IR_LABEL` | `LLVMAppendBasicBlock` |
| `IR_JUMP` | `LLVMBuildBr` |
| `IR_BRANCH` | `LLVMBuildCondBr` |
| `IR_RETURN` | `LLVMBuildRet` / `LLVMBuildRetVoid` |

#### Tasks
- [ ] Implement basic block creation
- [ ] Implement unconditional branch
- [ ] Implement conditional branch
- [ ] Implement return statement
- [ ] Implement return void
- [ ] Handle unreachable code (`LLVMBuildUnreachable`)

### 2.6 Function Calls

| IR Opcode | LLVM Builder Function |
|-----------|----------------------|
| `IR_CALL` | `LLVMBuildCall2` |

#### Tasks
- [ ] Implement function call
- [ ] Handle arguments correctly
- [ ] Handle return value capture
- [ ] Handle void function calls
- [ ] Handle variadic function calls

### 2.7 Type Conversions

| IR Opcode | LLVM Builder Functions |
|-----------|----------------------|
| `IR_CAST` | `LLVMBuildIntCast2`, `LLVMBuildFPCast`, `LLVMBuildSIToFP`, etc. |
| `IR_MOVE` | Copy or `LLVMBuildBitCast` |

#### Tasks
- [ ] Implement integer to integer cast (widening/narrowing)
- [ ] Implement float to float cast
- [ ] Implement integer to float cast
- [ ] Implement float to integer cast
- [ ] Implement pointer casts
- [ ] Implement struct copies

---

## Phase 3: Complex Language Features (Weeks 7-9)

### 3.1 Struct Support
- [ ] Emit LLVM struct type definitions
- [ ] Handle field ordering (same as C)
- [ ] Implement struct field access (`LLVMBuildStructGEP2`)
- [ ] Implement struct assignment (memcpy or field-by-field)
- [ ] Implement struct as function parameter
- [ ] Implement struct as return value
- [ ] Handle packed structs (`LLVMStructSetBody` with packed flag)

### 3.2 Enum Support
- [ ] Emit enum as i32 constants
- [ ] Handle enum variant values
- [ ] Implement match on enum (switch instruction)

### 3.3 Result Type Support
- [ ] Define result struct layout: `{i32 tag, union {T ok, E err}}`
- [ ] Implement `result::ok(v)` construction
- [ ] Implement `result::err(e)` construction
- [ ] Implement match on result (tag comparison)
- [ ] Handle result unwrapping

### 3.4 Array Support
- [ ] Emit fixed-size array types
- [ ] Implement array initialization
- [ ] Implement array indexing
- [ ] Implement array to slice conversion

### 3.5 Slice Support
Slice representation: `{ptr: *T, len: usize}`

- [ ] Define slice struct type
- [ ] Implement slice creation from array
- [ ] Implement slice indexing
- [ ] Implement slice length access
- [ ] Implement slice as parameter
- [ ] Implement for-in loop over slice

### 3.6 String Literals
String literals in Virex are `[]u8` slices (no special `cstring` type).

- [ ] Emit string constants as global `[N x i8]` arrays
- [ ] Create `[]u8` slice pointing to the constant (`{ptr, len}`)
- [ ] Handle escape sequences (already processed by lexer)
- [ ] Ensure null terminator for FFI compatibility when passed to `extern` functions

---

## Phase 4: FFI and External Calls (Week 10)

### 4.1 Extern Function Support
- [ ] Declare extern functions with correct signatures
- [ ] Map C ABI types to LLVM types
- [ ] Handle `printf` and variadic functions
- [ ] Handle struct passing to C functions
- [ ] Handle struct returns from C functions

### 4.2 C ABI Compatibility
| Virex ABI Type | LLVM Type |
|----------------|-----------|
| `c_int` | Platform `int` (i32 typically) |
| `c_long` | Platform `long` (i64 on LP64) |
| `c_char` | i8 |
| etc. | ... |

- [ ] Create ABI type resolution based on target triple
- [ ] Implement all C ABI types from CORE.md

### 4.3 Linking
- [ ] Emit object file with `LLVMTargetMachineEmitToFile`
- [ ] Link with system libraries (libc, libm)
- [ ] Handle `-l` flags for external libraries

---

## Phase 5: Optimization Integration (Weeks 11-12)

### 5.1 LLVM Optimization Passes
Replace current `iropt.c` with LLVM passes:

| Current Pass | LLVM Equivalent |
|--------------|-----------------|
| Constant Folding | Built into LLVM |
| Copy Propagation | `-mem2reg`, `-instcombine` |
| CSE | `-early-cse`, `-gvn` |
| LICM | `-licm` |
| Strength Reduction | `-loop-reduce` |
| Dead Code Elimination | `-dce`, `-adce` |

#### Tasks
- [ ] Create pass manager
- [ ] Add optimization passes based on `-O` level
- [ ] `-O0`: No optimization (debug)
- [ ] `-O1`: Basic optimizations
- [ ] `-O2`: Standard optimizations
- [ ] `-O3`: Aggressive optimizations

### 5.2 Optimization Level Mapping

```
-O0: No passes, fast compilation
-O1: mem2reg, instcombine, simplifycfg
-O2: + inline, gvn, licm, loop-unroll
-O3: + vectorization, aggressive inline
```

- [ ] Implement `-O` flag parsing
- [ ] Configure pass pipeline per level

---

## Phase 6: Debug Information (Week 13)

### 6.1 DWARF Debug Info
- [ ] Create DIBuilder for debug info
- [ ] Emit compilation unit
- [ ] Emit file information
- [ ] Emit function debug info
- [ ] Emit line number info
- [ ] Emit variable debug info

### 6.2 Source Mapping
- [ ] Track source locations through IR
- [ ] Associate LLVM instructions with source lines
- [ ] Generate `.dSYM` / `.debug` sections

---

## Phase 7: Code Emission (Week 14)

### 7.1 Target Machine
- [ ] Initialize all LLVM targets (`LLVMInitializeAllTargets`)
- [ ] Get target triple (`LLVMGetDefaultTargetTriple`)
- [ ] Create target machine
- [ ] Configure for optimization level
- [ ] Configure for relocation model (PIC, etc.)

### 7.2 Output Formats
- [ ] Emit LLVM IR (`.ll`) with `--emit-llvm-ir`
- [ ] Emit LLVM bitcode (`.bc`) with `--emit-llvm-bc`
- [ ] Emit assembly (`.s`) with `--emit-asm`
- [ ] Emit object file (`.o`) with `-c`
- [ ] Emit executable (default) via linker invocation

### 7.3 Linker Integration
- [ ] Invoke system linker (ld/lld)
- [ ] Pass object files
- [ ] Pass library flags
- [ ] Handle static vs dynamic linking

---

## Phase 8: Testing and Validation (Weeks 15-16)

### 8.1 Unit Tests
- [ ] Test type mapping
- [ ] Test arithmetic operations
- [ ] Test comparisons
- [ ] Test control flow
- [ ] Test function calls

### 8.2 Integration Tests
Run all existing tests with LLVM backend:

- [ ] `tests/basics/` (9 tests)
- [ ] `tests/control_flow/` (7 tests)
- [ ] `tests/error/` (8 tests)
- [ ] `tests/ffi/` (6 tests)
- [ ] `tests/generics/` (5 tests)
- [ ] `tests/modules/` (11 tests)
- [ ] `tests/safety/` (6 tests)
- [ ] `tests/slices/` (8 tests)
- [ ] `tests/types/` (9 tests)
- [ ] `tests/unsafe/` (2 tests)

### 8.3 Validation Against C Backend
During transition, validate LLVM output matches C backend:

- [ ] Compare output for all test cases
- [ ] Fix any behavioral differences
- [ ] Document any intentional improvements

### 8.4 Benchmarks
- [ ] Run existing benchmarks with both backends
- [ ] Measure compile time difference
- [ ] Measure runtime performance difference
- [ ] Document improvements

---

## Phase 9: C Backend Removal (Week 17)

> **IMPORTANT**: Only proceed with this phase after ALL 75 tests pass with LLVM backend.

### 9.1 Switch Default Backend
- [ ] Change default from `--backend=c` to `--backend=llvm`
- [ ] Run full test suite one more time
- [ ] Notify in release notes that C backend is deprecated

### 9.2 Remove C Backend Code
- [ ] Delete `src/codegen.c`
- [ ] Delete `include/codegen.h`
- [ ] Remove `--backend` flag entirely (LLVM only)
- [ ] Remove C-specific code paths from `compiler.c` and `main.c`
- [ ] Clean up unused helper functions

### 9.3 Simplify IR Layer
- [ ] Remove C-string type representations from IR
- [ ] Simplify `iropt.c` to validation-only (or remove entirely)
- [ ] Remove C-specific type emission code from `irgen.c`

### 9.4 Documentation
- [ ] Update README.md with LLVM requirements
- [ ] Document build prerequisites (LLVM 15+)
- [ ] Remove references to C backend
- [ ] Create LLVM troubleshooting guide

---

## Implementation Checklist Summary

### Files to Create
- [ ] `include/llvm_codegen.h` (~100 lines)
- [ ] `src/llvm_codegen.c` (~2500 lines)

### Files to Delete (after LLVM complete)
- [ ] `src/codegen.c` (C backend)
- [ ] `include/codegen.h`
- [ ] Possibly `src/iropt.c` (LLVM handles optimizations)

### Files to Modify
- [ ] `Makefile` - Replace C backend with LLVM
- [ ] `src/main.c` - Remove backend selection (LLVM only)
- [ ] `test_runner.sh` - Update for LLVM
- [ ] `src/compiler.c` - Call LLVM codegen directly

### Dependencies
- **LLVM 15-18** (any recent version)
- `llvm-config` in PATH
- Development packages:
  - Debian/Ubuntu: `sudo apt install llvm-dev`
  - macOS: `brew install llvm`
  - Fedora: `sudo dnf install llvm-devel`

---

## Timeline Summary

| Phase | Description | Duration | Cumulative |
|-------|-------------|----------|------------|
| 0 | Infrastructure | 1 week | Week 1 |
| 1 | Core Scaffolding | 2 weeks | Week 3 |
| 2 | IR Translation | 3 weeks | Week 6 |
| 3 | Complex Features | 3 weeks | Week 9 |
| 4 | FFI | 1 week | Week 10 |
| 5 | Optimization | 2 weeks | Week 12 |
| 6 | Debug Info | 1 week | Week 13 |
| 7 | Code Emission | 1 week | Week 14 |
| 8 | Testing | 2 weeks | Week 16 |
| 9 | Polish | 1 week | Week 17 |

**Total: ~17 weeks (4 months)**

---

## Success Criteria

1. All 75 existing tests pass with LLVM backend
2. Generated code is correct (matches C backend behavior)
3. Compile time is within 2x of C backend
4. Runtime performance is better than C backend with `-O2`
5. Debug symbols work with GDB/LLDB
6. Cross-compilation possible (x86_64, ARM64)

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| LLVM API changes | Pin to LLVM 17 LTS |
| Complex ABI issues | Use `clang -emit-llvm` as reference |
| Debug info complexity | Start without it, add in Phase 6 |
| Slice implementation | Match C backend exactly first |

---

*Last updated: 2026-01-18*
