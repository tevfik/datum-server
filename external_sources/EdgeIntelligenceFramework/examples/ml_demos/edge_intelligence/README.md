# Edge Intelligence Demo

Comprehensive showcase of all EIF edge intelligence capabilities.

## Features

This demo showcases:
- **Model Quantization** - INT8 symmetric and asymmetric quantization
- **Adaptive Thresholds** - Self-adjusting anomaly detection
- **Sensor Fusion** - Multi-sensor data combination
- **Online Learning** - On-device model updates
- **Edge Inference** - Model profiling and memory estimation

## Running

```bash
./build/bin/edge_intelligence_demo --batch
```

## Sample Output

```
╔════════════════════════════════════════╗
║  EIF Edge Intelligence Demo             ║
╚════════════════════════════════════════╝

╔════════════════════╗
║  1. Quantization   ║
╚════════════════════╝

  Original (float32): 93.7f, -41.2f, 128.5f
  INT8 quantized:     94, -41, 127
  Dequantized:        93.5f, -40.9f, 126.8f
  
  Quantization quality:
    MSE:  0.234
    SQNR: 45.2 dB

╔═════════════════════════╗
║  2. Adaptive Threshold  ║
╚═════════════════════════╝

  Feeding 50 normal samples...
  Stats: mean=0.501, std=0.078
  
  Testing anomaly detection:
    0.52 (normal)  → Not anomaly
    0.95 (spike)   → ANOMALY!
    -0.30 (drop)   → ANOMALY!
```

## Use Cases

- TinyML model deployment
- Real-time anomaly detection
- Multi-sensor fusion systems
- Resource-constrained inference

## API Overview

| Module | Purpose |
|--------|---------|
| `eif_quantize.h` | Model weight quantization |
| `eif_adaptive_threshold.h` | Self-tuning detection |
| `eif_sensor_fusion.h` | Multi-sensor combining |
| `eif_online_learning.h` | On-device updates |
| `eif_edge_inference.h` | Profiling & memory |
