# Deep Learning Fundamentals for Embedded

A practical guide to neural networks on microcontrollers.

> **For embedded developers**: You know C and MCUs. This guide explains
> how neural networks work and how to run them on tiny devices.

---

## Table of Contents

1. [What is a Neural Network?](#what-is-a-neural-network)
2. [The Building Blocks](#the-building-blocks)
3. [Network Architectures](#network-architectures)
4. [Quantization for MCUs](#quantization-for-mcu)
5. [Memory and Computation](#memory-and-computation)
6. [Model Deployment](#model-deployment)
7. [Common Mistakes](#common-mistakes)

---

## What is a Neural Network?

A neural network is a **function** that transforms input to output through learned weights.

```
Input    →    Network    →    Output
[1.2, 0.5]    weights       [0.1, 0.8, 0.1]
2 values      (learned)     3 probabilities
              ↓
              "class B"
```

### Why Neural Networks?

| Traditional ML | Neural Networks |
|----------------|-----------------|
| You design features | Network learns features |
| Limited complexity | Can learn complex patterns |
| Works with small data | Needs more data |
| Fast, small | Slower, larger |

### When to Use Neural Networks on Embedded

✅ **Good for**:
- Keyword spotting (audio → word)
- Gesture recognition (IMU → gesture)
- Image classification (camera → object)
- Anomaly detection (complex patterns)

❌ **Not ideal for**:
- Simple threshold-based decisions
- Very limited memory (<10KB)
- Battery-powered always-on (high power)

---

## The Building Blocks

### 1. Dense (Fully Connected) Layer

Every input connects to every output with a weight.

```
Input:  3 values
Output: 2 values
Weights: 3 × 2 = 6 parameters + 2 biases

y1 = w11*x1 + w12*x2 + w13*x3 + b1
y2 = w21*x1 + w22*x2 + w23*x3 + b2
```

**Memory**: `inputs × outputs × 4 bytes` (float32)

**Use for**: Final classification layer, small feature maps

### 2. Activation Functions

Non-linearity that allows networks to learn complex patterns.

| Activation | Formula | Use Case |
|------------|---------|----------|
| **ReLU** | max(0, x) | Hidden layers (most common) |
| **Sigmoid** | 1/(1+e^-x) | Binary output (0-1) |
| **Softmax** | e^xi / Σe^xj | Multi-class output |
| **Tanh** | tanh(x) | RNN hidden states |

```c
// ReLU is incredibly simple
float relu(float x) {
    return x > 0 ? x : 0;
}

// Integer ReLU (Q15)
int16_t relu_q15(int16_t x) {
    return x > 0 ? x : 0;
}
```

### 3. Convolutional Layer (Conv1D/Conv2D)

Slides a small kernel across input, detecting local patterns.

```
Conv1D:
Input:   [a, b, c, d, e, f, g, h]
Kernel:  [w1, w2, w3] (size 3)

Output[0] = w1*a + w2*b + w3*c
Output[1] = w1*b + w2*c + w3*d
...
```

**Why conv?**:
- Much fewer parameters than dense
- Detects patterns regardless of position
- Natural for time series and images

**Memory**: `kernel_size × in_channels × out_channels × 4 bytes`

### 4. Pooling Layer

Reduces size by taking max or average of windows.

```
Max Pooling (size 2):
Input:  [3, 1, 4, 2, 8, 5, 1, 0]
Output: [3,    4,    8,    1]
           ↑     ↑     ↑     ↑
         max   max   max   max
```

**Why pooling?**:
- Reduces memory and computation
- Makes network robust to small shifts
- Has zero parameters

### 5. Recurrent Layers (RNN, LSTM, GRU)

Process sequences step-by-step, maintaining state.

```
Input sequence: [x1, x2, x3, x4]

        h1 → h2 → h3 → h4 → output
        ↑    ↑    ↑    ↑
       x1   x2   x3   x4

Hidden state carries information through time.
```

**LSTM vs GRU**:
- LSTM: More parameters, better for long sequences
- GRU: Simpler, often enough for embedded

---

## Network Architectures

### Architecture 1: Simple Dense Network

For: Basic classification with pre-extracted features

```
Input (20 features)
    ↓
Dense(32) + ReLU
    ↓
Dense(16) + ReLU
    ↓
Dense(3) + Softmax
    ↓
Output (3 classes)

Parameters: 20×32 + 32×16 + 16×3 = 1,200
Memory: ~5 KB (float32)
```

### Architecture 2: DS-CNN (Keyword Spotting)

For: Audio keyword detection

```
MFCC features (40 × 32)
    ↓
Conv2D(32, 3×3) + BN + ReLU
    ↓
DepthwiseSeparable(64) + BN + ReLU
    ↓
DepthwiseSeparable(64) + BN + ReLU
    ↓
GlobalAveragePooling
    ↓
Dense(12) + Softmax
    ↓
Output (12 words)

Parameters: ~25K
Memory: ~100 KB (float32), ~25 KB (int8)
```

### Architecture 3: 1D-CNN (Time Series)

For: Activity recognition, vibration analysis

```
Accelerometer (128 × 3)
    ↓
Conv1D(16, kernel=5) + ReLU
    ↓
MaxPool1D(2)
    ↓
Conv1D(32, kernel=3) + ReLU
    ↓
MaxPool1D(2)
    ↓
Flatten
    ↓
Dense(32) + ReLU
    ↓
Dense(5) + Softmax
    ↓
Output (5 activities)

Parameters: ~8K
Memory: ~35 KB (float32), ~10 KB (int8)
```

---

## Quantization for MCUs

### The Problem

Float32 model: Each weight is 4 bytes.

```
100K parameters × 4 bytes = 400 KB
Too big for most MCUs!
```

### The Solution: Quantization

Convert float32 to int8 (or int16):

```
Float: -0.234, 0.567, -0.891, 0.123...  (4 bytes each)
Int8:   -30,    73,   -114,    16...    (1 byte each)

With scale=0.00781 and zero_point=0
value_float = (value_int8 - zero_point) × scale
```

### Quantization Types

| Type | Bits | Memory | Accuracy Loss |
|------|------|--------|---------------|
| Float32 | 32 | 100% | 0% |
| Float16 | 16 | 50% | ~0% |
| Int8 | 8 | 25% | 1-3% |
| Int4 | 4 | 12.5% | 5-10% |

### Post-Training Quantization (Easiest)

1. Train model in float32 on PC
2. Convert to int8 after training
3. Deploy int8 model on MCU

```python
# TensorFlow Lite example
converter = tf.lite.TFLiteConverter.from_saved_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
quantized_model = converter.convert()
```

### Quantization-Aware Training (Better)

1. Train model simulating int8 operations
2. Model learns to be robust to quantization
3. Better accuracy than post-training

---

## Memory and Computation

### Memory Breakdown

```
Total Memory = Weights + Activations + Code

Weights:     Model parameters (constant)
Activations: Intermediate values (per inference)
Code:        Inference engine (~10-50 KB)
```

### Activation Memory (Often Forgotten!)

```
Conv2D(64 filters, output 20×20)
Activation memory = 64 × 20 × 20 × 4 bytes = 102 KB!

Even if weights are tiny, activations can be huge.
```

### Reducing Memory

| Technique | Weights | Activations |
|-----------|---------|-------------|
| Quantization (int8) | 4x smaller | 4x smaller |
| Pruning | 2-10x smaller | Same |
| Smaller model | Smaller | Smaller |
| Layer fusion | Same | Sometimes smaller |

### Computation Estimation

```
Dense(100, 50): 100 × 50 = 5,000 MACs
Conv1D(32, kernel=3, input=100): 100 × 32 × 3 = 9,600 MACs
Conv2D(32, kernel=3×3, input=28×28): 28 × 28 × 32 × 9 = 225,792 MACs

MAC = Multiply-Accumulate (1 multiply + 1 add)
```

### MCU Performance Reference

| MCU | MACs/second | Example Model |
|-----|-------------|---------------|
| AVR @ 16MHz | 1-5 MMAC/s | Too slow for NN |
| Cortex-M0 @ 48MHz | 10-20 MMAC/s | Tiny Dense only |
| Cortex-M4F @ 80MHz | 50-100 MMAC/s | Small CNN |
| Cortex-M7 @ 400MHz | 300-500 MMAC/s | Medium CNN |
| ESP32 @ 240MHz | 80-150 MMAC/s | Small CNN |

---

## Model Deployment

### Workflow

```
1. Train (PC)         2. Convert           3. Deploy (MCU)
   Python/PyTorch  →     TFLite/ONNX    →     C code
   Float32              Quantized            Inference
```

### Export Methods

**Method 1: C Header with Weights**

```c
// model_weights.h (generated)
const int8_t layer1_weights[100][50] = { ... };
const int32_t layer1_bias[50] = { ... };
```

**Method 2: TFLite Micro**

```c
#include "tensorflow/lite/micro/micro_interpreter.h"

tflite::MicroInterpreter interpreter(model, resolver, arena, arena_size);
interpreter.Invoke();
```

**Method 3: EIF Native**

```c
#include "eif_nn.h"

eif_dense_t layer1;
eif_dense_init(&layer1, weights, bias, 100, 50);

float output[50];
eif_dense_forward(&layer1, input, output);
eif_relu(output, 50);
```

---

## Common Mistakes

### Mistake 1: Ignoring Activation Memory

```c
// Model looks small (10 KB weights)
// But needs 200 KB activation buffer at runtime!

// SOLUTION: Calculate total memory requirement
size_t weights = 10 * 1024;
size_t max_activation = 50 * 1024;  // Largest layer output
size_t arena = weights + max_activation;  // Total: 60 KB
```

### Mistake 2: Training Float, Deploying Int8 Without Testing

```python
# Model is 95% accurate in float
model.evaluate(test_data)  # 95%

# But you deploy int8 without checking!
quantized_model.evaluate(test_data)  # 82% ← Oops!
```

Always test quantized model accuracy before deployment.

### Mistake 3: Input Normalization Mismatch

```python
# Training: inputs normalized -1 to +1
X_train = (X_train - mean) / std

# Deployment: forgot to normalize!
raw_input = read_sensor();  // Range 0-4095
prediction = model(raw_input);  // WRONG!
```

### Mistake 4: Wrong Tensor Layout

```python
# PyTorch: channels first [batch, channels, height, width]
# TensorFlow: channels last [batch, height, width, channels]

# If you convert wrong, model gives garbage!
```

### Mistake 5: Running Model Too Often

```c
// WRONG: Run at every loop iteration
while (1) {
    sensor = read_sensor();
    prediction = run_model(sensor);  // 100ms inference
    delay(10);  // Missed readings!
}

// RIGHT: Collect window, then predict once
while (1) {
    buffer[idx++] = read_sensor();
    if (idx == WINDOW_SIZE) {
        prediction = run_model(buffer);
        idx = 0;
    }
    delay(10);
}
```

---

## Quick Reference

### Layer Memory (Float32)

| Layer | Parameters | Formula |
|-------|------------|---------|
| Dense(in, out) | in×out + out | ~4 × in × out bytes |
| Conv1D(K, F) | K × channels × F | ~4 × K × channels × F bytes |
| Conv2D(K×K, F) | K² × channels × F | ~4 × K² × channels × F bytes |
| LSTM(hidden) | 4 × hidden² | ~16 × hidden² bytes |
| GRU(hidden) | 3 × hidden² | ~12 × hidden² bytes |

### Optimization Checklist

- [ ] Use int8 quantization
- [ ] Use depthwise separable convolutions
- [ ] Use pooling to reduce activation size
- [ ] Prune small weights
- [ ] Use smallest model that achieves accuracy
- [ ] Test on target hardware early

---

## Next Steps

1. **Try the demos**: `./bin/cnn_1d_timeseries --batch`
2. **Read**: [TINYML_DEPLOYMENT.md](../TINYML_DEPLOYMENT.md) for deployment
3. **Practice**: Train a small model on PC, deploy to MCU
4. **Experiment**: Compare float vs int8 inference time
