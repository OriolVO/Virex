# Virex

**Explicit control. Predictable speed. Minimal magic.**

Virex is a compiled, statically typed, manual-memory systems language designed for clarity and performance. Inspired by C, Zig, and Rust, but with an original, readable syntax.

## Status

🚧 **In Development** - Currently implementing Phase 1 (Foundation & Tooling)

**Version:** 0.1.0-dev

## Features

- ✅ **Statically typed** - All types known at compile time
- ✅ **Manual memory management** - Explicit control over allocations
- ✅ **Null safety** - Nullable (`*T`) vs non-null (`*!T`) pointers
- ✅ **No exceptions** - Result types for error handling
- ✅ **Generics** - Compile-time monomorphization
- ✅ **C interop** - Direct FFI with C libraries
- ✅ **AOT compiled** - Native machine code, no runtime

## Building

### Prerequisites

- GCC or Clang (C11 support)
- Make
- curl (for downloading helper libraries)

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd Virex

# Build the compiler
make

# Run the compiler
./virex --version
./virex --help
```

### Build Targets

```bash
make          # Build the compiler (default)
make clean    # Remove build artifacts
make debug    # Build with debug symbols
make release  # Build optimized release version
make test     # Run tests (when implemented)
```

## Usage

```bash
# Compile a Virex program
virex build main.vx

# Show version
virex --version

# Show help
virex --help
```

## Project Structure

```
virex/
├── src/           # Compiler source code
├── include/       # Public header files
├── lib/           # Third-party helper libraries
├── tests/         # Test files
├── examples/      # Example Virex programs
├── build/         # Build artifacts (gitignored)
├── Makefile       # Build system
├── CORE.md        # Language specification
└── ROADMAP.md     # Development roadmap
```

## Documentation

- [CORE.md](CORE.md) - Complete language specification
- [ROADMAP.md](ROADMAP.md) - Development roadmap and timeline

## Development Status

### Phase 1: Foundation & Tooling ✅ Complete
- [x] Project structure
- [x] Makefile build system
- [x] Basic CLI interface
- [x] Helper libraries integration
- [x] Testing framework

### Phase 2: Lexer & Tokenization ✅ Complete
- [x] Token types
- [x] Lexer implementation
- [x] Error reporting

### Phase 3: Parser & AST Construction ✅ Complete
- [x] Expression parsing
- [x] Statement parsing
- [x] AST nodes
- [x] Type system

### Phase 4: Semantic Analysis ✅ Complete
- [x] Symbol table
- [x] Type checking
- [x] Pointer safety validation

### Phase 5: IR Generation ✅ Complete
- [x] IR instruction set
- [x] AST → IR lowering
- [x] `--emit-ir` flag

### Phase 6: Optimizations ✅ Complete
- [x] Constant folding
- [x] Dead code elimination
- [x] Copy propagation

### Phase 7: C Code Generation ✅ Complete
- [x] IR → C translation
- [x] C code generator

### Phase 8: Linking ✅ Complete
- [x] Automatic gcc compilation
- [x] Executable generation

### Phase 8: Linking ✅ Complete
- [x] Automatic gcc compilation
- [x] Executable generation

### Phase 9: Standard Library ✅ Complete
- [x] Memory functions (alloc, free, copy, set)
- [x] I/O functions (print/println for i32, bool, string)
- [x] OS functions (exit, argc/argv)

### Phase 10: Generics 🔜
- [ ] Generic type parameters
- [ ] Monomorphization

## Quick Start

```bash
# Build the compiler
make

# Compile a Virex program
./virex build examples/fibonacci.vx

# Run the executable
./examples/fibonacci
```

## Compiler Pipeline

```
Virex Source (.vx)
    ↓ Lexer
Tokens
    ↓ Parser
AST (Abstract Syntax Tree)
    ↓ Semantic Analyzer
Type-Checked AST
    ↓ IR Generator
Intermediate Representation
    ↓ Optimizer (constant folding, DCE, copy propagation)
Optimized IR
    ↓ C Code Generator
C Source (.c)
    ↓ GCC (automatic)
Native Executable
```

## Features Implemented

✅ **Complete Frontend**
- Lexer with all tokens
- Recursive descent parser
- Full AST representation
- Type system (primitives, pointers, arrays, structs, enums, functions)

✅ **Semantic Analysis**
- Symbol table with scopes
- Type checking
- Pointer safety validation
- Array bounds checking (compile-time)
- Unreachable code detection

✅ **Optimization**
- Three-address code IR
- Constant folding
- Dead code elimination
- Copy propagation

✅ **Code Generation**
- C backend (bootstrap-friendly)
- Automatic compilation
- Runtime library

✅ **Standard Library**
- `std::mem` - Memory management
- `std::io` - Console I/O
- `std::os` - OS interaction

## Statistics

- **Total Lines of Code:** ~4,100
- **Compiler Source:** ~3,500 lines (C)
- **Runtime Library:** ~100 lines (C)
- **Standard Library:** ~50 lines (Virex)
- **Build Time:** <1 second
- **Performance:** Equal to C (via gcc optimization)

## Example Code

```virex
func main() -> i32 {
    var i32 x = 42;
    var i32* p = &x;
    
    unsafe {
        *p = 100;
    }
    
    return x;
}
```

## Contributing

This project is in early development. Contributions welcome once Phase 1 is complete.

## License

TBD

## Contact

TBD
