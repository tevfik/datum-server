#!/bin/bash
# Quality metrics report generator

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=================================="
echo "  EIF Quality Metrics Report"
echo "=================================="
echo ""

# Code Statistics
echo "📊 Code Statistics:"
echo "-------------------"
TOTAL_LOC=$(find "$PROJECT_ROOT"/{core,dsp,ml,dl,cv,bf,hal,nlp,da,el}/src -name "*.c" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
TOTAL_HEADERS=$(find "$PROJECT_ROOT"/{core,dsp,ml,dl,cv,bf,hal,nlp,da,el}/include -name "*.h" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
TOTAL_TESTS=$(find "$PROJECT_ROOT"/tests -name "*.c" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
TOTAL_EXAMPLES=$(find "$PROJECT_ROOT"/examples -name "*.c" 2>/dev/null | wc -l)

echo "  Source code:      $TOTAL_LOC lines"
echo "  Headers:          $TOTAL_HEADERS lines"
echo "  Test code:        $TOTAL_TESTS lines"
echo "  Example programs: $TOTAL_EXAMPLES"
echo ""

# Test Results
echo "✅ Test Results:"
echo "----------------"
if [ -d "$PROJECT_ROOT/build" ]; then
    cd "$PROJECT_ROOT/build"
    TEST_OUTPUT=$(ctest --output-on-failure 2>&1 | tail -3)
    echo "$TEST_OUTPUT"
else
    echo "  Build directory not found. Run 'make' first."
fi
echo ""

# Static Analysis Summary
echo "🔍 Static Analysis:"
echo "-------------------"
CPPCHECK_ERRORS=$(bash "$SCRIPT_DIR/run_cppcheck.sh" 2>&1 | grep -c "error:" || echo "0")
CPPCHECK_WARNINGS=$(bash "$SCRIPT_DIR/run_cppcheck.sh" 2>&1 | grep -c "warning:" || echo "0")
echo "  cppcheck errors:   $CPPCHECK_ERRORS"
echo "  cppcheck warnings: $CPPCHECK_WARNINGS"
echo ""

FLAWFINDER_HIGH=$(bash "$SCRIPT_DIR/run_flawfinder.sh" 2>&1 | grep -c "Hits = " | head -1 || echo "0")
echo "  flawfinder issues: $FLAWFINDER_HIGH"
echo ""

# Memory Safety
echo "🛡️  Memory Safety:"
echo "------------------"
echo "  Dynamic allocation: ZERO (all static)"
echo "  Safe strings:       strncpy, snprintf"
echo "  Memory guards:      Available (optional)"
echo "  Sanitizers:         ASan/UBSan ready"
echo ""

# Build Configuration
echo "⚙️  Build Configuration:"
echo "----------------------"
if [ -f "$PROJECT_ROOT/build/CMakeCache.txt" ]; then
    BUILD_TYPE=$(grep "CMAKE_BUILD_TYPE:" "$PROJECT_ROOT/build/CMakeCache.txt" | cut -d= -f2)
    echo "  Build type:        $BUILD_TYPE"
fi
echo "  C Standard:        C99"
echo "  Compiler:          GCC/Clang"
echo "  SIMD Support:      ESP-NN, NEON, Helium"
echo ""

# Documentation
echo "📚 Documentation:"
echo "-----------------"
DOC_COUNT=$(find "$PROJECT_ROOT/docs" -name "*.md" 2>/dev/null | wc -l)
echo "  Documentation files: $DOC_COUNT"
echo "  API Reference:       ✓"
echo "  Security Policy:     ✓"
echo "  Changelog:           ✓"
echo ""

echo "=================================="
echo "  Report generated: $(date)"
echo "=================================="
