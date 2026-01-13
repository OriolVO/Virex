#!/bin/bash
# tests/cli/test_output_path.sh

mkdir -p tests/tmp
echo "func main() -> i32 { return 0; }" > tests/tmp/simple.vx

echo "Testing nested output path..."
./virex build tests/tmp/simple.vx -o tests/tmp/nested/dir/app

if [ -f tests/tmp/nested/dir/app ]; then
    echo "✓ Executable created at nested path"
    ./tests/tmp/nested/dir/app
    if [ $? -eq 0 ]; then
        echo "✓ Executable runs successfully"
    else
        echo "✗ Executable failed to run"
        exit 1
    fi
else
    echo "✗ Executable NOT found at nested path"
    exit 1
fi

# Cleanup
rm -rf tests/tmp
echo "Test passed!"
