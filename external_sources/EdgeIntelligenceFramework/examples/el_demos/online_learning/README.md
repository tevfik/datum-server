# Online Learning Demo - Concept Drift Detection

Adaptive learning for streaming sensor data with automatic drift detection.

## Scenario

Industrial sensor monitoring where the environment suddenly changes:
- Phase 1 (Stable): Normal sensor readings
- Phase 2 (Drift): Distribution shift due to environment change  
- Phase 3 (Adapted): Model adapts to new distribution

## Features

- **AdaGrad optimization**: Adaptive learning rate
- **Drift detection**: Rolling window error monitoring
- **Automatic reset**: Clears accumulated state when drift detected
- **Colored ASCII visualization**: Error rate over time

## Build & Run

```bash
cmake -B build && cmake --build build --target online_learning_demo
./build/bin/online_learning_demo
```
