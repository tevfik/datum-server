# Getting Started with EIF

Quick start guide for Edge Intelligence Framework.

## Installation

```bash
git clone https://github.com/edge-intelligence/eif.git
cd eif

# Build
mkdir build && cd build
cmake ..
make -j4

# Run tests
./bin/run_tests

# Run a demo
./bin/benchmark_suite --batch
```

## Quick Examples

### Sensor Smoothing
```c
#include "eif_dsp_smooth.h"

eif_ema_t smoother;
eif_ema_init(&smoother, 0.2f);

float raw = read_sensor();
float smooth = eif_ema_update(&smoother, raw);
```

### Anomaly Detection
```c
#include "eif_adaptive_threshold.h"

eif_z_threshold_t detector;
eif_z_threshold_init(&detector, 0.1f, 3.0f);

if (eif_z_threshold_check(&detector, value)) {
    trigger_alert();
}
```

### Activity Recognition
```c
#include "eif_activity.h"

eif_activity_window_t window;
eif_activity_window_init(&window, 64);

// In loop
if (eif_activity_window_add(&window, ax, ay, az)) {
    eif_accel_sample_t samples[128];
    eif_activity_features_t features;
    
    eif_activity_window_get_samples(&window, samples);
    eif_activity_extract_features(samples, 128, &features);
    
    eif_activity_t activity = eif_activity_classify_rules(&features);
}
```

### Digital Filter
```c
#include "eif_dsp_biquad.h"

eif_biquad_t lowpass;
eif_biquad_lowpass(&lowpass, 8000.0f, 1000.0f, 0.707f);

// In loop
float output = eif_biquad_process(&lowpass, input);
```

## Project Structure

```
eif/
├── core/      # Core utilities
├── dsp/       # Digital Signal Processing
├── ml/        # Machine Learning
├── dl/        # Deep Learning
├── examples/  # Demo applications
├── tests/     # Unit tests
└── docs/      # Documentation
```

## Platform Guides

| Platform | Guide |
|----------|-------|
| Arduino | [ARDUINO_GUIDE.md](ARDUINO_GUIDE.md) |
| Raspberry Pi Pico | [PICO_GUIDE.md](PICO_GUIDE.md) |
| STM32 | [STM32_GUIDE.md](STM32_GUIDE.md) |
| ESP32 | [ESP32_OPTIMIZATION.md](ESP32_OPTIMIZATION.md) |

## Next Steps

1. Run demos: `./build/bin/*_demo --batch`
2. Read platform guide for your board
3. Check [BENCHMARKS.md](BENCHMARKS.md) for performance
4. Browse [ALGORITHMS.md](ALGORITHMS.md) for available algorithms
