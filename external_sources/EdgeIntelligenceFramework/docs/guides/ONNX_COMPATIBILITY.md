# ONNX Compatibility Guide

EIF Convert supports importing models from any ONNX-compatible framework.

---

## Supported Frameworks

| Framework | Export Method |
|-----------|---------------|
| PyTorch | `torch.onnx.export()` |
| TensorFlow | `tf2onnx.convert` |
| Keras | via TensorFlow |
| MXNet | `export_block()` |
| ONNX Model Zoo | Direct download |

---

## Supported ONNX Operators

### Convolution

| ONNX Op | EIF Type | Status |
|---------|----------|--------|
| Conv | conv2d | ✅ Full |
| ConvTranspose | conv2d_transpose | ⚠️ Basic |

### Pooling

| ONNX Op | EIF Type | Status |
|---------|----------|--------|
| MaxPool | maxpool2d | ✅ Full |
| AveragePool | avgpool2d | ✅ Full |
| GlobalAveragePool | global_avgpool | ✅ Full |
| GlobalMaxPool | global_maxpool | ✅ Full |

### Activations

| ONNX Op | EIF Type | Status |
|---------|----------|--------|
| Relu | relu | ✅ Full |
| LeakyRelu | leaky_relu | ✅ Full |
| Sigmoid | sigmoid | ✅ Full |
| Tanh | tanh | ✅ Full |
| Softmax | softmax | ✅ Full |
| Clip (ReLU6) | relu6 | ✅ Full |

### Linear

| ONNX Op | EIF Type | Status |
|---------|----------|--------|
| Gemm | dense | ✅ Full |
| MatMul | matmul | ⚠️ Basic |

### Normalization

| ONNX Op | EIF Type | Status |
|---------|----------|--------|
| BatchNormalization | (folded) | ✅ Full |
| LayerNormalization | layernorm | 🔜 Soon |

### RNN

| ONNX Op | EIF Type | Status |
|---------|----------|--------|
| LSTM | lstm | ✅ Full |
| GRU | gru | ✅ Full |
| RNN | rnn | ✅ Full |

### Shape Operations

| ONNX Op | EIF Type | Status |
|---------|----------|--------|
| Flatten | flatten | ✅ Full |
| Reshape | reshape | ✅ Full |
| Squeeze | squeeze | ✅ Full |
| Transpose | transpose | ⚠️ Basic |

---

## Usage

### From PyTorch

```python
import torch

# Export model to ONNX
model.eval()
dummy_input = torch.randn(1, 3, 224, 224)
torch.onnx.export(model, dummy_input, "model.onnx", opset_version=11)

# Convert to EIF
import eif_convert as eif
converter = eif.EIFConverter()
converter.load_onnx_model("model.onnx")
converter.quantize(method='q15')
converter.generate("output/")
```

### From TensorFlow

```python
import tf2onnx

# Convert TF SavedModel to ONNX
!python -m tf2onnx.convert --saved-model ./model --output model.onnx

# Then convert to EIF
python -m eif_convert model.onnx -o output/
```

---

## BatchNorm Folding

When a `BatchNormalization` op follows `Conv` or `Gemm`, EIF automatically 
folds the normalization into the previous layer's weights:

```
Conv → BatchNorm → ReLU
        ↓
Conv+BN (folded) → ReLU
```

This reduces:
- Computation at inference time
- Memory for BN parameters
- Latency

---

## Unsupported Operators

These ONNX operators are **not yet supported**:

- `Attention`, `MultiHeadAttention`
- `Conv3D`, `MaxPool3D`
- `Embedding`, `Gather` (for embeddings)
- `Dynamic shape operations`

Workarounds:
1. Simplify model before export
2. Use ONNX simplifier: `pip install onnx-simplifier`
3. Implement custom layer in C

---

## Troubleshooting

### "Unsupported operator X"

Run ONNX simplifier first:

```bash
pip install onnx-simplifier
python -m onnxsim model.onnx model_simplified.onnx
python -m eif_convert model_simplified.onnx -o output/
```

### Shape inference fails

Ensure model has static shapes:

```python
# PyTorch: Use fixed batch size
torch.onnx.export(..., dynamic_axes=None)
```
