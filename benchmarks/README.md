# Virex Language Benchmarks

This directory contains performance benchmarks comparing Virex against C, Rust, and Lua.

## Overview

The benchmark suite measures execution time for identical algorithms implemented in all four languages. This helps validate that Virex's C backend produces competitive native code.

## Benchmark Categories

### 1. **Fibonacci (Recursive)**
- **Tests:** Function call overhead, recursion performance
- **Algorithm:** Classic recursive Fibonacci
- **Input:** `fib(35)` - chosen for ~1-2 second execution time
- **Expected Results:** Virex ≈ C ≈ Rust >> Lua

### Future Benchmarks (Planned)
- **Prime Sieve** - Loop and array performance
- **Binary Trees** - Memory allocation/deallocation
- **Matrix Multiplication** - Nested loops and cache performance
- **String Processing** - Pointer operations

## Running Benchmarks

### Prerequisites

```bash
# Install required tools
sudo apt-get install bc time gcc

# Optional: Install Rust for Rust benchmarks
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Optional: Install Lua for Lua benchmarks
sudo apt-get install lua5.4
```

### Run All Benchmarks

```bash
cd benchmarks
./run_benchmarks.sh
```

### Run Single Benchmark Manually

```bash
# Virex
../virex build fibonacci/fibonacci.vx -o fibonacci/fib_virex
/usr/bin/time -f "Time: %e seconds" ./fibonacci/fib_virex

# C
gcc -O2 fibonacci/fibonacci.c -o fibonacci/fib_c
/usr/bin/time -f "Time: %e seconds" ./fibonacci/fib_c

# Rust
rustc -O fibonacci/fibonacci.rs -o fibonacci/fib_rs
/usr/bin/time -f "Time: %e seconds" ./fibonacci/fib_rs

# Lua
/usr/bin/time -f "Time: %e seconds" lua fibonacci/fibonacci.lua
```

## Timing Methodology

### Why `/usr/bin/time`?

The benchmark script uses `/usr/bin/time` (not the shell built-in `time`) because:
- ✅ Provides **millisecond precision** (`%e` format gives decimals)
- ✅ Consistent across all languages
- ✅ Measures **wall-clock time** (real execution time)
- ✅ Standard benchmarking practice

### Format String

```bash
/usr/bin/time -f "%e" ./program
```

- `%e` = Elapsed real time in seconds (with decimal precision)
- Example output: `0.847` (847 milliseconds)

### Multiple Iterations

The script runs each benchmark **5 times** and reports:
- **Average** time (primary metric)
- **Min** time (best case)
- **Max** time (worst case)

This accounts for system noise and provides more reliable results.

## Expected Results

Based on Virex's compilation pipeline (Virex → C → gcc):

| Language | Expected Performance | Reason |
|----------|---------------------|---------|
| **Virex** | ~1.0x (baseline) | Compiles to C with gcc -O2 |
| **C** | ~1.0x | Direct gcc -O2 compilation |
| **Rust** | ~0.9-1.1x | LLVM optimizations, similar to C |
| **Lua** | ~20-50x slower | Interpreted/JIT, not native |

### Why Virex ≈ C?

Virex generates C code, so performance should be nearly identical to hand-written C:

```
Virex Source → C Code → gcc -O2 → Native Binary
C Source → gcc -O2 → Native Binary
```

Any performance difference would indicate:
- Suboptimal C code generation
- Missed optimization opportunities
- Areas for compiler improvement

## Benchmark Structure

```
benchmarks/
├── run_benchmarks.sh          # Main benchmark runner
├── README.md                  # This file
└── fibonacci/
    ├── fibonacci.vx           # Virex implementation
    ├── fibonacci.c            # C implementation
    ├── fibonacci.rs           # Rust implementation
    └── fibonacci.lua          # Lua implementation
```

## Adding New Benchmarks

1. Create a new directory: `benchmarks/my_benchmark/`
2. Implement the algorithm in all languages:
   - `my_benchmark.vx`
   - `my_benchmark.c`
   - `my_benchmark.rs`
   - `my_benchmark.lua`
3. Add to `run_benchmarks.sh`:
   ```bash
   benchmark_suite "my_benchmark" "Description of what it tests"
   ```

## Compilation Flags

### Virex
- Default flags (currently equivalent to gcc -O2)

### C
- `-O2` - Standard optimization level
- `-lm` - Link math library if needed

### Rust
- `-O` or `--release` - Release mode optimizations

### Lua
- No compilation (interpreted)

## Interpreting Results

### Good Results
- Virex time ≈ C time (within 10%)
- Virex time ≈ Rust time (within 20%)
- All native languages >> Lua

### Concerning Results
- Virex significantly slower than C (>20% difference)
  - Indicates C codegen issues
  - Check generated `virex_out.c` for inefficiencies

### Optimization Opportunities
If Virex is slower than C, investigate:
1. Generated C code quality
2. Missing compiler optimizations
3. Unnecessary temporary variables
4. Suboptimal control flow

## Future Improvements

- [ ] Add more benchmark categories (memory, I/O, data structures)
- [ ] Generate HTML report with graphs
- [ ] Track performance over time (regression testing)
- [ ] Add memory usage benchmarks
- [ ] Compare binary sizes
- [ ] Add compilation time benchmarks
