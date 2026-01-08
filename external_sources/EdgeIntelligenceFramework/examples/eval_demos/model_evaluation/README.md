# Model Evaluation Demo

Demonstrates onboard model evaluation with metrics and profiling.

## Quick Start

```bash
cd build && make model_evaluation_demo
./bin/model_evaluation_demo
```

## Features

- **Confusion Matrix**: Visual breakdown of predictions vs actual
- **Per-Class Metrics**: Precision, recall, F1 for each class
- **Overall Accuracy**: Total correct / total samples
- **Timing Analysis**: Min/max/avg inference time
- **Layer Profiling**: Time breakdown by layer

## Output Example

```
╔═══════════════════════════════════════════════════════════════╗
║              Model Evaluation Results                           ║
╚═══════════════════════════════════════════════════════════════╝

Overall Metrics:
───────────────────────────────────────────────────────
  Samples:    100
  Correct:    72
  Accuracy:   72.00%
  Macro F1:   0.6854

Per-Class Metrics:
───────────────────────────────────────────────────────
  Class   Precision   Recall      F1     Support
      0      85.71%   80.00%   0.8276      30
      1      66.67%   72.00%   0.6923      25
      ...

Confusion Matrix:
───────────────────────────────────────────────────────
           0    1    2    3    4  <- Actual
    0:    24    4    2    .    1
    1:     3   18    3    1    .
    ...
```

## API Usage

```c
#include "eif_eval.h"

eif_eval_t eval;
eif_eval_init(&eval, 10);  // 10 classes

for (int i = 0; i < test_size; i++) {
    model_infer(test_x[i], output);
    eif_eval_update(&eval, output, test_y[i]);
}

printf("Accuracy: %.2f%%\n", eif_eval_accuracy(&eval) * 100);
eif_eval_print(&eval);
```
