#!/bin/bash
# EIF Test Runner Script
# Run from project root directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Build if needed
if [ ! -f "build/bin/run_tests" ]; then
    echo "Building tests..."
    mkdir -p build && cd build
    cmake .. && make -j$(nproc)
    cd ..
fi

# Run tests
echo "Running EIF Unit Tests..."
echo "========================="
./build/bin/run_tests "$@"
