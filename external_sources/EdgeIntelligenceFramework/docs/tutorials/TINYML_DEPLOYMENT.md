# TinyML Deployment Guide

Complete guide for deploying Edge Intelligence Framework models to edge devices.

## Table of Contents
1. [Target Platforms](#target-platforms)
2. [Model Optimization](#model-optimization)
3. [Quantization](#quantization)
4. [Memory Planning](#memory-planning)
5. [Deployment Workflow](#deployment-workflow)
6. [Best Practices](#best-practices)

---

## Target Platforms

### MCU Categories

| Category | RAM | Flash | FPU | Best For |
|----------|-----|-------|-----|----------|
| **High-End** | 512KB+ | 4MB+ | Yes | Full CNN, RNN |
| **Mid-Range** | 128-512KB | 512KB-4MB | Maybe | Small NN, DSP |
| **Low-End** | <64KB | <512KB | No | Fixed-point, Rules |

### Recommended Platforms

| Platform | RAM | Flash | FPU | Power |
|----------|-----|-------|-----|-------|
| ESP32-S3 | 512KB | 8-16MB | SP | 40mA |
| ESP32 | 520KB | 4MB | DP | 80mA |
| ESP32-C3 | 400KB | 4MB | No | 20mA |
| STM32F4 | 192KB | 1MB | SP | 30mA |
| STM32L4 | 640KB | 2MB | SP | 10mA |
| nRF52840 | 256KB | 1MB | SP | 5mA |
| RP2040 | 264KB | 2MB | No | 25mA |

---

## Model Optimization

### Layer-Level Optimizations

```c
// 1. Use depthwise separable convolutions
// Instead of Conv2D(64, 3x3):
//   - DepthwiseConv2D(3x3) + Conv2D(64, 1x1)
//   - Reduces params by ~9x

// 2. Reduce hidden dimensions
// Instead of LSTM(128):
//   - GRU(64) - 25% fewer params than LSTM
//   - Or LSTM(32) with attention

// 3. Use smaller kernel sizes
// 3x3 is usually sufficient, 1x1 for channel mixing
```

### Architecture Guidelines

| Model Type | Recommended Size | RAM | Flash |
|------------|------------------|-----|-------|
| Keyword Spotting | 20-50K params | 30KB | 100KB |
| Wake Word | 50-100K params | 50KB | 200KB |
| Gesture Recognition | 10-30K params | 20KB | 50KB |
| Anomaly Detection | 5-20K params | 10KB | 30KB |

---

## Quantization

### Quantization Types

| Type | Bits | Accuracy Loss | Speedup | Use Case |
|------|------|---------------|---------|----------|
| FP32 | 32 | Baseline | 1x | Development |
| FP16 | 16 | ~0.1% | 1.5x | GPU inference |
| INT8 | 8 | 0.5-2% | 2-4x | General edge |
| INT4 | 4 | 2-10% | 4-8x | Extreme edge |

### EIF Quantization Workflow

```c
#include "eif_quantize.h"

// 1. Collect calibration stats
eif_quant_stats_t stats;
eif_quant_stats_init(&stats);

for (int i = 0; i < num_calibration_samples; i++) {
    eif_quant_stats_update(&stats, activations[i], size);
}

// 2. Calculate quantization parameters
eif_quant_params_t params;
eif_quant_calc_int8_symmetric(&params, stats.min_val, stats.max_val);

// 3. Quantize weights
int8_t quantized_weights[WEIGHT_SIZE];
eif_quant_to_int8_sym(float_weights, quantized_weights, WEIGHT_SIZE, &params);

// 4. Measure quality
float sqnr = eif_quant_sqnr(original, dequantized, size);
printf("SQNR: %.1f dB (>40 dB is excellent)\n", sqnr);
```

---

## Memory Planning

### Memory Components

```
Total RAM = Weights + Activations + Scratch + Stack

Weights:       Model parameters (can be in Flash)
Activations:   Layer outputs (double buffer: in + out)
Scratch:       Temporary computation space
Stack:         Function calls, local variables
```

### Using EIF Memory Estimator

```c
#include "eif_edge_inference.h"

eif_model_t model;
// ... add layers ...

eif_memory_estimate_t mem;
eif_estimate_memory(&model, &mem);

printf("Weights: %.1f KB\n", mem.weights_bytes / 1024.0f);
printf("Activations: %.1f KB\n", mem.activations_bytes / 1024.0f);
printf("Total: %.1f KB\n", mem.total_bytes / 1024.0f);

if (eif_model_fits_memory(&model, TARGET_RAM_BYTES)) {
    printf("Model fits target!\n");
}
```

### Memory Reduction Strategies

| Strategy | Reduction | Complexity |
|----------|-----------|------------|
| INT8 quantization | 4x | Low |
| Weight sharing | 2-4x | Medium |
| Pruning | 2-10x | Medium |
| Knowledge distillation | Variable | High |
| Layer fusion | 1.2-1.5x | Low |

---

## Deployment Workflow

### Step 1: Train in Python
```python
import tensorflow as tf

model = tf.keras.Sequential([...])
model.fit(X_train, y_train)
model.save('model.h5')
```

### Step 2: Convert & Quantize
```python
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_data_gen
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
tflite_model = converter.convert()
```

### Step 3: Export to C
```bash
xxd -i model.tflite > model_data.h
# Or use our converter
python tools/tflite_to_eif.py model.tflite --output model.h
```

### Step 4: Deploy with EIF
```c
#include "model.h"
#include "eif_edge_inference.h"

// Use the model
float input[INPUT_SIZE];
float output[OUTPUT_SIZE];
eif_model_forward(&model, input, output);
```

---

## Best Practices

### Power Optimization
```c
// 1. Use sleep between inferences
esp_light_sleep_start();

// 2. Reduce inference frequency
#define INFERENCE_INTERVAL_MS 100  // Don't run every frame

// 3. Early-exit for obvious cases
if (max_activation > CONFIDENT_THRESHOLD) {
    return early_result;
}
```

### Latency Optimization
```c
// 1. Keep hot data in fast RAM
DRAM_ATTR float weights[256];

// 2. Use SIMD where available
#ifdef __ARM_NEON
// Use NEON intrinsics
#endif

// 3. Batch multiple samples
#define BATCH_SIZE 4
```

### Accuracy Monitoring
```c
#include "eif_adaptive_threshold.h"

// Monitor for concept drift
eif_drift_detector_t drift;
eif_drift_init(&drift, 0.01f, 0.1f);

if (eif_drift_update(&drift, prediction_confidence)) {
    printf("Warning: Model may need retraining\n");
}
```

---

## Checklist

- [ ] Model fits in Flash
- [ ] Activations fit in RAM
- [ ] Latency meets requirements
- [ ] Power budget acceptable
- [ ] Quantization quality verified (SQNR > 35 dB)
- [ ] Edge cases tested
- [ ] Drift monitoring enabled
