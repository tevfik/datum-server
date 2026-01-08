# Development Tools

This directory contains advanced development and analysis tools for the Edge Intelligence Framework.

## 📁 Directory Structure

```
tools/
├── analysis/          # Memory and performance analysis
├── benchmarks/        # Performance tracking system
├── build/             # Platform build helpers (ESP32, STM32, Arduino)
├── power/             # Power profiling and battery life estimation
├── quality/           # Code quality metrics
└── validate_model.py  # EIF model validator
```

## 🔧 Tools Overview

### 1. Memory Footprint Analyzer

Analyzes RAM and Flash memory usage across all modules.

**Location:** `tools/analysis/memory_footprint.sh`

**Usage:**
```bash
# Analyze current build
./tools/analysis/memory_footprint.sh build text

# JSON output for CI integration
./tools/analysis/memory_footprint.sh build json

# CSV export
./tools/analysis/memory_footprint.sh build csv

# Or use Makefile shortcut
make memory
```

**Features:**
- Per-module Flash (text + data) and RAM (data + bss) breakdown
- Platform compatibility analysis (ESP32, STM32, Arduino, etc.)
- Top 10 largest symbols identification
- Optimization recommendations
- Color-coded output (green/yellow/red based on usage)

**Output Example:**
```
Module                          Text       Data        BSS  Flash Total    RAM Total
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
libeif_core                      451          4        264          455          268
libeif_dl                       1,4K          0          0         1,4K            0
libeif_dsp                       988          0          0          988            0
TOTAL                                                               31K          268

📱 Target Platform Analysis:
✅ ESP32-S3             Flash:   0%  RAM:   0%
✅ STM32F401            Flash:   6%  RAM:   0%
```

---

### 2. Benchmark Tracking System

Tracks performance metrics across commits and detects regressions.

**Location:** `tools/benchmarks/track_benchmarks.sh`

**Usage:**
```bash
# Run benchmarks and record results
./tools/benchmarks/track_benchmarks.sh run

# Compare two runs
./tools/benchmarks/track_benchmarks.sh compare baseline.json latest.json

# Set current run as baseline
./tools/benchmarks/track_benchmarks.sh baseline

# Show benchmark history
./tools/benchmarks/track_benchmarks.sh report

# Or use Makefile shortcut
make benchmarks
```

**Features:**
- Automatic git metadata capture (commit hash, branch, message)
- System information recording (CPU, cores, OS)
- JSON-based results storage
- Regression detection (>5% slowdown = ❌, >5% speedup = ✅)
- Historical performance tracking
- Baseline comparison

**Output Example:**
```
📊 Performance Comparison
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Baseline: a1b2c3d
Current:  e4f5g6h

Benchmark                      Baseline        Current     Change
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
benchmark_dsp                  12500 ops/s     13200 ops/s    +5.6% ✅
benchmark_neural               8400 ops/s      7900 ops/s     -5.9% ❌

⚠️  1 performance regression(s) detected
```

**Results Storage:**
- `tools/benchmarks/results/` - Historical benchmark data
- `tools/benchmarks/baseline.json` - Current baseline reference

---

### 3. Quality Metrics Dashboard

Comprehensive code quality report generator.

**Location:** `tools/quality/metrics.sh`

**Usage:**
```bash
# Generate full quality report
./tools/quality/metrics.sh

# Or use Makefile shortcut
make metrics
```

**Features:**
- Code statistics (LOC, files, modules)
- Test coverage summary
- Static analysis results (cppcheck, flawfinder)
- Build configuration
- Memory safety status
- Documentation count

**Output Example:**
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 Edge Intelligence Framework - Quality Metrics
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📁 Codebase Statistics:
  Source Code:       17,200 lines
  Headers:           19,813 lines
  Tests:              7,361 lines
  Examples:              83 programs

🧪 Test Results:
  Total Tests:           40
  Passed:                40
  Failed:                 0
  Success Rate:      100.0%

✅ Memory Safety:
  Dynamic Allocation:    Zero (all static)
  String Operations:     Safe (bounds-checked)
  Pool-based Memory:     100%
