# On-Device Learning Demo

Demonstrates prototype-based learning for gesture personalization.

## Quick Start

```bash
cd build && make incremental_learning_demo
./bin/incremental_learning_demo
```

## Features

- **Prototype Learning**: Learn class centroids from examples
- **Online Updates**: Adapt incrementally with new samples
- **Personalization**: Fine-tune to user's specific style
- **Running Statistics**: Track mean/variance in streaming fashion

## Output Example

```
╔═══════════════════════════════════════════════════════════════╗
║          🎓 On-Device Learning Demo                            ║
╚═══════════════════════════════════════════════════════════════╝

📚 Phase 1: Learning from examples
  Learning 'Wave'...
  Learning 'Tap'...
  
  ✅ Learned 4 prototypes from 20 examples

📊 Phase 2: Testing recognition
  ✅ Wave -> Wave
  ✅ Tap -> Tap
  ...
  Accuracy: 18/20 = 90.0%

🔧 Phase 3: Personalization with user feedback
  ✅ Adapted to user's style with 10 corrections

📊 Phase 4: Post-adaptation testing
  Post-adaptation accuracy: 20/20 = 100.0%
```

## API Usage

```c
#include "eif_learning.h"

// Initialize classifier
eif_proto_classifier_t clf;
eif_proto_init(&clf, feature_dim);

// Learn from examples
eif_proto_update(&clf, features, label);

// Predict
int class = eif_proto_predict(&clf, features);
```
