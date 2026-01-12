# Virex v0.1 Development Roadmap

> **Goal:** Build a minimal but fully functional Virex compiler that demonstrates core language features and produces working executables.

---

## Phase 1: Foundation & Tooling Setup

**Goal:** Establish the development environment and basic project structure.

### 1.1 Project Infrastructure
- [x] Set up C-based compiler project structure
- [x] Configure build system (Makefile or CMake)
- [x] Create basic CLI interface (`virex build`, `virex --version`)
- [x] Set up testing framework for compiler tests (check, cmocka, or custom)
- [x] Create example programs directory (`examples/`)
- [x] Set up memory leak detection (Valgrind integration)

### 1.2 Documentation & Planning
- [x] Complete CORE.md specification
- [x] Create architecture documentation (compiler stages overview)
- [x] Set up contribution guidelines
- [x] Create test case template

**Deliverable:** Working project skeleton with CLI that prints help messages. ✅ **COMPLETE**

---

## C-Specific Tooling & Recommendations

**Goal:** Set up a productive C development environment for compiler development.

### Build System Options

**Option A: Makefile (Recommended for simplicity)**
- Simple, universal, no dependencies
- Good for learning and small projects
- Example structure:
  ```makefile
  CC = gcc
  CFLAGS = -std=c11 -Wall -Wextra -O2
  LDFLAGS = 
  
  virex: main.o lexer.o parser.o ...
      $(CC) -o $@ $^ $(LDFLAGS)
  ```

**Option B: CMake (Recommended for larger projects)**
- Cross-platform, modern
- Better dependency management
- Easier to integrate with IDEs

### Recommended Helper Libraries