```

---

### 4. Pre-commit Hook

Automatic code quality checks before each commit.

**Location:** `.git/hooks/pre-commit`

**Installation:** 
Automatically active after cloning (executable hook provided)

**Checks Performed:**
1. ✅ **Trailing whitespace** - Auto-fixed
2. ✅ **File ends with newline** - Auto-fixed
3. ❌ **Merge conflict markers** - Blocks commit
4. ⚠️  **Large files (>100KB)** - Warning only
5. ❌ **Forbidden patterns** - Blocks commit:
   - `malloc()`, `free()`, `calloc()`, `realloc()`
   - `strcpy()`, `strcat()`, `gets()`, `sprintf()`
6. ⚠️  **cppcheck quick scan** - Warning only
7. ⚠️  **TODO/FIXME/HACK** without explanation - Warning only

**Example Output:**
```
🔍 Running pre-commit checks...
  → Checking trailing whitespace...
  ⚠ Trailing whitespace in: test.c
    ✓ Auto-fixed
  → Checking files end with newline...
  → Checking for merge conflict markers...
  → Checking for large files...
  → Checking for forbidden patterns...
  → Running cppcheck quick scan...
  → Checking TODO/FIXME/HACK comments...

✅ All pre-commit checks passed!
```

**Bypassing (not recommended):**
```bash
git commit --no-verify
```

---

## 🎯 Recommended Workflow

### Daily Development
```bash
# 1. Write code with confidence (pre-commit hook protects you)
git add your_file.c
git commit -m "feat: add new feature"  # Hook runs automatically

# 2. Run tests
make test

# 3. Check quality periodically
make metrics
```

### Before Release
```bash
# 1. Full quality check
make quality        # Static analysis
make asan           # Memory safety
make coverage       # Test coverage

# 2. Performance validation
make benchmarks     # Track performance

# 3. Memory analysis
make memory         # Check Flash/RAM usage

# 4. Final metrics
make metrics        # Comprehensive report
```

### CI Integration

All tools support JSON output for CI/CD:

```yaml
# .github/workflows/analysis.yml
- name: Memory Analysis
  run: tools/analysis/memory_footprint.sh build json > memory.json

- name: Benchmark Tracking
  run: |
    tools/benchmarks/track_benchmarks.sh run
    tools/benchmarks/track_benchmarks.sh compare
```

---

## 📝 Configuration

### Memory Footprint Analyzer
Edit script variables:
- `MCU_FLASH` - Add new platform Flash sizes
- `MCU_RAM` - Add new platform RAM sizes

### Benchmark Tracker
Edit `tools/benchmarks/track_benchmarks.sh`:
- Adjust parsing logic for your benchmark output format
- Modify regression thresholds (default: ±5%)

### Pre-commit Hook
Edit `.git/hooks/pre-commit`:
- Add/remove forbidden patterns
- Adjust file size warning threshold
- Enable/disable specific checks

---

## 🐛 Troubleshooting

### Memory Footprint Shows Zero
**Problem:** `size` command not found  
**Solution:** Install `binutils`: `sudo apt install binutils`

### Benchmark Tracker: No Benchmarks Found
**Problem:** Benchmark executables not in `build/bin/`  
**Solution:** Ensure benchmarks are built: `make`

### Pre-commit Hook Not Running
**Problem:** Hook file not executable  
**Solution:** `chmod +x .git/hooks/pre-commit`

### cppcheck Not Found
**Problem:** Static analyzer not installed  
**Solution:** `sudo apt install cppcheck`

### Arduino CLI: Permission Denied on Upload
**Problem:** No permission for serial port  
**Solution:** `sudo usermod -a -G dialout $USER` (logout/login required)

### ESP-IDF: Command Not Found
**Problem:** ESP-IDF environment not sourced  
**Solution:** `. $ESP_IDF_PATH/export.sh` or add to ~/.bashrc

---

## 📚 Additional Resources

- [Main README](../README.md) - Project overview
- [CONTRIBUTING.md](../CONTRIBUTING.md) - Contribution guidelines
- [SECURITY.md](../docs/SECURITY.md) - Security policy
- [CI/CD Configuration](../.github/workflows/) - GitHub Actions
- [Platform Documentation](../docs/) - ESP32, STM32, Arduino guides

---

**Last Updated:** 2025-12-24  
**Version:** 1.0.0

---

### 5. Model Validator

Validates .eif model files for correctness, compatibility, and memory requirements.

**Location:** `tools/validate_model.py`

**Usage:**
```bash
# Validate single model
python3 tools/validate_model.py models/gesture_nn.eif

# Validate all models in directory
python3 tools/validate_model.py models/ -v

# JSON output for CI
python3 tools/validate_model.py model.eif --json > validation.json
```

**Features:**
- EIF binary format validation
- Platform compatibility check (ESP32, STM32, Arduino, RP2040)
- Memory footprint calculation
- Layer sequence validation
- Common issue detection

**Output Example:**
```
======================================================================
Model Validation Report: gesture_nn.eif
======================================================================

