# ESP32 Optimization Notes

Guide for deploying Edge Intelligence Framework on ESP32.

## Hardware Considerations

| ESP32 Variant | Flash | RAM | FPU | Best For |
|---------------|-------|-----|-----|----------|
| ESP32 | 4MB | 520KB | Yes (DP) | General AI/DSP |
| ESP32-S3 | 8-16MB | 512KB | Yes (SP) | Best for NN |
| ESP32-C3 | 4MB | 400KB | No | Use fixed-point |
| ESP32-C6 | 4MB | 512KB | No | Use fixed-point |

## Memory Management

### SRAM Optimization
```c
// Use DRAM_ATTR for frequently accessed data
DRAM_ATTR float weights[256];

// Use IRAM_ATTR for timing-critical functions
IRAM_ATTR float eif_fir_process(eif_fir_t *fir, float input);
```

### PSRAM Usage
```c
// For larger models, allocate in PSRAM
float *large_buffer = heap_caps_malloc(32768, MALLOC_CAP_SPIRAM);
```

## Fixed-Point for ESP32-C3/C6

These variants lack FPU - use Q15:

```c
#include "eif_dsp_fir_fixed.h"
#include "eif_dsp_biquad_fixed.h"

// Q15 filter (no FPU needed)
eif_fir_q15_t fir;
eif_fir_q15_init(&fir, coeffs, 16);

int16_t output = eif_fir_q15_process(&fir, input);
```

## DSP Optimization

### Use ESP-DSP Library
```cmake
idf_component_register(
    REQUIRES esp-dsp
)
```

```c
#include "esp_dsp.h"

// Use ESP-DSP FFT (optimized for ESP32)
dsps_fft2r_fc32(data, N);
```

### Biquad on ESP32
```c
// ESP-DSP has optimized biquad
dsps_biquad_f32_ae32(input, output, N, coeffs, state);
```

## Neural Network Tips

### Quantization
Use INT8 quantization for 3-4x speedup:
```c
// Convert float model to int8
eif_nn_quantize_model(model, EIF_QUANT_INT8);
```

### Layer Fusion
```c
// Fuse Conv + BN + ReLU for efficiency
eif_nn_fuse_layers(model);
```

## Power Optimization

### Light Sleep Between Inferences
```c
esp_sleep_enable_timer_wakeup(100000);  // 100ms
esp_light_sleep_start();

// Run inference
eif_nn_forward(model, input, output);
```

### Dynamic Frequency Scaling
```c
// Lower frequency for simple DSP
esp_pm_config_t pm_config = {
    .max_freq_mhz = 80,   // Save power
    .min_freq_mhz = 10,
    .light_sleep_enable = true
};
esp_pm_configure(&pm_config);
```

## Recommended Configurations

### Voice Command (ESP32-S3)
```c
// MFCC + GRU keyword spotting
#define SAMPLE_RATE 16000
#define MFCC_FEATURES 13
#define HIDDEN_SIZE 32
#define NUM_CLASSES 10
```

### Sensor Fusion (ESP32)
```c
// IMU processing at 100Hz
#define IMU_RATE 100
#define BIQUAD_STAGES 2
#define FUSION_ALPHA 0.98f
```

### Anomaly Detection (ESP32-C3)
```c
// Fixed-point for no-FPU
#include "eif_dsp_fir_fixed.h"
#define USE_Q15 1
#define WINDOW_SIZE 64
```

## Building for ESP32

```bash
# Set up ESP-IDF
source ~/esp/esp-idf/export.sh

# Build project
cd examples/hw_demos/esp32_voice_cmd
idf.py build

# Flash
idf.py flash monitor
```
