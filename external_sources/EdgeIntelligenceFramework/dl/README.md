# Deep Learning Module (`dl/`)

Neural network inference and on-device training for edge devices.

## Features

| Layer Type | Description |
|------------|-------------|
| **Conv2D** | 2D Convolution with padding, stride, groups |
| **Depthwise Conv2D** | Depthwise Convolution (Float32 & Int8) |
| **Conv1D** | 1D Convolution for time series |
| **Transpose Conv2D** | Transposed Convolution (Deconvolution) |
| **Dense** | Fully connected layer |
| **MaxPool/AvgPool** | Pooling operations |
| **GlobalAvgPool** | Global Average Pooling |
| **BatchNorm** | Batch normalization |
| **Instance/Layer Norm** | Layer/Instance normalization |
| **Dropout** | Regularization |
| **Attention** | Multi-head self-attention |
| **Embedding** | Token embeddings |
| **Resize** | Bilinear and Nearest Neighbor interpolation |
| **RNN/LSTM/GRU** | Recurrent layers |
| **ArgMax** | Index of maximum value |
| **TopK** | Top-K values and indices |
| **Gather** | Gather elements from indices |
| **Pad** | Spatial padding |
| **Split** | Split tensor along axis |
| **MatMul** | Matrix Multiplication |
| **Reduce** | Analyze Sum/Mean along axis |
| **Math Ops** | Add, Sub, Mul, Div, Min, Max, Exp, Log, Sqrt |

## Activations
- ReLU, ReLU6, Sigmoid, Tanh, Softmax, GELU

## Training
- Forward pass
- Backward pass (gradients)
- Weight update (SGD)

## Quantization
- INT8 quantization support
- Per-layer or per-channel

## Usage

```c
#include "eif_nn.h"

// Create model
eif_nn_model_t model;
eif_nn_model_init(&model, &pool);

// Add layers
eif_nn_layer_t conv = eif_nn_layer_conv2d(3, 32, 3, 1, 1);
eif_nn_model_add_layer(&model, &conv);

// Inference
eif_nn_model_invoke(&model, input, output);
```

## Files
- `eif_nn_types.h` - Data types
- `eif_nn_layers.h` - Layer definitions
- `eif_nn_model.h` - Model management
- `eif_nn_inference.h` - Inference API
- `eif_nn_train.h` - Training API
