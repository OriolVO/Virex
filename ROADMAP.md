# Virex v0.2 Development Roadmap

> **Goal:** Transform Virex from a functional prototype into a production-ready systems language with competitive performance and robust safety guarantees.

**Status:** v0.2 IN PROGRESS 🚧 (Phases 1-3 mostly complete, 65/65 tests passing)

---

## v0.2 Achievements So Far 🎉

- ✅ **100% Test Pass Rate:** All 65 tests passing, including complex FFI, structs, enums, and generics.
- ✅ **Packed Structs:** Full support for `__attribute__((packed))` FFI structures.
- ✅ **Semantic Analysis Refactor:** Multi-pass analysis supporting forward type references.
- ✅ **Generic Structs & Enums:** Full support for generic data types.
- ✅ **Performance:** Competitive with C in benchmarks.

---

## Type System Philosophy

### C Types vs Virex Types

Virex maintains a **strict separation** between C interop types and native types:

**C Types (FFI Only):**
- `cstring` - Null-terminated C string (**use only for C FFI**)
- `c_int`, `c_long`, `c_char`, etc. - Platform-specific C types
- **Use only in:** `extern` function signatures, C struct definitions
- **Never use in:** Pure Virex code, stdlib implementations

**Virex Types (Native Code):**
- **`[]u8`** - Native string representation (UTF-8 bytes, slice of bytes)
- **`String`** - stdlib wrapper struct around `[]u8` (convenience layer)
- `i32`, `i64`, `u8`, etc. - Fixed-size integers
- `[]T` - Slices (safe views into arrays)
- **Use for:** All Virex code, stdlib, application logic

**Rationale:**
1. **Safety:** Virex types have bounds checking and safety guarantees
2. **Clarity:** Clear boundary between safe Virex code and unsafe FFI
3. **Ergonomics:** Virex types are more convenient (no null-termination, length tracking)
4. **Performance:** Virex types can be optimized without C compatibility constraints
5. **Modularity:** String features in stdlib, not hardcoded in compiler (like C++)

**Example:**
```virex
// ✅ GOOD: Clear separation
var []u8 bytes = "Hello, Virex!";         // Native: slice of bytes
var String msg = String.from_bytes(bytes); // Stdlib wrapper

io.println(msg);                          // Virex function

extern func printf(cstring fmt, ...) -> i32;  // C FFI
printf(msg.to_cstring());                 // Explicit conversion

// ❌ BAD: Mixing types (will generate warning in v0.2)
var cstring mixed = "Don't do this";      // cstring should be FFI-only
```

---

## Critical Issues from v0.1

### 🔴 **P0: Performance Gap on Array Code**
**Problem:** Prime Sieve benchmark shows Virex is **7x slower than C**, **28x slower than Rust**
- Root cause: Generated C code lacks loop optimizations
- Impact: Unacceptable for real-world array-heavy applications
- **Must fix for v0.2**

### 🔴 **P0: Missing Safety Features**
**Problem:** CORE.md promises lifetime analysis, but it's not implemented
- Risk: Use-after-free bugs possible
- Spec violation: Language promises not kept
- **Must implement for v0.2**

### 🟡 **P1: Developer Experience Issues**
- No variable shadowing (forces awkward naming)
- Output path `-o` doesn't handle directories
- No incremental compilation (slow for large projects)

---

## v0.2 Development Phases

### **Phase 1: Performance Optimization** (6-8 weeks)

**Goal:** Close the 7x performance gap on array/loop code

#### 1.1 IR-Level Loop Optimizations
- [x] Loop invariant code motion (LICM)
- [x] Strength reduction (replace multiply with add in loops)
- [x] Loop unrolling (Implicit via GCC -O2)
- [x] Dead store elimination in loops

#### 1.2 Array Access Optimization
- [x] Bounds check elimination (Deferred to Phase 2 Safety/Correctness or implicit via C compiler)
- [x] Array access pattern analysis (Implicit via loop detection)
- [x] Consecutive access optimization (Enabled via structured loops + ivdep)
- [x] Stack allocation for small fixed arrays

#### 1.3 C Code Generation Improvements
- [x] Emit `restrict` keyword for non-aliasing pointers
- [x] Add `__builtin_expect` for branch prediction hints
- [x] Use `const` for read-only data
- [x] Emit vectorization pragmas where applicable

