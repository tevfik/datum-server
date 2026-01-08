#!/bin/bash
set -e

# Configuration
BUILD_DIR="build-coverage"
OUTPUT_DIR="coverage_report"
INFO_FILE="coverage.info"

# Ensure we are in the project root
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' not found."
    echo "Please run this script from the project root."
    exit 1
fi

echo "=== Generating Coverage Report ==="

# 1. Reset counters (optional, but good for clean run)
# lcov --directory $BUILD_DIR --zerocounters

# 2. Capture coverage data
echo "Capturing coverage data..."
lcov --capture --directory $BUILD_DIR --output-file $BUILD_DIR/$INFO_FILE --ignore-errors mismatch,gcov

# 3. Filter out unnecessary files (system headers, tests, etc. if desired)
# For now, we keep everything or maybe exclude /usr/*
echo "Filtering system files..."
lcov --remove $BUILD_DIR/$INFO_FILE '/usr/*' --output-file $BUILD_DIR/$INFO_FILE

# 4. Generate HTML report
echo "Generating HTML report..."
genhtml $BUILD_DIR/$INFO_FILE --output-directory $BUILD_DIR/$OUTPUT_DIR --ignore-errors mismatch

echo "=== Success! ==="
echo "Report generated at: $BUILD_DIR/$OUTPUT_DIR/index.html"
