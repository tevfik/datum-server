# Edge Learning Demo

On-device learning algorithms for privacy-preserving, adaptive edge AI.

## Features

| Algorithm | Use Case | Privacy |
|-----------|----------|---------|
| **Federated Learning** | Collaborative training | ✅ Data stays local |
| **Continual Learning (EWC)** | Learn without forgetting | ✅ On-device |
| **Online Learning** | Streaming adaptation | ✅ On-device |
| **Few-Shot Learning** | Quick personalization | ✅ On-device |

## Usage

```bash
cd build && make edge_learning_demo && ./bin/edge_learning_demo
```

## Example Output

```
Demo 1: Federated Learning (FedAvg)
  Client 1: Trained on 2 samples
  Client 2: Trained on 2 samples
  Client 3: Trained on 2 samples
  Aggregated global weights[0:3]: [0.100, 0.050, 0.080]

Demo 4: Few-Shot Learning
  Query 1 -> Wave (distance: 0.123)
  Class probabilities: Wave: 85%, Swipe: 10%, Tap: 5%
```

## Real-World Applications

| Algorithm | Application |
|-----------|-------------|
| Federated | Keyboard prediction (Gboard), Health devices |
| EWC | Voice assistant learning new commands |
| Online | Sensor drift compensation |
| Few-Shot | User gesture personalization |
