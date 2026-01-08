# EWC Demo - Elastic Weight Consolidation

Prevent catastrophic forgetting in continual learning scenarios.

## Scenario

An edge device learns new sensor patterns without forgetting old ones:
- Task A: Recognize low-frequency patterns (1-3Hz sine, square, triangle)
- Task B: Recognize high-frequency patterns (5-7Hz sine, chirp, pulse)

## How EWC Works

1. Train on Task A normally
2. Compute Fisher Information (which weights are important for Task A)
3. Consolidate Task A (save optimal weights)
4. Train on Task B with EWC regularization (penalize changing important weights)
5. Result: Task A performance is preserved!

## Features

- **Pattern visualization**: ASCII waveforms
- **Side-by-side comparison**: With vs. without EWC
- **Fisher Information display**: Shows weight importance
- **Forgetting analysis**: Quantifies performance drop

## Build & Run

```bash
cmake -B build && cmake --build build --target ewc_learning_demo
./build/bin/ewc_learning_demo
```