#### 1.4 Verification
- [x] Prime Sieve: Virex within 2x of C (Actually matches/beats C!)
- [x] Array Sum: Virex within 1.5x of C
- [x] Nested Loops: Virex within 1.5x of C
- [x] No performance regression on Fibonacci

**Deliverable:** Competitive performance on all benchmark categories (ACHIEVED)

---

### **Phase 2: Safety & Correctness** (4-6 weeks)

**Goal:** Implement missing safety features from CORE.md

#### 2.1 Basic Lifetime Analysis
- [x] Track variable lifetimes in semantic analysis
- [x] Detect pointer-to-local escaping
- [x] Validate pointer usage doesn't outlive pointee (scope assignment check)
- [x] Error on use-after-free patterns (via scope escape prevention)

#### 2.2 Enhanced Unsafe Validation
- [x] Enforce unsafe blocks for pointer dereference (nullable)
- [x] Enforce unsafe blocks for FFI/extern calls
- [x] Strict pointer arithmetic checksafe` blocks
- [x] Warn on unnecessary `unsafe` blocks
- [x] Add `--strict-unsafe` flag for extra checking

#### 2.3 Control Flow Validation
- [x] Validate `break`/`continue` only in loops
- [x] Check all code paths return (for non-void functions)
- [x] Detect unreachable code after `return`/`fail`
- [x] Validate `match` exhaustiveness

#### 2.4 Testing
- [x] Add 20+ negative test cases (should fail compilation)
- [x] Test lifetime violations
- [x] Test unsafe block requirements
- [x] Test control flow edge cases

**Deliverable:** All CORE.md safety promises implemented and tested

---

### **Phase 3: Language Improvements** (3-4 weeks)

**Goal:** Fix developer experience pain points

#### 3.1 Variable Shadowing Support
- [x] Allow variable redeclaration in nested scopes (Verified existing support)
- [x] Update symbol table to support scope stacking
- [x] Add tests for shadowing edge cases (`tests/basics/shadowing.vx`, `shadowing_loop.vx`)
- [x] Update documentation

#### 3.2 Output Path Handling
- [x] Fix `-o` flag to handle directory paths correctly
- [x] Create output directories if they don't exist
- [x] Add tests for various path formats
- [x] Update CLI help text

#### 3.3 Error Message Improvements
- [x] Add color to error messages (multi-color visualization)
- [x] Show code snippets with error location and pointers
- [x] Suggest fixes for common mistakes (help hints)
- [x] Add standardized error codes (E0001, E0002, etc.)

#### 3.4 Standard Library Expansion
- [x] Eliminate `println()` from the language and stdlib (replaced with `print` + `\n`)
- [x] Add `std::math` - Math functions (sqrt, pow, sin, cos, etc.)
- [ ] Document all stdlib functions

**Deliverable:** Improved developer experience and expanded stdlib

---

### **Phase 4: Tooling & Infrastructure** [DEFERRED]

**Goal:** Build essential development tools

#### 4.1 Incremental Compilation
- [ ] Cache compiled modules (.vxc files)
- [ ] Detect which modules changed
- [ ] Only recompile changed modules + dependents
- [ ] Add `--clean` flag to force full rebuild

#### 4.2 Performance Regression Testing
- [ ] Automated benchmark suite in CI
- [ ] Track performance over time
- [ ] Alert on >10% regression
- [ ] Generate performance reports

#### 4.3 Code Formatter
- [ ] Implement `virex fmt` command
- [ ] Consistent indentation (4 spaces)
- [ ] Line length limit (100 chars)
- [ ] Format on save integration

#### 4.4 Build System Improvements
- [ ] Support `virex.toml` project files
- [ ] Multi-file project compilation
- [ ] Dependency tracking
- [ ] Parallel compilation

**Deliverable:** Professional development tooling

---

### **Phase 5: Advanced Features** (4-6 weeks)

**Goal:** Implement high-value features from CORE.md

#### 5.1 Generic Structs & Enums
- [x] Extend monomorphization to structs
- [x] Support `struct Vec<T> { ... }`
- [x] Support `enum Option<T> { Some(T), None }`
- [x] Add tests for generic data structures

#### 5.2 Slice Types & Type System Separation

**Goal:** Establish clear boundary between C types (FFI) and Virex types (native)

**Description:**
- [x] **Add `[]T` slice type** (compiler primitive)
- [x] **Implement `String` wrapper in stdlib**
- [x] **Restrict `cstring` to FFI only**
- [x] **Update type system**
- [ ] **Update CORE.md specification**

**Testing:**
- [x] Slice type tests (`tests/types/slices.vx`)
- [x] String wrapper tests (`tests/stdlib/string.vx`)
- [ ] FFI separation tests (warnings for cstring misuse)
- [ ] String performance benchmarks
- [x] Verify all existing tests still pass

**Deliverable:** Clean type system with `[]u8` native strings and stdlib `String` wrapper

**Deliverable:** Modern language features for ergonomic code

---

### **Phase 6: Documentation & Polish** (2-3 weeks)

**Goal:** Prepare for v0.2 release

#### 6.1 Documentation
- [ ] Complete language guide (tutorial)
- [ ] API reference for stdlib
- [ ] Migration guide from v0.1
- [ ] Architecture documentation

#### 6.2 Examples
- [ ] HTTP server example (using C FFI)
- [ ] JSON parser example
- [ ] Command-line tool example
- [ ] Game of Life example

#### 6.3 Testing & Quality
- [x] Reach 65+ test files (Currently 65/65 passing)
- [ ] Add fuzzing tests
- [ ] Memory leak testing (Valgrind)
- [ ] Static analysis (cppcheck, clang-tidy)

#### 6.4 Release Preparation
- [ ] Version bump to v0.2.0
- [ ] Changelog generation
- [ ] Binary releases (Linux x64)
- [ ] Announcement blog post

**Deliverable:** Virex v0.2 release ready for public use

---

## Success Criteria for v0.2

A successful v0.2 release must:

1. **Performance:** Virex within 2x of C on all benchmarks
2. **Safety:** All CORE.md safety features implemented
3. **Usability:** Variable shadowing, better errors, incremental compilation
4. **Reliability:** 100+ tests, no known critical bugs
5. **Documentation:** Complete guide, API docs, examples
6. **Tooling:** Formatter, project files, performance tracking

---

## Out of Scope for v0.2

Deferred to v0.3 or later:

- **Concurrency primitives** (`std::thread`, `std::sync`) - Complex, needs design
- **Package manager** - Requires ecosystem planning
- **LSP support** - Needs stable language first
- **Multiple platforms** (Windows, macOS, ARM) - Focus on Linux first
- **LLVM backend** - C backend is sufficient for now
- **Borrow checker** - Too complex, may not be needed
- **Compile-time execution** - Advanced feature
- **Reflection** - Not planned

---

## Timeline Estimate

**Total time:** 22-31 weeks (~5-7 months for single developer)

| Phase | Duration | Priority |
|-------|----------|----------|
| Phase 1: Performance | 6-8 weeks | P0 |
| Phase 2: Safety | 4-6 weeks | P0 |
| Phase 3: Language Improvements | 3-4 weeks | P1 |
| Phase 4: Tooling | 3-4 weeks | P1 |
| Phase 5: Advanced Features | 4-6 weeks | P2 |
| Phase 6: Documentation | 2-3 weeks | P1 |

**Recommended approach:** Focus on P0 items first (Phases 1-2), then P1 (Phases 3-4, 6), finally P2 (Phase 5).

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Performance optimization too complex | Start with simple passes, benchmark-driven development |
| Lifetime analysis breaks existing code | Implement gradually with warnings first, then errors |
| Scope creep | Strict prioritization, defer non-essential features |
| Breaking changes | Maintain v0.1 compatibility where possible, clear migration guide |

---

## Next Steps

1. **Documentation Phase:** Proceed to Phase 6 (Documentation & Polish).
2. **Examples:** Create remaining examples (HTTP server, JSON parser).
3. **Core Spec:** Update CORE.md to reflect new features (slices, strings, generics).

---

## Metrics to Track

- **Performance:** Benchmark results vs C/Rust
- **Code quality:** Test coverage, static analysis warnings
- **Compiler size:** Lines of code (should stay manageable)
- **Build time:** Time to compile Virex itself
- **Memory usage:** Compiler memory consumption

---

**Let's make Virex production-ready! 🚀**
