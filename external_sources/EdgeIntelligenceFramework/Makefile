
# EIF Convenience Makefile

BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

.PHONY: all clean docs test help quality asan coverage memory benchmarks metrics

all:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release
	@cd $(BUILD_DIR) && make -j4

clean:
	@rm -rf $(BUILD_DIR)
	@find . -name "*.gcov" -delete
	@find . -name "*.gcda" -delete
	@find . -name "*.gcno" -delete
	@find . -name "*.o" -delete
	@find . -name "*.a" -delete
	@rm -rf coverage build-coverage build-asan docs/site
	@echo "Cleaned build directory and artifacts."

docs:
	@echo "Generating documentation..."
	@python3 tools/generate_docs.py
api-docs:
	@echo "Generating API documentation with Doxygen..."
	@command -v doxygen >/dev/null 2>&1 || { echo "Error: doxygen not found. Install with: sudo apt install doxygen graphviz"; exit 1; }
	doxygen Doxyfile
	@echo "API documentation generated in docs/api/html/index.html"
test:
	@cd $(BUILD_DIR) && ctest --output-on-failure

help:
	@echo "Edge Intelligence Framework"
	@echo "Targets:"
	@echo "  all       : Build the project"
	@echo "  clean     : Remove build files"
	@echo "  docs      : Generate static documentation site"
	@echo "  test      : Run unit tests"
	@echo "  quality   : Run static analysis (cppcheck, flawfinder)"
	@echo "  asan      : Build and run tests with AddressSanitizer"
	@echo "  coverage  : Generate code coverage report (HTML)"
	@echo "  memory    : Analyze RAM/Flash memory footprint"
	@echo "  benchmarks: Run and track performance benchmarks"
	@echo "  metrics   : Show comprehensive quality metrics"

quality:
	@bash tools/quality/report.sh

asan:
	@mkdir -p build-asan
	@cd build-asan && cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
	@cd build-asan && make -j4 && ctest --output-on-failure

coverage:
	@echo "Building with coverage instrumentation..."
	@mkdir -p build-coverage
	@cd build-coverage && cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="--coverage -fprofile-arcs -ftest-coverage" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
	@cd build-coverage && make -j4
	@echo "Running tests..."
	@cd build-coverage && ctest --output-on-failure
	@echo "Generating coverage report..."
	@lcov --capture --directory build-coverage --output-file build-coverage/coverage.info --ignore-errors mismatch 2>/dev/null || true
	@lcov --remove build-coverage/coverage.info '/usr/*' '*/tests/*' --output-file build-coverage/coverage_filtered.info 2>/dev/null || true
	@genhtml build-coverage/coverage_filtered.info --output-directory build-coverage/coverage_html 2>/dev/null || true
	@echo "Coverage report generated: build-coverage/coverage_html/index.html"
	@xdg-open build-coverage/coverage_html/index.html 2>/dev/null || echo "Open build-coverage/coverage_html/index.html in browser"

memory:
	@bash tools/analysis/memory_footprint.sh $(BUILD_DIR) text

benchmarks:
	@bash tools/benchmarks/track_benchmarks.sh run

metrics:
	@bash tools/quality/metrics.sh
