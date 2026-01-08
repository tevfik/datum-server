# Model Compression Toolkit

Quantization, pruning, and format conversion for deploying models on edge devices.

## Features

| Feature | Description |
|---------|-------------|
| **ONNX Import** | Load PyTorch/TensorFlow models via ONNX |
| **INT8 Quantization** | 4x size reduction, symmetric/asymmetric |
| **INT4 Quantization** | 8x size reduction, packed format |
| **Magnitude Pruning** | Unstructured weight pruning |
| **Structured Pruning** | Channel/filter pruning |
| **C Header Export** | Ready for embedded deployment |
| **Binary Export** | Runtime model loading |

## Installation

```bash
pip install onnx  # Required for ONNX support
```

## Usage

```bash
# Run demo
python model_compress.py --demo

# Show ONNX model info
python model_compress.py --input model.onnx --info

# Quantize ONNX to INT8
python model_compress.py --input model.onnx --quantize int8 --output model_int8.h

# Prune 30% + quantize  
python model_compress.py --input model.onnx --prune 0.3 --quantize int8 --output model.h

# Export to binary format
python model_compress.py --input model.onnx --quantize int8 --format binary --output model.bin
```

## Supported Formats

| Input | Output |
|-------|--------|
| `.onnx` (ONNX) | `.h` (C header) |
| `.npy` (NumPy) | `.bin` (Binary blob) |

## Example Output

```
Model Compression Statistics
==================================================
Original Size:     64.75 KB
Compressed Size:   16.19 KB
Compression Ratio: 4.00x
Total Parameters:  16,576
Quantization:      int8
```

## Integration with EIF

```c
#include "model_int8.h"

// Use quantized weights
eif_nn_layer_set_weights(&layer, layer1_weights, layer1_weights_scale);
```

