# MNIST CNN Demo - Handwritten Digit Classification

## Overview
This demo runs inference on a pre-trained CNN model to classify handwritten digits (0-9) from the MNIST dataset.

## Usage

```bash
./mnist_cnn <model.eif> <image.bin>
```

**Arguments:**
- `model.eif` - Converted EIF model file (from TFLite)
- `image.bin` - Raw binary image data (28x28 grayscale, 784 bytes)

## Workflow

### 1. Train Model (Python/TensorFlow)
```python
import tensorflow as tf

model = tf.keras.Sequential([
    tf.keras.layers.Conv2D(32, 3, activation='relu', input_shape=(28, 28, 1)),
    tf.keras.layers.MaxPooling2D(),
    tf.keras.layers.Conv2D(64, 3, activation='relu'),
    tf.keras.layers.MaxPooling2D(),
    tf.keras.layers.Flatten(),
    tf.keras.layers.Dense(128, activation='relu'),
    tf.keras.layers.Dense(10, activation='softmax')
])

model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
model.fit(train_images, train_labels, epochs=5)

# Export to TFLite
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()
with open('mnist.tflite', 'wb') as f:
    f.write(tflite_model)
```

### 2. Convert to EIF
```bash
python tools/tflite_to_eif.py mnist.tflite mnist.eif
```

### 3. Prepare Test Image
```python
import numpy as np
from tensorflow.keras.datasets import mnist

(_, _), (test_images, _) = mnist.load_data()
test_images[0].astype(np.float32).tofile('test_image.bin')
```

### 4. Run Inference
```bash
./mnist_cnn mnist.eif test_image.bin
```

## CNN Architecture

```
Input (28x28x1)
    │
    ▼
Conv2D (32 filters, 3x3, ReLU)
    │
    ▼
MaxPool2D (2x2)
    │
    ▼
Conv2D (64 filters, 3x3, ReLU)
    │
    ▼
MaxPool2D (2x2)
    │
    ▼
Flatten
    │
    ▼
Dense (128, ReLU)
    │
    ▼
Dense (10, Softmax)
    │
    ▼
Output (10 classes)
```

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_model_deserialize()` | Load model from binary buffer |
| `eif_neural_init()` | Initialize inference context |
| `eif_neural_set_input()` | Set input tensor data |
| `eif_neural_invoke()` | Run forward pass |
| `eif_neural_get_output()` | Get output predictions |

## Output Example

```
Initializing EIF...
Model loaded. Nodes: 8, Tensors: 12
Running inference...
Predictions:
  0: 0.001234
  1: 0.000012
  2: 0.000045
  3: 0.987654   ← Highest
  4: 0.000123
  5: 0.002345
  6: 0.000456
  7: 0.005678
  8: 0.001234
  9: 0.001219
Predicted Class: 3
```

## Performance

| Metric | Value |
|--------|-------|
| Model Size | ~100 KB |
| Inference Time | ~5-10 ms (MCU) |
| Memory Usage | ~200 KB |
| Accuracy | ~99% (MNIST) |

## Files in This Directory

| File | Description |
|------|-------------|
| main.c | Demo application source |
| CMakeLists.txt | Build configuration |
| README.md | This documentation |
