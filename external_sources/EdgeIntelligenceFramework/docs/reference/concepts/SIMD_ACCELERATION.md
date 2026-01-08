# EIF SIMD & Hardware Acceleration Guide

This guide covers EIF's platform-specific SIMD optimizations for maximum performance.

## Supported Platforms

| Platform | Instruction Set | Header Flag |
|----------|-----------------|-------------|
| ESP32-S3 | ESP-NN | `EIF_SIMD_ESP_NN` |
| Cortex-M55 | Helium (MVE) | `EIF_SIMD_HELIUM` |
| Cortex-A | NEON | `EIF_SIMD_NEON` |
| Generic | C fallback | `EIF_SIMD_GENERIC` |

## Quick Start

```c
#include "eif_hal_simd.h"

// Check platform
printf("SIMD Platform: %s\n", eif_simd_get_platform());
printf("Accelerated: %s\n", eif_simd_is_accelerated() ? "Yes" : "No");

// Vectorized dot product (auto-dispatches)
float a[256], b[256];
float result = eif_simd_dot_f32(a, b, 256);
```

## Available Functions

### Dot Products

```c
// Float32 dot product
float32_t eif_simd_dot_f32(const float32_t* a, const float32_t* b, int n);

// INT8 (Q7) dot product - returns int32
int32_t eif_simd_dot_q7(const q7_t* a, const q7_t* b, int n);
```

### Vector Operations

```c
// ReLU activation (in-place)
void eif_simd_relu_f32(float32_t* data, int n);

// Vector add: c = a + b
void eif_simd_add_f32(const float32_t* a, const float32_t* b, float32_t* c, int n);

// Vector scale: b = a * scale
void eif_simd_scale_f32(const float32_t* a, float32_t scale, float32_t* b, int n);
```

### Matrix Operations

```c
// Matrix-vector multiply: y = A * x
void eif_simd_matvec_f32(const float32_t* A, const float32_t* x, 
                         float32_t* y, int m, int n);

// Single conv2d output pixel
float32_t eif_simd_conv2d_pixel_f32(const float32_t* patch, 
                                     const float32_t* filter,
                                     float32_t bias, int patch_size);
```

## Performance Tips

1. **Align your data** - Use 16-byte alignment for best SIMD performance
2. **Use power-of-2 sizes** - Vectors divisible by 4 (NEON) or 8 (AVX2)
3. **Prefer Q8 over FP32** - 2-4x faster with minimal accuracy loss

## ESP32-S3 Example

```c
// platformio.ini
// board = esp32-s3-devkitc-1
// build_flags = -DCONFIG_IDF_TARGET_ESP32S3

#include "eif_hal_simd.h"

void app_main(void) {
    // Will use ESP-NN optimized kernels
    float input[512], weights[512];
    float dot = eif_simd_dot_f32(input, weights, 512);
}
```

## ARM Cortex-M55 Example

```c
// Compile with: -mcpu=cortex-m55 -mfloat-abi=hard

#include "eif_hal_simd.h"

// Uses Helium (MVE) automatically
float result = eif_simd_dot_f32(a, b, 256);
```

## Benchmarking

```c
#include "eif_power.h"
#include "eif_hal_simd.h"

eif_timer_t timer;
eif_timer_start(&timer);

for (int i = 0; i < 1000; i++) {
    eif_simd_dot_f32(a, b, 512);
}

uint32_t us = eif_timer_stop(&timer);
printf("1000 iterations: %u us\n", us);
printf("Per iteration: %.2f us\n", us / 1000.0f);
```

## Adding Custom Platform Support

```c
// In eif_hal_simd.h, add:
#elif defined(MY_CUSTOM_DSP)
    #define EIF_SIMD_CUSTOM 1
    #include "my_dsp.h"

// Then implement in eif_simd_dot_f32:
#elif defined(EIF_SIMD_CUSTOM)
    return my_dsp_dot(a, b, n);
```
