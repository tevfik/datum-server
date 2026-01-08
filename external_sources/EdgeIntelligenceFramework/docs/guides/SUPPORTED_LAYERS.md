# Supported Layers

Layers supported by the EIF model converter.

---

## Core Layers

| Keras Layer | EIF Type | Status | Notes |
|-------------|----------|--------|-------|
| `Input` | input | ✅ Full | |
| `Conv2D` | conv2d | ✅ Full | Supports dilations |
| `DepthwiseConv2D` | dwconv2d | ✅ Full | |
| `SeparableConv2D` | sepconv2d | ⚠️ Partial | Split into DW+PW |
| `Dense` | dense | ✅ Full | |
| `Flatten` | flatten | ✅ Full | No-op in memory |
| `Reshape` | reshape | ✅ Full | |

## Pooling Layers

| Keras Layer | EIF Type | Status | Notes |
|-------------|----------|--------|-------|
| `MaxPooling2D` | maxpool2d | ✅ Full | |
| `AveragePooling2D` | avgpool2d | ✅ Full | |
| `GlobalMaxPooling2D` | global_maxpool2d | ✅ Full | |
| `GlobalAveragePooling2D` | global_avgpool2d | ✅ Full | |

## Activation Layers

| Keras Layer | EIF Type | Status | Notes |
|-------------|----------|--------|-------|
| `Activation` | activation | ✅ Full | |
| `ReLU` | relu | ✅ Full | |
| `LeakyReLU` | leaky_relu | ✅ Full | |
| `Softmax` | softmax | ✅ Full | |
| `Sigmoid` | sigmoid | ✅ Full | LUT-based |
| `Tanh` | tanh | ✅ Full | LUT-based |

## Normalization Layers

| Keras Layer | EIF Type | Status | Notes |
|-------------|----------|--------|-------|
| `BatchNormalization` | batchnorm | ✅ Folded | Merged into Conv/Dense |
| `LayerNormalization` | layernorm | ❌ Not yet | Planned |

## Merge Layers

| Keras Layer | EIF Type | Status | Notes |
|-------------|----------|--------|-------|
| `Concatenate` | concat | ✅ Full | Any axis |
| `Add` | add | ✅ Full | |
| `Multiply` | multiply | ✅ Full | |
| `Subtract` | subtract | ⚠️ Partial | |

## Regularization Layers

| Keras Layer | EIF Type | Status | Notes |
|-------------|----------|--------|-------|
| `Dropout` | (ignored) | ✅ Skipped | Not used in inference |
| `SpatialDropout2D` | (ignored) | ✅ Skipped | |

## Padding Layers

| Keras Layer | EIF Type | Status | Notes |
|-------------|----------|--------|-------|
| `ZeroPadding2D` | zeropad2d | ✅ Full | |
| `Cropping2D` | cropping2d | ⚠️ Basic | |

## RNN Layers (Phase 3)

| Keras Layer | EIF Type | Status | Notes |
|-------------|----------|--------|-------|
| `SimpleRNN` | rnn | 🔜 Planned | |
| `LSTM` | lstm | 🔜 Planned | |
| `GRU` | gru | 🔜 Planned | |

---

## Layer Parameters

### Conv2D

```python
Conv2D(
    filters,          # ✅ Supported
    kernel_size,      # ✅ Supported
    strides=(1,1),    # ✅ Supported
    padding='valid',  # ✅ 'valid' or 'same'
    dilation_rate=(1,1),  # ✅ Supported
    activation=None,  # ✅ Fused if linear
    use_bias=True     # ✅ Supported
)
```

### Dense

```python
Dense(
    units,            # ✅ Supported
    activation=None,  # ✅ Fused if linear
    use_bias=True     # ✅ Supported
)
```

---

## Unsupported Layers

These layers are **not supported** and will be skipped or cause errors:

- `Lambda` - Custom functions cannot be converted
- `TimeDistributed` - Use explicit loop
- `Embedding` - Use lookup table
- `Attention` - Planned for Phase 7
- `Conv3D` - 3D not supported
- Custom layers - Must implement manually

---

## Adding Custom Layer Support

For unsupported layers, you can:

1. Replace with equivalent supported layers
2. Implement manually in C
3. Use Lambda with simple operations

```python
# Instead of
x = Lambda(lambda x: x * 2)(x)

# Use
x = layers.Multiply()([x, tf.constant(2.0)])
```