✅ No errors found

📱 Target Platform Analysis:
  ✅ ESP32-S3             Flash:   0%  RAM:   0%
  ✅ STM32F401            Flash:   8%  RAM:   0%
  ✅ Arduino Nano 33      Flash:  16%  RAM:   0%

✅ Model is valid and ready for deployment!
```

---

### 6. Power Profiler

Analyzes power consumption and estimates battery life for embedded platforms.

**Location:** `tools/power/power_profile.sh`

**Usage:**
```bash
# Profile ESP32-S3 with model
./tools/power/power_profile.sh esp32s3 models/gesture_nn.eif

# Profile STM32F4 (no model)
./tools/power/power_profile.sh stm32f4

# Supported platforms: esp32, esp32s3, stm32f4, stm32h7, rp2040, arduino
```

**Features:**
- Platform-specific power characteristics (active/sleep current)
- Multiple duty cycle scenarios (continuous, 10Hz, 1Hz, event-driven)
- Battery life estimates for common batteries (18650, Li-Po, AA, coin cell)
- Optimization recommendations
- Platform-specific power saving tips

**Output Example:**
```
⚡ Power Profiling Report - ESP32S3

📋 Platform Specifications:
  CPU Frequency:     240 MHz
  Active Current:    180 mA
  Sleep Current:     0.01 mA

⚡ Power Consumption Analysis:
Scenario                    Duty Cycle  Avg Current       Avg Power   Battery Life*
────────────────────────────────────────────────────────────────────────────────
Continuous                        100%      180.0 mA        594.0 mW        11 hrs
10 Hz sampling                     2%        3.6 mA         11.9 mW       23 days
1 Hz sampling                    0.2%        0.4 mA          1.3 mW      208 days

🔋 Battery Life Estimates (1 Hz sampling):
Battery Type                   Capacity        Est. Life
────────────────────────────────────────────────────────
18650 Li-ion                   2000 mAh       208 days
Small Li-Po                    1000 mAh       104 days
```

---

### 7. Platform Build Helpers

Simplifies project creation and building for ESP32, STM32, and Arduino platforms.

**Locations:**
- `tools/build/build_esp32.sh` - ESP32/ESP-IDF helper
- `tools/build/build_stm32.sh` - STM32/ARM GCC helper  
- `tools/build/build_arduino.sh` - Arduino CLI helper

#### ESP32 Build Helper

```bash
# Setup ESP-IDF
./tools/build/build_esp32.sh setup

# Create new project
./tools/build/build_esp32.sh create my_project esp32s3

# Build & flash
cd my_project
export EIF_PATH=../..
. $ESP_IDF_PATH/export.sh
../tools/build/build_esp32.sh build
../tools/build/build_esp32.sh flash /dev/ttyUSB0
```

**Features:**
- Automated ESP-IDF installation
- Project template with EIF integration
- CMake configuration for EIF libraries
- FreeRTOS task setup
- Serial monitor integration

#### STM32 Build Helper

```bash
# Setup ARM toolchain
./tools/build/build_stm32.sh setup

# Create new project
./tools/build/build_stm32.sh create my_project STM32F407VG

# Build & flash
cd my_project
../tools/build/build_stm32.sh build
../tools/build/build_stm32.sh flash
```

**Features:**
- ARM GCC toolchain installation
- Makefile-based project template
- ST-Link/OpenOCD flash support
- Minimal HAL integration
- Cortex-M4/M7 optimization flags

#### Arduino Build Helper

```bash
# Setup Arduino CLI
./tools/build/build_arduino.sh setup

# Create new project
./tools/build/build_arduino.sh create my_project arduino:mbed_nano:nano33ble

# Build & upload
cd my_project
ln -s $PWD/../.. ~/Arduino/libraries/EIF
../tools/build/build_arduino.sh build arduino:mbed_nano:nano33ble
../tools/build/build_arduino.sh upload arduino:mbed_nano:nano33ble /dev/ttyACM0
```

**Features:**
- Arduino CLI installation
- Auto core installation (SAMD, mbed_nano)
- Project template with C++ wrapper
- Serial monitor integration
- Board auto-detection

**Common Board FQBNs:**
- Arduino Nano 33 BLE: `arduino:mbed_nano:nano33ble`
- Arduino Nano 33 IoT: `arduino:samd:nano_33_iot`
- ESP32 Dev Module: `esp32:esp32:esp32`
- ESP32-S3: `esp32:esp32:esp32s3`

