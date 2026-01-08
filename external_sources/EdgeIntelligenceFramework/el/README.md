# Edge Learning Module (`el/`)

On-device machine learning algorithms for continual and distributed learning.

## Algorithms

| Algorithm | Description | File |
|-----------|-------------|------|
| **Federated Learning** | FedAvg distributed training | `eif_el_federated.c` |
| **EWC** | Elastic Weight Consolidation (continual) | `eif_el_ewc.c` |
| **Online Learning** | Concept drift adaptation | `eif_el_online.c` |
| **Few-Shot Learning** | Prototypical networks | `eif_el_fewshot.c` |
| **Q-Learning** | Tabular RL | `eif_el_rl.c` |
| **DQN** | Deep Q-Network with replay | `eif_el_dqn.c` |
| **Contextual Bandits** | Multi-armed bandits | `eif_el_rl.c` |

## Features

### Federated Learning
- FedAvg aggregation
- Privacy-preserving (local training)
- Support for non-IID data
- Configurable communication rounds

### Continual Learning (EWC)
- Prevents catastrophic forgetting
- Fisher Information regularization
- Elastic weight importance

### Online Learning
- Streaming data processing
- Concept drift detection
- Adaptive learning rate

### Few-Shot Learning
- N-way K-shot classification
- Prototypical network approach
- Euclidean distance metric

## Usage

```c
#include "eif_el.h"

// Federated Learning
eif_federated_client_t client;
eif_federated_client_init(&client, 100, 0.01f, &pool);
eif_federated_client_train(&client, data, labels, n_samples);
eif_federated_client_get_update(&client, weight_update);

// EWC
eif_ewc_context_t ewc;
eif_ewc_init(&ewc, 100, 0.001f, 1000.0f, &pool);
eif_ewc_compute_fisher(&ewc, data, labels, n);
eif_ewc_train_step(&ewc, x, y);

// Online Learning
eif_online_learner_t learner;
eif_online_learner_init(&learner, 10, 0.01f, &pool);
eif_online_learner_update(&learner, features, label);
```

## Demos

```bash
./bin/federated_learning_demo  # Multi-client simulation
./bin/ewc_learning_demo        # Catastrophic forgetting prevention
./bin/online_learning_demo     # Concept drift adaptation
./bin/fewshot_learning_demo    # Gesture recognition
```
