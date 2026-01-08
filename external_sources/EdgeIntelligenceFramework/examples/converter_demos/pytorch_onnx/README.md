# PyTorch to EIF Conversion Demo

Convert PyTorch models to EIF C code via ONNX.

## Requirements

```bash
pip install torch onnx numpy
```

## Quick Start

```bash
cd examples/converter_demos/pytorch_onnx
python pytorch_to_eif.py
```

## Workflow

```
┌─────────────┐      ┌─────────────┐      ┌─────────────┐
│   PyTorch   │ ───▶ │    ONNX     │ ───▶ │     EIF     │
│   (.pt)     │      │   (.onnx)   │      │   (.h/.c)   │
└─────────────┘      └─────────────┘      └─────────────┘
     Model           torch.onnx.export    eif_convert
```

## Example Output

```
PyTorch to EIF Conversion Demo
============================================================

🏗️  Creating PyTorch model...
   Total parameters: 12,250

📦 Exporting to ONNX: output/pytorch_model.onnx
   ✅ Exported successfully!

⚙️  Converting to EIF: output
   Parsed 15 layers
   
   Layer Summary:
   --------------------------------------------------
    0. conv2d          Conv_0
    1. batchnorm       BatchNormalization_1
    2. relu            Relu_2
    ...

   ✅ ONNX parsing successful!
```

## Supported PyTorch Layers

| PyTorch | ONNX Op | EIF Type |
|---------|---------|----------|
| nn.Conv2d | Conv | conv2d |
| nn.Linear | Gemm | dense |
| nn.ReLU | Relu | relu |
| nn.BatchNorm2d | BatchNormalization | (folded) |
| nn.MaxPool2d | MaxPool | maxpool2d |
| nn.AdaptiveAvgPool2d | GlobalAveragePool | global_avgpool |
| nn.Flatten | Flatten | flatten |
| nn.LSTM | LSTM | lstm |
