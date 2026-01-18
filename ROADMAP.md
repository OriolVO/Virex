# Virex Roadmap

## Philosophy
- **Explicit Control**: No hidden allocations or magic behavior.
- **Predictable Speed**: What you see is what you get.
- **Minimal Magic**: The core language is small; complex features are built in the standard library.
- **Pure Implementation**: Core features like Strings and dynamic arrays are implemented in Virex itself.

---

## Implementation Status

### ✅ Fully Implemented
- Core types (i8-i64, u8-u64, f32/f64, bool)
- Variables (`var`/`const`), functions, control flow
- Structs, enums, arrays, slices (`[]T`)
- Pointers (`*T`) with address-of and dereference
- Generics with monomorphization (functions and types)
- `result<T, E>` with `match` statements
- Type aliases (`type Name = Target`)
- Modules and imports with visibility
- FFI (`extern` functions, C ABI types, packed structs)
- For-in loops over slices
- `unsafe` blocks (basic enforcement)
- IR generation with C backend

### ⚠️ Partially Implemented
- Non-null pointers (`*!T`) - parser support, limited semantic validation
- `unsafe` validation - only checks 3 operations, no recursive validation
- Error messages - basic but lacking context

### ❌ Not Implemented (from CORE.md spec)
- Threads (`std::thread`)
- Atomic types (`atomic_i32`, etc.)
- Synchronization (`mutex<T>`, `rwlock<T>`)
- File I/O (`std::fs`)
- Build system with project files

---

## Phase 1.0: Language Completion (CURRENT)

Focus: **Complete the CORE.md specification** before adding new features.

### 1.1 Non-Null Pointer Enforcement ⚡ Priority
- [x] **Safe Dereference**: Allow `*p` without `unsafe` for `*!T` pointers
- [x] **Null Assignment Prevention**: Forbid `null` assignment to `*!T`
- [x] **Initialization Requirement**: Ensure `*!T` is always initialized
- [x] **Type Inference**: Infer non-null when taking address of valid object

### 1.2 Unsafe Block Improvements
- [x] **Recursive Validation**: Validate all nested expressions in unsafe
- [x] **Scope Tracking**: Track unsafe operations through function calls
- [x] **Warning Precision**: Only warn when truly unnecessary

### 1.3 Compiler Robustness
- [x] **Fix Segfaults**: Address known crash in strict_ctypes.vx test
- [x] **Monomorphization Edge Cases**: Symbol consistency in all scenarios
- [x] **Memory Safety**: Analyzed leaks (~1.9KB acceptable, shared type ownership)

### 1.4 C Backend Refinements
- [x] **Alias Resolution**: Emit underlying types for aliases (e.g., `i32` for `c_int`) to ensure C compatibility.
- [x] **Type Definition Ordering**: Ensure dependent structs are generated in correct order (already working - AST preserves source order).

---

## Phase 1.1: Developer Experience

Focus: **Make Virex usable for real projects.**

### 1.1.1 Error Messages & Diagnostics
- [x] **Contextual Errors**: Show source snippets with caret
- [x] **Suggestions**: "Did you mean X?" for common mistakes
- [x] **Notes**: Add explanatory notes for complex errors
- [x] **Color Output**: Syntax-highlighted diagnostics

### 1.1.2 Basic Tooling
- [x] **Project Configuration**: `virex.toml` for multi-file projects
- [x] **Build Command**: `virex build` reads project config (CLI wrapper created)
- [x] **Watch Mode**: `virex watch` for auto-rebuild
- [x] **Formatter**: `virex fmt` for consistent style

### 1.1.3 Documentation
- [ ] **Language Reference**: Complete syntax specification
- [ ] **Standard Library Docs**: All modules documented
- [ ] **Tutorial**: Getting started guide
- [ ] **Examples**: Real programs demonstrating features

---

## Phase 1.2: Essential Standard Library

Focus: **Implement remaining CORE.md stdlib modules.**

### 1.2.1 File I/O (`std::fs`)
- [ ] `func read_file(path: []u8) -> result<[]u8, i32>`
- [ ] `func write_file(path: []u8, data: []u8) -> result<void, i32>`
- [ ] `func exists(path: []u8) -> bool`
- [ ] `func remove(path: []u8) -> result<void, i32>`

### 1.2.2 Result Utilities (expand `std::result`)
- [ ] `func map<T, U, E>(r: result<T, E>, f: func(T) -> U) -> result<U, E>`
- [ ] `func and_then<T, U, E>(r: result<T, E>, f: func(T) -> result<U, E>) -> result<U, E>`
- [ ] `func is_ok<T, E>(r: result<T, E>) -> bool`
- [ ] `func is_err<T, E>(r: result<T, E>) -> bool`

### 1.2.3 String Utilities (expand `std::string`)
- [ ] **Basic Formatting**: Simple format function
- [ ] **Comparison**: Lexicographic comparison
- [ ] **Search**: `contains`, `starts_with`, `ends_with`
- [ ] **Manipulation**: `trim`, `split`

### 1.2.4 OS Utilities (expand `std::os`)
- [ ] Command-line arguments
- [ ] Environment variables
- [ ] Process exit with code

---

## Phase 2.0: Concurrency (Future)

**Note**: Per CORE.md, concurrency is "explicit and opt-in" via stdlib.

### 2.1 Thread Support (`std::thread`)
- [ ] `func spawn(f: func() -> void) -> thread`
- [ ] `func join(t: thread*) -> void`
- [ ] Thread-local storage

### 2.2 Synchronization (`std::sync`)
- [ ] `mutex<T>` - mutual exclusion
- [ ] `rwlock<T>` - reader-writer lock
- [ ] Condition variables

### 2.3 Atomics (`std::atomic`)
- [ ] `atomic_i32`, `atomic_u64`, etc.
- [ ] `load`, `store`, `fetch_add`, `fetch_sub`
- [ ] Memory ordering options

---

## Phase 3.0: Backend Evolution (Future)

### 3.1 IR Improvements
- [ ] **Canonical IR Format**: Structured, serializable
- [ ] **IR Verification**: Validate IR correctness
- [ ] **IR Optimization Passes**: Dead code, constant folding, inlining

### 3.2 Alternative Backends
- [ ] **LLVM Backend**: Prototype LLVM IR generation
- [ ] **Direct x86-64**: Native code without C intermediate

---

## Explicitly NOT Planned

Per CORE.md philosophy and analysis:

| Feature | Reason |
|---------|--------|
| Smart Pointers (`Box`, `Rc`, `Arc`) | Conflicts with manual memory philosophy |
| Garbage Collection | Explicit control, no hidden behavior |
| Collections in stdlib | External packages preferred |
| Async/Await | Explicit threading only |
| Reflection | Compile-time only |
| Exceptions | Result types used instead |

---

## Version Milestones

| Version | Focus | Key Features |
|---------|-------|--------------|
| 0.4 | Refactoring | ✅ Complete - Monomorphization, name mangling |
| 1.0 | Language Complete | Non-null pointers, unsafe cleanup, stability |
| 1.1 | Usability | Tooling, diagnostics, documentation |
| 1.2 | Stdlib | File I/O, result helpers, strings |
| 2.0 | Concurrency | Threads, atomics, synchronization |
| 3.0 | Backends | LLVM, native codegen |

---

*Last updated: 2026-01-16*
