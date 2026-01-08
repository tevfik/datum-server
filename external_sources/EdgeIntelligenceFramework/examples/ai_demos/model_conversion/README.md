# Model Conversion Demo - TFLite to EIF Workflow

## Overview
This tutorial demonstrates how to convert and deploy TensorFlow Lite models to embedded devices using the EIF framework's model converter.

## Scenario
A developer has trained a deep learning model in TensorFlow/Keras and needs to deploy it to a resource-constrained microcontroller.

## Conversion Pipeline

```
┌───────────────┐    ┌───────────────┐    ┌───────────────┐    ┌───────────────┐
│  TensorFlow   │───►│    TFLite     │───►│  tflite_to_   │───►│   EIF Model   │
│    Model      │    │   .tflite     │    │   eif.py      │    │     .eif      │
└───────────────┘    └───────────────┘    └───────────────┘    └───────────────┘
      Train              Export              Convert             Deploy
```

## Tools Used

### tflite_to_eif.py
Main converter script that transforms TFLite models to EIF binary format.

**Usage:**
```bash
python tools/tflite_to_eif.py model.tflite model.eif
```

**Supported Layers:**

| TFLite Op | EIF Layer | Notes |
|-----------|-----------|-------|
| FULLY_CONNECTED | Dense | With optional bias |
| CONV_2D | Conv2D | All padding modes |
| DEPTHWISE_CONV_2D | DepthwiseConv2D | Optimized |
| MAX_POOL_2D | MaxPool2D | |
| AVERAGE_POOL_2D | AvgPool2D | |
| SOFTMAX | Softmax | |
| RELU | ReLU | |
| LOGISTIC | Sigmoid | |
| TANH | Tanh | |
| RESHAPE | Reshape | |
| ADD | Add | Element-wise |
| CONCATENATION | Concat | |

### model-compiler (Advanced)
More advanced compilation with:
- Operator fusion
- Quantization support
- Memory optimization
- Multiple backends

## EIF Binary Format

```
┌─────────────────────────────────────┐
│ Header (28 bytes)                   │
│  - Magic: 0x4549464D ("EIFM")       │
│  - Version                          │
│  - Tensor count                     │
│  - Node count                       │
│  - Input/Output indices             │
│  - Weight blob size                 │
├─────────────────────────────────────┤
│ Tensor Descriptors                  │
│  - Type, Shape, Size                │
│  - Data offset in weight blob       │
├─────────────────────────────────────┤
│ Node (Layer) Descriptors            │
│  - Layer type                       │
│  - Input/Output tensor indices      │
│  - Layer-specific parameters        │
├─────────────────────────────────────┤
│ Graph I/O Indices                   │
├─────────────────────────────────────┤
│ Weight Blob (aligned)               │
└─────────────────────────────────────┘
```

## C API Usage

```c
#include "eif_model.h"
#include "eif_neural.h"

// Load model
eif_model_t model;
eif_model_load(&model, "model.eif", &pool);

// Initialize context
eif_neural_context_t ctx;
eif_neural_init(&ctx, &model, arena, &pool);

// Get input tensor
float* input = eif_neural_input(&ctx, 0);
// ... fill input data ...

// Run inference
eif_neural_invoke(&ctx);

// Get output
float* output = eif_neural_output(&ctx, 0);
```

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_model_load()` | Load .eif model from file/memory |
| `eif_model_info()` | Get model metadata |
| `eif_neural_init()` | Initialize inference context |
| `eif_neural_invoke()` | Run forward pass |
| `eif_neural_input()` | Get input tensor pointer |
| `eif_neural_output()` | Get output tensor pointer |
| `eif_neural_reset()` | Reset stateful layers (RNN) |

## Best Practices

1. **Model Optimization**
   - Use TFLite's built-in optimizations first
   - Quantize to INT8 for 4x smaller models
   - Prune unused operations

2. **Memory Management**
   - Pre-allocate arena based on model analysis
   - Use streaming for large inputs
   - Share buffers between layers when possible

3. **Performance**
   - Profile with `eif_neural_profile()`
   - Use SIMD-enabled builds
   - Consider layer fusion

## Run the Demo
```bash
cd build && ./bin/model_conversion_demo
```
