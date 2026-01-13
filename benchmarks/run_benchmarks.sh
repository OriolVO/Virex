#!/bin/bash
# Virex Language Benchmarks
# Compares Virex performance against C, Rust, and Lua

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Number of iterations for averaging
ITERATIONS=3

# Benchmark results directory
RESULTS_DIR="benchmark_results"
mkdir -p "$RESULTS_DIR"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         Virex Language Performance Benchmarks             ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Function to run a benchmark and get average time
run_benchmark() {
    local name=$1
    local cmd=$2
    local color=$3
    
    echo -e "${color}Running: $name${NC}"
    
    local total=0
    local times=()
    
    for i in $(seq 1 $ITERATIONS); do
        # Use /usr/bin/time with custom format for precise timing
        # %e = elapsed real time in seconds (with decimals)
        local elapsed=$(/usr/bin/time -f "%e" $cmd 2>&1 >/dev/null | tail -n 1)
        times+=($elapsed)
        total=$(echo "$total + $elapsed" | bc)
        echo -e "  Iteration $i: ${elapsed}s"
    done
    
    local avg=$(echo "scale=6; $total / $ITERATIONS" | bc)
    
    # Calculate min and max
    local min=${times[0]}
    local max=${times[0]}
    for t in "${times[@]}"; do
        if (( $(echo "$t < $min" | bc -l) )); then
            min=$t
        fi
        if (( $(echo "$t > $max" | bc -l) )); then
            max=$t
        fi
    done
    
    echo -e "  ${GREEN}Average: ${avg}s${NC} (min: ${min}s, max: ${max}s)"
    echo ""
    
    # Return average for comparison
    echo "$avg"
}

# Function to compile and benchmark
benchmark_suite() {
    local bench_name=$1
    local description=$2
    
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}Benchmark: $bench_name${NC}"
    echo -e "${YELLOW}Description: $description${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo ""
    
    # Compile all versions
    echo -e "${BLUE}Compiling all versions...${NC}"
    
    # Virex
    if [ -f "${bench_name}/${bench_name}.vx" ]; then
        (cd .. && ./virex build "benchmarks/${bench_name}/${bench_name}.vx" 2>&1 | grep -E "(✓|Error)" || true)
        # Move the output file to the correct location
        if [ -f "../${bench_name}" ]; then
            mv "../${bench_name}" "${bench_name}/${bench_name}_virex"
        fi
    fi
    
    # C
    if [ -f "${bench_name}/${bench_name}.c" ]; then
        gcc -O2 -o "${bench_name}/${bench_name}_c" "${bench_name}/${bench_name}.c" -lm
        echo "✓ Compiled C version"
    fi
    
    # Rust
    if [ -f "${bench_name}/${bench_name}.rs" ]; then
        rustc -O -o "${bench_name}/${bench_name}_rs" "${bench_name}/${bench_name}.rs" 2>/dev/null
        echo "✓ Compiled Rust version"
    fi
    
    echo ""
    echo -e "${BLUE}Running benchmarks (${ITERATIONS} iterations each)...${NC}"
    echo ""
    
    # Run benchmarks
    local virex_time=""
    local c_time=""
    local rust_time=""
    local lua_time=""
    
    if [ -f "${bench_name}/${bench_name}_virex" ]; then
        virex_time=$(run_benchmark "Virex" "./${bench_name}/${bench_name}_virex" "$GREEN")
    fi
    
    if [ -f "${bench_name}/${bench_name}_c" ]; then
        c_time=$(run_benchmark "C (gcc -O2)" "./${bench_name}/${bench_name}_c" "$BLUE")
    fi
    
    if [ -f "${bench_name}/${bench_name}_rs" ]; then
        rust_time=$(run_benchmark "Rust (--release)" "./${bench_name}/${bench_name}_rs" "$RED")
    fi
    
    if [ -f "${bench_name}/${bench_name}.lua" ]; then
        lua_time=$(run_benchmark "Lua" "lua ${bench_name}/${bench_name}.lua" "$YELLOW")
    fi
    
    # Summary
    echo -e "${YELLOW}───────────────────────────────────────────────────────────${NC}"
    echo -e "${YELLOW}Summary for $bench_name:${NC}"
    echo ""
    
    [ -n "$virex_time" ] && echo -e "  ${GREEN}Virex:${NC}        ${virex_time}s"
    [ -n "$c_time" ] && echo -e "  ${BLUE}C:${NC}            ${c_time}s"
    [ -n "$rust_time" ] && echo -e "  ${RED}Rust:${NC}         ${rust_time}s"
    [ -n "$lua_time" ] && echo -e "  ${YELLOW}Lua:${NC}          ${lua_time}s"
    
    # Calculate relative performance
    if [ -n "$virex_time" ] && [ -n "$c_time" ]; then
        local ratio=$(echo "scale=2; $virex_time / $c_time" | bc)
        echo ""
        echo -e "  ${GREEN}Virex vs C:${NC}   ${ratio}x"
    fi
    
    echo ""
}

# Check dependencies
echo -e "${BLUE}Checking dependencies...${NC}"
command -v bc >/dev/null 2>&1 || { echo -e "${RED}Error: 'bc' is required but not installed.${NC}" >&2; exit 1; }
test -x /usr/bin/time || { echo -e "${RED}Error: '/usr/bin/time' is required but not installed.${NC}" >&2; exit 1; }
echo -e "${GREEN}✓ All dependencies found${NC}"
echo ""

# Run benchmark suites
benchmark_suite "fibonacci" "Recursive Fibonacci (tests function call overhead)"
benchmark_suite "primes" "Prime number sieve (tests loops and arrays)"
benchmark_suite "arraysum" "Array summation (tests memory access patterns)"
benchmark_suite "nestedloops" "Nested loops (tests control flow performance)"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║              Benchmarks Complete!                         ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
