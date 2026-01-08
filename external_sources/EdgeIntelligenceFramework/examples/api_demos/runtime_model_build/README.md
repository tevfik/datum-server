# Runtime Model Build Demo

Demonstrates building CNN models at runtime using structured API.

## Quick Start

```bash
cd build && make runtime_model_demo
./bin/runtime_model_demo
```

## Features

- **Declarative model definition** using macros
- **Automatic shape inference** through the network
- **Runtime model creation** from layer array
- **Performance benchmarking** with timing

## API Usage

```c
#include "eif_model.h"

// Define layers using macros
eif_layer_t layers[] = {
    EIF_INPUT(28, 28, 1),
    EIF_CONV2D(32, 3, 1, conv_weights, conv_bias),
    EIF_RELU(),
    EIF_MAXPOOL(2),
    EIF_FLATTEN(),
    EIF_DENSE(10, dense_weights, dense_bias),
    EIF_SOFTMAX()
};

// Create model
eif_model_t model;
eif_model_create(&model, layers, 7);

// Run inference
int16_t input[784], output[10];
eif_model_infer(&model, input, output);

// Cleanup
eif_model_destroy(&model);
```

## Available Layer Macros

| Macro | Description |
|-------|-------------|
| `EIF_INPUT(h, w, c)` | Input layer |
| `EIF_CONV2D(filters, kernel, stride, w, b)` | 2D convolution |
| `EIF_DENSE(units, w, b)` | Fully connected |
| `EIF_MAXPOOL(size)` | Max pooling |
| `EIF_AVGPOOL(size)` | Average pooling |
| `EIF_GLOBAL_AVGPOOL()` | Global average pooling |
| `EIF_FLATTEN()` | Flatten to 1D |
| `EIF_RELU()` | ReLU activation |
| `EIF_SOFTMAX()` | Softmax output |
