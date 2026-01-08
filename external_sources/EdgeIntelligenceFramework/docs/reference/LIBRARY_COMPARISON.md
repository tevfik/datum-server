# Edge ML Framework Comparison: EIF vs Industry Libraries

A comprehensive comparison of Edge Intelligence Framework (EIF) with popular edge ML libraries:
**nnom**, **MNN**, **ncnn**, and **uTensor**.

---

## Executive Summary

| Library | Origin | Primary Target | Strengths | Weaknesses |
|---------|--------|----------------|-----------|------------|
| **EIF** | TinyEdge | MCU/DSP | Lightweight, educational, DSP+ML | Limited NN operators |
| **nnom** | Community | MCU | Keras-direct, RNN, CMSIS-NN | Single framework (Keras) |
| **MNN** | Alibaba | Mobile/Edge | Multi-backend, LLM, production | Large size for MCU |
| **ncnn** | Tencent | Mobile | Zero deps, Vulkan GPU, fast | Complex for beginners |
| **uTensor** | ARM/UT Austin | MCU (mbed) | Code-gen, tiny runtime | Limited operators |

---

## Detailed Feature Comparison

### 1. Target Platform and Size

| Feature | EIF | nnom | MNN | ncnn | uTensor |
|---------|-----|------|-----|------|---------|
| **Primary Target** | Cortex-M0+ | Cortex-M4+ | Mobile/Edge | Mobile | Cortex-M/mbed |
| **Min RAM** | ~1KB | ~5KB | ~100KB | ~50KB | ~2KB |
| **Library Size** | ~10KB | ~20KB | ~400KB Android | ~300KB | ~2KB runtime |
| **FPU Required** | No (Q15) | No | Recommended | Recommended | No |
| **Bare Metal** | ✅ | ✅ | ⚠️ Limited | ⚠️ Limited | ✅ |
| **RTOS Support** | ✅ | ✅ | ✅ | ✅ | ✅ mbed OS |

### 2. Neural Network Capabilities

| Feature | EIF | nnom | MNN | ncnn | uTensor |
|---------|-----|------|-----|------|---------|
| **Conv2D** | ✅ | ✅ | ✅ | ✅ | ✅ |
| **DepthwiseConv** | ⚠️ Basic | ✅ | ✅ | ✅ | ⚠️ Limited |
| **Dense/FC** | ✅ | ✅ | ✅ | ✅ | ✅ |
| **RNN/LSTM/GRU** | ❌ | ✅ | ✅ | ✅ | ❌ |
| **Transformer** | ❌ | ❌ | ✅ | ✅ | ❌ |
| **Quantization** | Q15 only | Per-channel | FP16/INT8 | INT8/FP16 | INT8 |
| **Dilated Conv** | ❌ | ✅ | ✅ | ✅ | ❌ |
| **Dynamic Shapes** | ❌ | ⚠️ Limited | ✅ | ✅ | ❌ |

### 3. Framework/Model Support

| Feature | EIF | nnom | MNN | ncnn | uTensor |
|---------|-----|------|-----|------|---------|
| **Keras** | Export | ✅ Direct | Via ONNX | Via ONNX | Via TF |
| **TensorFlow** | Export | Via Keras | ✅ | Via ONNX | ✅ |
| **PyTorch** | ❌ | ❌ | Via ONNX | ✅ Native | Via ONNX |
| **ONNX** | ❌ | ❌ | ✅ 158 ops | ✅ | ⚠️ Limited |
| **Caffe** | ❌ | ❌ | ✅ 34 ops | ✅ | ❌ |
| **TFLite** | ❌ | ❌ | ✅ 58 ops | Limited | ❌ |

### 4. Hardware Acceleration

| Feature | EIF | nnom | MNN | ncnn | uTensor |
|---------|-----|------|-----|------|---------|
| **CMSIS-NN** | ✅ | ✅ (5x boost) | ❌ | ❌ | ✅ |
| **ARM NEON** | ❌ | ❌ | ✅ | ✅ Optimized | ❌ |
| **Vulkan GPU** | ❌ | ❌ | ✅ | ✅ | ❌ |
| **Metal (iOS)** | ❌ | ❌ | ✅ | ✅ | ❌ |
| **OpenCL** | ❌ | ❌ | ✅ | ✅ | ❌ |
| **NPU/Custom** | ❌ | ❌ | ⚠️ Planned | ⚠️ | ❌ |

### 5. Development Experience

| Feature | EIF | nnom | MNN | ncnn | uTensor |
|---------|-----|------|-----|------|---------|
| **Language** | C | C | C++ | C++ | C++ |
| **Documentation** | ✅ Extensive | ✅ Good | ✅ Chinese+ | ✅ Examples | ⚠️ Basic |
| **Learning Curve** | Low | Low | Medium | Medium | Low |
| **Deploy Script** | Python | Python | Python | Python/CLI | Python |
| **Examples** | Many | Many | Many | Many | Few |
| **Active Maint.** | ✅ | ✅ | ✅ | ✅ | ⚠️ Slow |

---

## Unique Strengths of Each Library

### 🔹 nnom Strengths
1. **One-line Keras Deployment** - `nnom.generate(model, 'weights.h')`
2. **Complex Architectures** - ResNet, DenseNet, Inception native support
3. **Onboard Evaluation** - Top-k accuracy, confusion matrix on MCU
4. **RNN Support** - LSTM, GRU, SimpleRNN with stateful option
5. **Pre-compiling** - Zero interpreter overhead at runtime

