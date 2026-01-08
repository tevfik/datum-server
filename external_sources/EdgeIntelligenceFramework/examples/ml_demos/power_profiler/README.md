# Power Profiler Demo

Track energy consumption and estimate battery life for edge AI models.

## Features

- Per-layer energy tracking
- INT8 vs FP32 comparison
- Battery life estimation
- Pre-deployment energy planning

## Usage

```bash
cd build && make power_demo && ./bin/power_demo
```

## Example Output

```
=== Demo 1: FP32 Model ===
Layer               MACs         Energy(nJ)   Time(us)
conv1               1.2M         12.0M        1200
...
Battery Life (370mWh, 1 FPS): 85.2 hours

=== Demo 2: INT8 Model ===
Energy Savings: 10x
Battery Life: 852 hours
```

## Applications

- Wearable device optimization
- IoT battery sizing
- Model architecture selection
- Quantization impact analysis
