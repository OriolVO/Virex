# Virex Roadmap

## Philosophy
- **Explicit Control**: No hidden allocations or magic behavior.
- **Predictable Speed**: What you see is what you get.
- **Minimal Magic**: The core language is small; complex features are built in the standard library.
- **Pure Implementation**: Core features like Strings and dynamic arrays are implemented in Virex itself.

---

## Phase 0.3: Core Refinement & FFI Architecture (CURRENT)

### 1. Type System Extensions
- [x] **Type Aliases**: Implement `type Alias = TargetType;` syntax.
    -   [x] Add `TOKEN_TYPE` keyword.
    -   [x] Update AST for `ASTTypeAlias`.
    -   [x] Implement parser and semantic analysis support.
    -   [x] Support public export: `public type c_int = i32;`.

### 2. Standard Library FFI (`std::ffi`)
- [x] **Migrate C Types**: Remove built-in C types (`c_int`, `c_char`, etc.) from the compiler.
    -   [x] Define them as type aliases in `std/ffi.vx`.
    -   [x] Example: `public type c_int = i32;`, `public type c_void = void;`.
- [x] **Explicit Interop**:
    -   [x] Users import `std::ffi` to access C types (or use raw pointers).
    -   [x] C function calls (`extern`) must be inside `unsafe` blocks.
    -   [x] `cstring` was removed in favor of `u8*` for better C compatibility and less compiler magic.

### 3. Minimal Core Validation
- [x] **Audit Parser & AST**: Ensure no higher-level constructs (like managed strings/arrays) are hardcoded in the compiler.
- [x] **Literal Cleanup**:
    -   [x] **Constraint**: String literals `"foo"` must be treated as `[]u8` (byte slices) in pure Virex.
    -   [x] **Implementation**: Compiler generates static byte array + slice.
    -   [x] **FFI Exception**: Slices (`[]u8`) are implicitly converted to `u8*` ONLY when passed directly to `extern` C functions.
    -   [x] **Cast Removal**: The `cast` keyword was removed; use explicit types or implicit pointer conversions for FFI.

### 4. Testing & Examples
- [x] **Interop Demo**: Verified with `tests/ffi/printf.vx` and `tests/ffi/string_ffi.vx`.
- [x] **Migration**: All tests updated to use `u8*` and explicit `unsafe` blocks. 100% test success (68/68).

---

## Phase 0.4: LLVM Backend Preparation (PLANNED)
- [ ] **LLVM IR Generation**: Begin work on generating LLVM IR instead of C.
- [ ] **Memory Safety**: Smart pointers (`Box<T>`, `Rc<T>`) implemented in pure Virex.