### 🔹 MNN Strengths
1. **Production Scale** - 100M+ daily inferences at Alibaba
2. **LLM Support** - Run large language models on mobile
3. **Multi-backend** - CPU/GPU/NPU hybrid scheduling
4. **MNN-CV** - Built-in image processing (no OpenCV needed)
5. **Workbench** - Visual model deployment tool

### 🔹 ncnn Strengths
1. **Zero Dependencies** - Pure C++, no external libs
2. **Vulkan GPU** - Fast GPU inference on Android
3. **Memory Efficiency** - Zero-copy model loading
4. **NEON Optimization** - Hand-tuned assembly for ARM
5. **8-bit/FP16** - Multiple quantization options

### 🔹 uTensor Strengths
1. **Tiny Runtime** - ~2KB core, smallest footprint
2. **Code Generation** - Generates standalone C++ files
3. **mbed Integration** - Deep ARM mbed OS support
4. **Memory Guarantee** - Guaranteed RAM bounds at compile time
5. **RomTensor/RamTensor** - Efficient ROM/RAM management

---

## EIF Current Advantages

| Advantage | Description |
|-----------|-------------|
| **DSP + ML Integration** | FIR/IIR filters, FFT, PID combined with ML |
| **Fixed-Point Focus** | Q15 optimized for no-FPU MCUs |
| **Educational Value** | Comprehensive docs, conceptual guides |
| **Bare-Metal First** | No RTOS required |
| **Sensor Fusion** | Kalman, complementary filters built-in |
| **Application Focus** | Activity recognition, predictive maintenance |
| **Small Footprint** | Minimal dependencies |

## EIF Current Gaps

| Gap | Libraries with Solution |
|-----|-------------------------|
| RNN/LSTM support | nnom, MNN, ncnn |
| ONNX import | MNN, ncnn |
| GPU acceleration | MNN, ncnn |
| Per-channel quantization | nnom, MNN |
| Model converter tool | All competitors |
| Transformer support | MNN, ncnn |
| Dilated convolutions | nnom, MNN, ncnn |

---

## Recommendations for EIF Enhancement

### 🎯 High Priority (Essential)

1. **Model Converter Script**
   - Add `eif_export.py` to convert Keras/TFLite models to C headers
   - Reference: nnom's single-line deployment approach
   ```python
   # Target API
   eif.convert(model, 'model.h', quantize='q15')
   ```

2. **ONNX Operator Support**
   - Implement core ONNX ops: Conv2D, Dense, BatchNorm, Concat
   - Priority: ~20 most common operators
   - Reference: MNN's 158 ONNX ops

3. **Per-Channel Quantization**
   - Current: Per-layer Q15
   - Target: Per-channel INT8/Q15
   - Benefit: ~10-20% accuracy improvement

### 🎯 Medium Priority (Competitive)

4. **RNN Layer Support**
   - Implement SimpleRNN, GRU, LSTM cells
   - Required for: Wake word, gesture sequences, time series
   - Reference: nnom's RNN implementation

5. **Structured Model API**
   - Add `eif_model_t` structure for model definition
   - Enable runtime model building vs compile-time only
   ```c
   eif_layer_t layers[] = {
       EIF_CONV2D(32, 3, 3, "relu"),
       EIF_MAXPOOL(2, 2),
       EIF_DENSE(10, "softmax")
   };
   eif_model_create(&model, layers, 3);
   ```

6. **Benchmark Suite Expansion**
   - Add standardized benchmarks (MNIST, CIFAR-10 subset)
   - Compare with nnom, TFLite Micro
   - Publish reproducible results

### 🎯 Nice to Have (Differentiation)

7. **On-Device Evaluation Tools**
   - Top-k accuracy calculator
   - Confusion matrix generator
   - Reference: nnom's onboard tools

8. **Tiny LLM Support** (Long-term)
   - Explore tiny transformer inference
   - Reference: MNN-LLM approach

9. **Visual Model Builder**
   - Web-based drag-and-drop model design
   - Export to EIF C code
   - Reference: Edge Impulse, MNN Workbench

---

## Feature Adoption Priority Matrix

```
                    IMPACT
                High ─────────────────────────────────────
                    │ Model Converter  │  ONNX Support    │
                    │ (nnom style)     │  (MNN style)     │
                    │                  │                  │
                    ├──────────────────┼──────────────────┤
                    │ Per-Channel      │  RNN Layers      │
                    │ Quantization     │  (nnom style)    │
                    │                  │                  │
                Low ├──────────────────┼──────────────────┤
                    │ Onboard Eval     │  GPU Backend     │
                    │ (nnom style)     │  (ncnn style)    │
                    │                  │                  │
                    ─────────────────────────────────────
                         Low              High
                              EFFORT
```

---

## Conclusion

EIF has a strong foundation with its DSP+ML integration and educational focus. 
To become competitive with industry libraries, prioritize:

1. **Immediate**: Model converter script (Keras → C header)
2. **Short-term**: Per-channel quantization, RNN support
3. **Medium-term**: ONNX basic support, structured API
4. **Long-term**: GPU backends, on-device evaluation

The biggest differentiators for EIF should remain:
- **Bare-metal friendly** (unlike MNN/ncnn)
- **DSP integration** (unique in this space)
- **Educational documentation** (best in class)
- **Q15 fixed-point** (FPU-less MCUs)
