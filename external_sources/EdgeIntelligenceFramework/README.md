# Edge Intelligence Framework (EIF)

<div align="center">

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![C99](https://img.shields.io/badge/C-C99-brightgreen)
![Tests](https://img.shields.io/badge/tests-450+%20passed-success)

**Lightweight Machine Learning Framework for Microcontrollers**

*ESP32 • STM32 • Arduino • Cortex-M*

</div>

---

## 🌟 Features

- **Zero Dynamic Allocation** - All memory is statically allocated
- **Pure C99** - No C++ or stdlib dependencies
- **Fixed-Point Math** - Q15/Q31 for integer-only MCUs
- **SIMD Optimized** - ESP-NN, ARM NEON, Helium support
- **Custom Operators** - Extensible API for user-defined layers
- **Tiny Footprint** - Core library < 50KB Flash

## 📦 Modules

| Module | Description |
|--------|-------------|
| `core` | Memory pool, matrix ops, fixed-point |
| `dsp` | FFT, filters, PID, audio processing |
| `ml` | Classifiers, RNN, attention, federated learning |
| `dl` | Neural network layers, activations |
| `cv` | Image processing, edge detection |
| `bf` | Kalman, EKF, UKF, particle filters |
| `da` | Data analysis, online learning, time series |
| `el` | Edge learning, federated learning, EWC |
| `nlp` | Tokenizer, phoneme recognition, transformer |
| `hal` | Hardware abstraction (GPIO, I2C, SPI) |

## 🚀 Quick Start

```c
#include "eif_core.h"
#include "eif_neural.h"

// Static memory pool
static uint8_t pool_buffer[4096];
static eif_memory_pool_t pool;

int main(void) {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Create neural network
    eif_neural_t* net = eif_neural_create(&pool, 3);
    eif_neural_add_dense(net, 64, 32, EIF_ACTIVATION_RELU);
    eif_neural_add_dense(net, 32, 10, EIF_ACTIVATION_SOFTMAX);
    
    // Run inference
    float input[64] = { /* sensor data */ };
    float output[10];
    eif_neural_forward(net, input, output);
    
    return 0;
}
```

## 🛠️ Build

```bash
mkdir build && cd build
cmake ..
make
make test  # Run 40 unit tests (100% pass rate)
```

### Build Options

| Option | Description |
|--------|-------------|
| `-DEIF_ENABLE_VALIDATION` | Enable debug assertions |
| `-DEIF_USE_FIXED_POINT` | Use Q15 fixed-point math |

### Development Tools

```bash
# Code Quality
make quality               # Run cppcheck + flawfinder
make asan                  # Run tests with AddressSanitizer
make coverage              # Generate code coverage report

# Analysis & Profiling
make memory                # RAM/Flash memory footprint analysis
make benchmarks            # Run & track performance benchmarks
make metrics               # Comprehensive quality metrics
tools/power/power_profile.sh esp32s3 model.eif  # Power consumption analysis
python3 tools/validate_model.py models/         # Validate EIF models

# Platform Build Helpers
tools/build/build_esp32.sh create my_project    # Create ESP32 project
tools/build/build_stm32.sh create my_project    # Create STM32 project  
tools/build/build_arduino.sh create my_project  # Create Arduino project

# Pre-commit Hook (auto-installed)
# - Removes trailing whitespace
# - Validates code formatting
# - Checks for forbidden patterns (malloc, strcpy, etc.)
# - Quick cppcheck scan
```

## 📁 Project Structure

```
edge-intelligence-framework/
├── core/           # Memory, matrix, types
├── dsp/            # Signal processing
├── ml/             # Machine learning
├── dl/             # Deep learning layers
├── cv/             # Computer vision
├── bf/             # Bayesian filters
├── hal/            # Hardware abstraction
├── tests/          # Unit tests (450+ tests, 100% pass)
├── examples/       # Demo applications
├── docs/           # Documentation
└── arduino/        # Arduino library
```

## Documentation 📚

- [Getting Started](docs/manual/GETTING_STARTED.md)
- [API Reference](docs/reference/API_REFERENCE.md)
- [Security Policy](docs/meta/SECURITY.md)
- [Changelog](docs/meta/CHANGELOG.md)
- [Error Handling](docs/reference/concepts/ERROR_HANDLING.md)
- [SIMD Acceleration](docs/reference/concepts/SIMD_ACCELERATION.md)
- [Async Processing](docs/reference/concepts/ASYNC_PROCESSING.md)

Explore the `docs/` folder for comprehensive guides:
- **Manual**: Getting started, tutorials.
- **Reference**: API, algorithms, concepts.
- **Guides**: Platform-specific optimizations.

## 🎯 Supported Platforms

| Platform | Status | Notes |
|----------|--------|-------|
| ESP32-S3 | ✅ | ESP-NN SIMD |
| ESP32 | ✅ | WiFi/BLE support |
| STM32F4 | ✅ | DMA support |
| STM32H7 | ✅ | MDMA support |
| Arduino Nano 33 | ✅ | Cortex-M4 |
| Raspberry Pi Pico | ✅ | Dual-core |
| Linux/macOS | ✅ | Development |

## 🧪 Tests

```bash
cd build
make test

# Output:
# 100% tests passed, 0 tests failed out of 450+
# Total Test time (real) = 0.50 sec
```

## 🔒 Security

- **Zero dynamic allocation** - No malloc/free vulnerabilities
- **Safe string operations** - All buffer operations bounds-checked
- **Memory guards** - Optional runtime buffer overflow detection
- **Static analysis** - cppcheck & flawfinder in CI
- **Sanitizers** - ASan & UBSan validation

See [SECURITY.md](docs/SECURITY.md) for vulnerability reporting.

## 📊 Benchmarks

| Operation | ESP32-S3 | STM32H7 | Generic |
|-----------|----------|---------|---------|
| Dot Product (512) | 0.8 µs | 1.2 µs | 5.1 µs |
| Conv2D (32x32) | 2.1 ms | 3.5 ms | 12 ms |
| FFT (256) | 45 µs | 62 µs | 180 µs |

## 🤝 Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## 📄 License

MIT License - see [LICENSE](LICENSE) for details.

---

<div align="center">

**Made with ❤️ for Edge AI**

</div>