| Library | Purpose | Why Use It |
|---------|---------|------------|
| [uthash](https://troydhanson.github.io/uthash/) | Hash tables | Symbol tables, type lookups |
| [stb_ds.h](https://github.com/nothings/stb/blob/master/stb_ds.h) | Dynamic arrays, hash maps | AST nodes, token lists |
| [LLVM C API](https://llvm.org/doxygen/group__LLVMC.html) | Code generation | Production-quality backend (optional) |
| [check](https://libcheck.github.io/check/) or [cmocka](https://cmocka.org/) | Unit testing | Test framework |

### Memory Management Tools

- **Valgrind** - Memory leak detection
  ```bash
  valgrind --leak-check=full ./virex test.vx
  ```
- **AddressSanitizer** - Compile-time instrumentation
  ```bash
  gcc -fsanitize=address -g ...
  ```

### Recommended Project Structure

```
virex/
├── Makefile or CMakeLists.txt
├── README.md
├── CORE.md
├── ROADMAP.md
├── src/
│   ├── main.c           # CLI entry point
│   ├── lexer.c/h        # Tokenization
│   ├── parser.c/h       # AST construction
│   ├── ast.c/h          # AST node definitions
│   ├── semantic.c/h     # Type checking
│   ├── codegen.c/h      # Code generation
│   ├── symtable.c/h     # Symbol table
│   ├── types.c/h        # Type system
│   ├── error.c/h        # Error reporting
│   ├── util.c/h         # Utilities (string, memory)
│   └── ir.c/h           # Intermediate representation
├── include/
│   └── virex.h          # Public API header
├── lib/                 # Third-party libraries (uthash, stb, etc.)
├── tests/
│   ├── test_lexer.c
│   ├── test_parser.c
│   └── ...
├── examples/
│   ├── hello.vx
│   ├── fibonacci.vx
│   └── ...
└── build/               # Build artifacts (gitignored)
```

### Development Workflow

1. **Edit code** in `src/`
2. **Build** with `make` or `cmake --build build/`
3. **Test** with `make test` or `ctest`
4. **Debug** with `gdb ./virex` or `lldb ./virex`
5. **Check leaks** with `valgrind`

### C11 Features to Use

- `_Static_assert` for compile-time checks
- `_Generic` for type-generic macros (optional)
- `stdint.h` types (`int32_t`, `uint64_t`, etc.)
- `stdbool.h` for `bool`, `true`, `false`
- Designated initializers for structs

### Coding Standards

- **C11 standard** (or C17)
- **-Wall -Wextra -Wpedantic** compiler flags
- **Consistent naming** (snake_case for functions/variables)
- **Header guards** or `#pragma once`
- **Documentation** with comments

---

## Phase 2: Lexer & Tokenization

**Goal:** Convert source code into a stream of tokens.

### 2.1 Core Token Types
- [x] Implement keywords (`var`, `const`, `func`, `if`, `else`, `while`, `for`, `return`, `struct`, `enum`, `unsafe`, `public`, `module`, `import`, `extern`, `match`, `fail`)
- [x] Implement primitive type tokens (`i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `bool`, `void`)
- [x] Implement operators (`+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`, `&`, `->`)
- [x] Implement delimiters (`{`, `}`, `(`, `)`, `[`, `]`, `;`, `,`, `.`)
- [x] Implement literals (integers, floats, strings, booleans)
- [x] Implement identifiers and comments

### 2.2 Lexer Features
- [x] Line and column tracking for error reporting
- [x] String escape sequences
- [x] Multi-line comment support (`/* */`)
- [x] Single-line comment support (`//`)

### 2.3 Testing
- [ ] Unit tests for each token type
- [x] Error handling for invalid characters
- [x] Test files with various edge cases

**Deliverable:** Lexer that can tokenize all basic Virex syntax. ✅ **COMPLETE**

---

## Phase 3: Parser & AST Construction

**Goal:** Build an Abstract Syntax Tree from tokens.

### 3.1 Expression Parsing
- [x] Literal expressions (numbers, booleans)
- [x] Variable references
- [x] Binary operations (arithmetic, comparison, logical)
- [x] Unary operations (`-`, `!`, `&`, `*`)
- [x] Function calls
- [x] Array indexing
- [x] Struct field access (`.` operator)
- [x] Pointer dereference and address-of

### 3.2 Statement Parsing
- [x] Variable declarations (`var`, `const`)
- [x] Assignment statements
- [x] If/else statements
- [x] While loops
- [x] For loops
- [x] Return statements
- [x] Expression statements
- [x] Block statements

### 3.3 Declaration Parsing
- [x] Function declarations
- [x] Struct declarations
- [x] Enum declarations (basic)
- [x] Module declarations (placeholder)
- [x] Import statements (placeholder)

### 3.4 Type Parsing
- [x] Primitive types
- [x] Pointer types (`*T`, `*!T`)
- [x] Array types (`T[N]`)
- [x] Function types (for function pointers)
- [x] Generic type parameters (`<T>`)

### 3.5 Error Recovery
- [x] Synchronization points for error recovery
- [x] Helpful error messages with line/column info
- [x] Multiple error reporting (don't stop at first error)

**Deliverable:** Parser that produces a complete AST for valid Virex programs. ✅ **COMPLETE**

---

## Phase 4: Semantic Analysis

**Goal:** Validate the AST and prepare for code generation.

### 4.1 Symbol Table & Scoping
- [x] Build symbol table for variables, functions, types
- [x] Implement scope management (nested scopes)
- [x] Name resolution (local → module → std)
- [x] Detect duplicate declarations
- [x] Detect undefined references

### 4.2 Type Checking
- [x] Type inference for literals
- [x] Type checking for binary operations
- [x] Type checking for function calls (argument types, return type)
- [x] Type checking for assignments
- [x] Struct field type validation (basic implementation)
- [x] Array bounds checking (compile-time where possible)
- [x] Pointer type compatibility

### 4.3 Safety Analysis
- [x] Detect nullable pointer dereferences outside `unsafe`
- [x] Validate non-null pointer initialization

### 4.4 Control Flow Analysis
- [x] Ensure all code paths return a value (for non-void functions)
- [x] Detect unreachable code

**Deliverable:** Semantic analyzer that catches type errors and safety violations. ✅ **COMPLETE**

---

## Phase 5: Intermediate Representation (IR)

**Goal:** Lower AST to a simpler, platform-agnostic IR.

### 5.1 IR Design
- [x] Define IR instruction set (load, store, add, sub, mul, div, call, ret, br, etc.)
- [x] Design IR data structures (basic blocks, functions, modules)
- [x] Implement SSA form (Static Single Assignment) or similar
- [x] Handle control flow (jumps, branches, labels)

### 5.2 AST → IR Lowering
- [x] Lower expressions to IR instructions
- [x] Lower statements to IR instructions
- [x] Lower function declarations
- [x] Lower struct types (memory layout)
- [x] Handle pointer operations
- [x] Handle array operations

### 5.3 IR Validation
- [x] Verify IR correctness (type consistency, valid jumps)
- [x] IR pretty-printer for debugging (`--emit-ir` flag)

**Deliverable:** Working IR generator with debug output capability. ✅ **COMPLETE**

---

## Phase 6: Basic Optimizations

**Goal:** Implement essential optimizations for reasonable performance.

### 6.1 Local Optimizations
- [x] Constant folding (evaluate constant expressions at compile time)
- [x] Dead code elimination (remove unreachable code)
- [x] Copy propagation
- [x] Common subexpression elimination (basic)

**Deliverable:** Optimized IR that produces faster executables. ✅ **COMPLETE**

---

## Phase 7: Code Generation (C Backend)

**Goal:** Generate executable code via C transpilation (bootstrap-friendly approach).

### 7.1 Backend Setup
- [x] Choose backend approach: **C code generation** (Option C - best for bootstrapping)
- [x] Set up C code generator infrastructure
- [x] Implement IR → C translation

### 7.2 Code Emission
- [x] Generate C for arithmetic operations
- [x] Generate C for memory operations (load/store)
- [x] Generate C for function calls and returns
- [x] Generate C for control flow (goto, labels)
- [x] Handle temporary variables
- [x] Handle function declarations

### 7.3 Build Integration
- [x] Integrate code generator into `build` command
- [x] Generate `.c` files from `.vx` source
- [x] Provide gcc compilation instructions

**Deliverable:** Compiler that generates C code, compilable with gcc. ✅ **COMPLETE**

**Bootstrap Path:** Virex → C → gcc → binary (later: Virex → Virex → binary)

---

## Phase 8: Linking & Executable Generation

**Goal:** Link object files and produce runnable executables.

### 8.1 Linker Integration
- [x] Invoke system linker (gcc)
- [x] Link against libc for basic functions
- [x] Generate executable with correct permissions
- [x] Automatic compilation after C generation

### 8.2 Testing
- [x] Test simple programs (arithmetic)
- [x] Test function calls
- [x] Test control flow (if/else, loops)

**Deliverable:** Working executables that run on x86_64 Linux. ✅ **COMPLETE**

---

## Phase 9: Core Standard Library

**Goal:** Implement minimal standard library functions.

### 9.1 `std::mem`
- [x] `alloc(size, count) -> *u8` (wrapper around malloc)
- [x] `free(*u8)` (wrapper around free)
- [x] `copy(*u8 dst, *u8 src, count)` (memcpy wrapper)
- [x] `set(*u8 dst, value, count)` (memset wrapper)

### 9.2 `std::io`
- [x] `print_i32(value)` (print integer)
- [x] `println_i32(value)` (print integer with newline)
- [x] `print_bool(value)` (print boolean)
- [x] `println_bool(value)` (print boolean with newline)
- [x] `print_str(str)` (print string)
- [x] `println_str(str)` (print string with newline)

### 9.3 `std::os`
- [x] `exit(code)` (wrapper around exit syscall)
- [x] `get_argc()` (get argument count)
- [x] `get_argv(index)` (get argument value)

### 9.4 Testing
- [x] Runtime library compiled
- [x] Linked with generated code
- [x] Example programs created

**Deliverable:** Working standard library with essential functions. ✅ **COMPLETE**

---

## Phase 10: Generics with Monomorphization

**Goal:** Support generic functions with compile-time specialization.

### 10.1 Generic Infrastructure
- [x] AST support for generic type parameters
- [x] Type parameter storage in function declarations
- [x] Parser compatibility with generic syntax

### 10.2 Monomorphization Engine
- [x] Type parameter substitution
- [x] Generic function instantiation
- [x] Mangled name generation for instantiated functions
- [x] Duplicate instantiation prevention
- [x] Type substitution in parameters and return types

### 10.3 Integration
- [x] Monomorphization context management
- [x] Program transformation with instantiated functions
- [x] Pointer and array type substitution

**Deliverable:** Working generics with compile-time monomorphization. ✅ **COMPLETE**

**Note:** Full AST body cloning deferred to future optimization. Current implementation instantiates function signatures correctly.

---

## Phase 11: Module System

**Goal:** Support multi-file projects with imports and exports.

### 11.1 Module Resolution
- [x] Parse `module` declarations
- [x] Parse `import` statements
- [x] Implement file-based module lookup (local → std)
- [x] Handle module aliasing (`import "x" as y`)

### 11.2 Visibility
- [x] Implement `public` keyword
- [x] Enforce visibility rules (private by default)
- [x] Export public symbols

### 11.3 Compilation Units
- [x] Compile multiple files
- [x] Link compiled modules together
- [x] Handle circular dependencies (detect and error)

### 11.4 Testing
- [x] Test multi-file projects
- [x] Test import resolution
- [x] Test visibility enforcement

**Deliverable:** Working module system for multi-file projects. ✅ **COMPLETE**

---

## Phase 12: Error Handling (Result Types)

**Goal:** Implement `result<T, E>` type and pattern matching.

### 12.1 Result Type
- [x] Define `result<T, E>` as a built-in generic enum
- [x] Implement `result::ok(T)` and `result::err(E)` constructors
- [x] Type checking for result types

### 12.2 Pattern Matching
- [x] Parse `match` expressions
- [x] Implement pattern matching for enums
- [x] Exhaustiveness checking (ensure all cases covered)
- [x] Extract values from patterns

### 12.3 Fail Keyword
- [x] Implement `fail` keyword (terminates program)
- [x] Generate appropriate error messages

### 12.4 Standard Library Support
- [x] Implement `std::result::unwrap<T, E>(result<T, E>)`
- [x] Implement `std::result::expect<T, E>(result<T, E>, cstring)`

### 12.5 Testing
- [x] Test result type creation and matching
- [x] Test `unwrap` and `expect`
- [x] Test error propagation patterns

**Deliverable:** Robust error handling mechanism. ✅ **COMPLETE**



---

## Phase 13: C Interop (FFI)

**Goal:** Enable calling C functions and linking C libraries.

### 13.1 Extern Declarations
- [x] Parse `extern` function declarations
- [x] Support C ABI types (`c_int`, `c_char`, etc.)
- [x] Handle variadic functions (`...`)

### 13.2 ABI Compatibility
- [x] Ensure struct layout matches C (padding, alignment)
- [x] Implement `packed` attribute for structs
  - [x] Add parser support for `packed struct`
  - [x] Update codegen to emit `__attribute__((packed))`
  - [x] Verify layout with C tests
- [x] Handle calling convention (cdecl)

### 13.3 Linking
- [x] Link against C libraries (`-l` flag)
- [x] Resolve external symbols

### 13.4 Testing
- [x] Test calling `printf` from C
- [x] Test calling other libc functions (`strlen`, `strcmp`)
- [x] Test struct interop with C

**Deliverable:** Working C interop for calling external libraries. ✅ **COMPLETE**

---

## Phase 14: Testing & Examples

**Goal:** Comprehensive testing and example programs.

### 14.1 Compiler Edge Case Tests
- [x] Complex expression precedence and nesting
- [x] Deeply nested control flow (if/else, while, for)
- [x] Nested structs and self-referential pointer fields
- [x] Large enums and enums in complex data structures
- [x] Shadowing and deep module import hierarchies
- [x] Exhaustive pattern matching for complex enums

### 14.2 Feature Integration Tests
- [x] Generics in multi-file projects
- [x] FFI with custom struct types and pointers
- [x] Result type propagation across multiple function calls
- [x] Pointer safety across different scopes

### 14.3 Comprehensive Examples
- [x] Fibonacci (recursive)
- [ ] LinkedList implementation (pointers + structs)
- [ ] Vector3 math library (structs + methods-like functions)
- [ ] Minimal "JSON" parser style string processing

**Deliverable:** Comprehensive test suite and robust example library.

---

## Phase 15: Documentation & Polish

**Goal:** Finalize v0.1 release with documentation.

### 15.1 Documentation
- [ ] Write getting started guide
- [ ] Document compiler flags and options
- [ ] Create language tutorial
- [ ] Document standard library functions
- [ ] Write troubleshooting guide

### 15.2 Error Messages
- [ ] Improve error message clarity
- [ ] Add suggestions for common mistakes
- [ ] Implement error codes

### 15.3 Performance
- [ ] Benchmark compiler speed
- [ ] Benchmark generated code performance
- [ ] Optimize slow compilation paths

### 15.4 Release Preparation
- [ ] Version tagging (v0.1.0)
- [ ] Create release notes
- [ ] Package binaries for distribution
- [ ] Set up CI/CD pipeline

**Deliverable:** Virex v0.1 release ready for public use.

---

## Out of Scope for v0.1

The following features are defined in CORE.md but deferred to future versions:

### Deferred to v0.2+

**Phase 4 Semantic Analysis:**
- [ ] Basic lifetime analysis (prevent use-after-free)
- [ ] Validate `unsafe` block requirements
- [ ] Validate break/continue usage in loops

**Phase 6 Optimizations:**
- [ ] Inline small functions (simple heuristic)
- [ ] Tail call optimization
- [ ] Stack allocation for non-escaping heap objects (escape analysis)
- [ ] Remove unused variables

**Future Features:**
- [ ] Generic struct and enum support (Phase 10 extended)
- [ ] Advanced concurrency (`std::thread`, `std::sync`, `std::atomic`)
- [ ] Advanced optimizations (loop unrolling, vectorization)
- [ ] Multiple target platforms (Windows, macOS, ARM)
- [ ] Package manager
- [ ] Language server protocol (LSP) for IDE support
- [ ] Debugger integration (DWARF debug info)
- [ ] Incremental compilation
- [ ] Compile-time function execution (comptime)
- [ ] Advanced lifetime analysis
- [ ] Borrow checker (if desired)
- [ ] Native x86_64 backend (optional)
- [ ] LLVM backend (optional)

### Explicitly NOT in v0.1
- Collections (Vec, Map, etc.) - external libraries
- String types beyond `cstring` - external libraries
- Filesystem abstraction - external libraries
- Async runtime - external libraries
- Reflection - not planned
- Garbage collection - not planned

---

## Success Criteria for v0.1

A successful v0.1 release must:

1. ✅ Compile simple Virex programs to working x86_64 Linux executables
2. ✅ Support all core syntax (variables, functions, structs, control flow)
3. ✅ Implement basic type checking and safety analysis
4. ✅ Support generics via monomorphization
5. ✅ Provide a minimal but functional standard library
6. ✅ Support multi-file projects with modules
7. ✅ Enable C interop for calling external libraries
8. ✅ Include comprehensive tests and examples
9. ✅ Have clear documentation and error messages
10. ✅ Demonstrate the core philosophy: "Explicit control. Predictable speed. Minimal magic."

---

## Timeline Estimate

**Total estimated time:** 8-12 months (for a single developer working part-time)

- Phase 1-2 (Foundation + Lexer): 3-4 weeks
- Phase 3 (Parser): 4-5 weeks
- Phase 4 (Semantic Analysis): 4-5 weeks
- Phase 5 (IR): 3-4 weeks
- Phase 6 (Optimizations): 3-4 weeks
- Phase 7-8 (Code Generation + Linking): 8-10 weeks
- Phase 9 (Standard Library): 2-3 weeks
- Phase 10 (Generics): 4-5 weeks
- Phase 11 (Modules): 3-4 weeks
- Phase 12 (Result Types): 3-4 weeks
- Phase 13 (C Interop): 1-2 weeks (easier in C!)
- Phase 14 (Testing): 4-5 weeks
- Phase 15 (Documentation): 2-3 weeks

**Note:** Timeline assumes familiarity with C and compiler development. C implementation requires more manual work (data structures, memory management) but has zero learning curve. Adjust based on team size and experience.

---

## Next Steps

1. Set up the Rust project structure
2. Implement the lexer (Phase 2)
3. Begin parser development (Phase 3)
4. Iterate and test continuously

**Let's build Virex! 🚀**
