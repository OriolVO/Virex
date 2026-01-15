#!/bin/bash

# Virex Test Runner
# Automatically finds and runs all .vx tests

BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0
TOTAL=0

echo -e "${BOLD}Running Virex Test Suite...${NC}\n"

# Rebuild compiler
echo "Rebuilding compiler..."
make clean > /dev/null
make > /dev/null
if [ $? -ne 0 ]; then
    echo -e "${RED}Compiler build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}Compiler ready.${NC}\n"

# Find all .vx files in tests/ (excluding helper files)
TEST_FILES=$(find tests -name "*.vx" | grep -v "helper" | grep -v "lib.vx")

for test in $TEST_FILES; do
    TOTAL=$((TOTAL + 1))
    test_name=$(basename "$test")
    
    echo -n "Running $test... "
    
    # Compile
    bin_out=$(echo "$test_name" | cut -f 1 -d '.')
    if [[ "$test_name" == "ffi_structs.vx" ]]; then
       gcc -c tests/ffi/struct_helper.c -o struct_helper.o
       ./virex build "$test" -o "$bin_out" struct_helper.o > /dev/null 2>&1
       rm -f struct_helper.o
    elif [[ "$test_name" == "packed_struct.vx" ]]; then
       gcc -c tests/ffi/packed_helper.c -o packed_helper.o
       ./virex build "$test" -o "$bin_out" packed_helper.o > /dev/null 2>&1
       rm -f packed_helper.o
    else
       ./virex build "$test" -o "$bin_out" > /dev/null 2>&1
    fi
    if [ $? -ne 0 ]; then
        # Check if it was supposed to fail
        if [[ "$test_name" == "visibility.vx" ]] || [[ "$test_name" == "circular_a.vx" ]] || [[ "$test_name" == "circular_b.vx" ]] || [[ "$test_name" == "escape_local.vx" ]] || [[ "$test_name" == "unsafe_ffi.vx" ]] || [[ "$test_name" == "scope_escape.vx" ]] || [[ "$test_name" == "missing_return.vx" ]] || [[ "$test_name" == "missing_return_if.vx" ]] || [[ "$test_name" == "unreachable.vx" ]] || [[ "$test_name" == "invalid_break.vx" ]] || [[ "$test_name" == "match_missing.vx" ]] || [[ "$test_name" == "const_reassignment.vx" ]] || [[ "$test_name" == "type_mismatch.vx" ]] || [[ "$test_name" == "arg_count_mismatch.vx" ]] || [[ "$test_name" == "enhanced_error_test.vx" ]] || [[ "$test_name" == "strict_ctypes.vx" ]]; then
            echo -e "${GREEN}PASSED${NC} (Expected failure)"
            PASSED=$((PASSED + 1))
        else
            echo -e "${RED}FAILED${NC} (Compilation error)"
            FAILED=$((FAILED + 1))
        fi
        continue
    fi
    
    # Run
    binary="./$(echo "$test_name" | cut -f 1 -d '.')"
    if [ ! -f "$binary" ]; then
        # Try generic name if specific one not found
        binary="./virex_out"
    fi
    
    "$binary" > /dev/null 2>&1
    exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        # Check if it was supposed to fail at runtime
        if [[ "$test_name" == "fail.vx" ]] || [[ "$test_name" == "unwrap_test.vx" ]] || [[ "$test_name" == "bounds_fail.vx" ]] || [[ "$test_name" == "slice_fail.vx" ]]; then
             echo -e "${GREEN}PASSED${NC} (Expected runtime failure)"
             PASSED=$((PASSED + 1))
        else
            echo -e "${RED}FAILED${NC} (Runtime error)"
            FAILED=$((FAILED + 1))
        fi
    fi
    
    # Cleanup
    rm -f "$binary" virex_out.c
done

echo -e "\n${BOLD}Test Summary:${NC}"
echo -e "${GREEN}Passed:${NC} $PASSED"
echo -e "${RED}Failed:${NC} $FAILED"
echo -e "Total:  $TOTAL"

if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
