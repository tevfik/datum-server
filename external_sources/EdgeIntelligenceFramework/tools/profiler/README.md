# Model Profiler

Analyzes neural network models for FLOPS, memory, and computational requirements.

## Features

- **FLOPS Calculation**: Layer-by-layer FLOPS analysis
- **Memory Footprint**: Weights + activation memory
- **MACs**: Multiply-Accumulate operations
- **Inference Time Estimation**: For embedded targets
- **Layer Distribution**: Visual breakdown of compute

## Usage

```bash
# Demo with synthetic model
python model_profiler.py --demo --verbose

# Profile a model file
python model_profiler.py model.json

# JSON output
python model_profiler.py model.json --json
```

## Supported Layers

- Conv2D, Conv1D (with groups/depthwise)
- Dense/Linear/FC
- MaxPool2D, AvgPool2D
- BatchNorm
- ReLU, Sigmoid, Tanh, Softmax, GELU
- Attention (Multi-Head)
- Embedding
- Flatten

## Model JSON Format

```json
{
  "name": "MyModel",
  "input_shape": [1, 3, 224, 224],
  "layers": [
    {"name": "conv1", "type": "conv2d", "in_channels": 3, "out_channels": 32, "kernel_size": 3},
    {"name": "fc", "type": "dense", "in_features": 32, "out_features": 10}
  ]
}
```

## Example Output

```
======================================================================
  MODEL PROFILE: DemoMobileNet
======================================================================

  Total Parameters:    6,281,000 (23.95 MB)
  Total FLOPS:         600.00M
  Est. Inference Time @ Cortex-M7: 6000.0 ms
```
