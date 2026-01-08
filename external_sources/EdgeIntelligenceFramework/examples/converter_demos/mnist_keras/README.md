# MNIST Keras Conversion Demo

End-to-end demo: Train MNIST → Convert to EIF → Run on MCU

## Quick Start

```bash
# 1. Train and convert
cd examples/converter_demos/mnist_keras
python train_mnist.py

# 2. Build C demo
cd ../../../build
make mnist_keras_demo

# 3. Run
./bin/mnist_keras_demo
```

## Files

| File | Description |
|------|-------------|
| `train_mnist.py` | Train CNN on MNIST, convert to EIF |
| `main.c` | C inference demo |
| `output/` | Generated C code (after conversion) |

## Model Architecture

```
Input (28x28x1)
    ↓
Conv2D(16, 3x3) + BN + ReLU
    ↓
MaxPool(2x2)
    ↓
Conv2D(32, 3x3) + BN + ReLU
    ↓
MaxPool(2x2)
    ↓
Flatten
    ↓
Dense(64) + ReLU
    ↓
Dense(10) + Softmax
```

## Expected Output

```
╔═══════════════════════════════════════╗
║     MNIST Inference Demo (EIF)        ║
╚═══════════════════════════════════════╝

Class Probabilities:
═══════════════════════════════════════
  0: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 3.0%
  1: ██████████████████████████████ 76.2%
  2: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 3.0%
  ...

Predicted digit: 1
```

## Conversion Options

```bash
# Q15 (default, best accuracy)
python -m eif_convert mnist_model.h5 -o output/ -q q15

# Q7 (smaller, faster)
python -m eif_convert mnist_model.h5 -o output/ -q q7

# Per-channel quantization (better accuracy)
python -m eif_convert mnist_model.h5 -o output/ -q q15 --per-channel
```
