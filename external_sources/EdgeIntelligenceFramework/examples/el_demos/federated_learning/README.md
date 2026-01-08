# Federated Learning Demo

Privacy-preserving collaborative model training using the FedAvg algorithm.

## Scenario

5 factory sensors collaboratively train a temperature prediction model:
- Each sensor holds private local data (non-IID distribution)
- Raw data NEVER leaves the devices
- Only gradients are shared and aggregated

## Features

- **Multi-client simulation**: 5 independent edge devices
- **Non-IID data**: Each client has biased data distribution  
- **Privacy metrics**: Gradient norm, data leakage risk
- **Convergence visualization**: ASCII loss curve
- **FedAvg aggregation**: Weighted averaging by sample count

## Build & Run

```bash
cmake -B build && cmake --build build --target federated_learning_demo
./build/bin/federated_learning_demo
```

## Output

- Client data distribution table
- Per-round progress with loss
- Client contribution summary
- Training loss curve
- Privacy guarantees summary
