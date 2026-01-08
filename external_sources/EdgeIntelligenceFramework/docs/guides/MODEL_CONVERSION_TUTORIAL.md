# Model Conversion Tutorial

Convert Keras and TensorFlow models to optimized C code for embedded deployment.

---

## Quick Start

```python
import eif_convert as eif

# One-line conversion
eif.convert('model.h5', 'output/')
```

Or from command line:

```bash
python -m eif_convert model.h5 -o output/
```

---

## Table of Contents

1. [Installation](#installation)
2. [Basic Conversion](#basic-conversion)
3. [Quantization Options](#quantization-options)
4. [Advanced Usage](#advanced-usage)
5. [Deploying to MCU](#deploying-to-mcu)
6. [Troubleshooting](#troubleshooting)

---

## Installation

```bash
cd tools/eif_convert
pip install -r requirements.txt

# Or install directly
pip install tensorflow numpy
```

---

## Basic Conversion

### Step 1: Train your model

```python
import tensorflow as tf
from tensorflow import keras

# Create and train your model
model = keras.Sequential([
    keras.layers.Input(shape=(28, 28, 1)),
    keras.layers.Conv2D(32, 3, activation='relu'),
    keras.layers.MaxPooling2D(2),
    keras.layers.Flatten(),
    keras.layers.Dense(10, activation='softmax')
])

model.compile(optimizer='adam', loss='sparse_categorical_crossentropy')
model.fit(x_train, y_train, epochs=5)

# Save the model
model.save('my_model.h5')
```

### Step 2: Convert to EIF

```python
import eif_convert as eif

eif.convert(
    model='my_model.h5',
    output='eif_model/',
    quantize='q15',        # Q15 fixed-point
    per_channel=True,      # Better accuracy
    model_name='mymodel'
)
```

### Step 3: Use in C

```c
#include "mymodel_config.h"

int main() {
    int16_t input[MYMODEL_INPUT_SIZE];
    int16_t output[MYMODEL_OUTPUT_SIZE];
    
    // Load your input data...
    
    mymodel_inference(input, output);
    
    // Process output...
}
```

---

## Quantization Options

| Method | Bits | Range | Use Case |
|--------|------|-------|----------|
| `q15` | 16-bit | -1.0 to ~1.0 | Best accuracy |
| `q7` | 8-bit | -1.0 to ~1.0 | Smaller, faster |
| `int8` | 8-bit | -127 to 127 | TFLite compatible |
| `float` | 32-bit | Full range | No quantization |

### Per-Channel Quantization

Per-channel quantization uses different scales for each output channel,
improving accuracy for convolutional layers.

```python
eif.convert(model, output, quantize='q15', per_channel=True)
```

**Accuracy comparison:**

| Method | MNIST Accuracy |
|--------|---------------|
| Per-layer Q15 | 97.2% |
| Per-channel Q15 | 98.5% |
| Float32 | 98.9% |

---

## Advanced Usage

### Programmatic API

```python
from eif_convert import EIFConverter

converter = EIFConverter()

# Load model
converter.load_keras_model('model.h5')

# Configure quantization
converter.quantize(
    method='q15',
    per_channel=True,
    calibration_data=x_calibrate  # Optional
)

# Generate code
files = converter.generate(
    output_path='output/',
    model_name='mymodel',
    include_inference=True
)

# Print summary
print(converter.summary())
```

### CLI Options

```bash
python -m eif_convert model.h5 -o output/ \
    --quantize q15 \
    --per-channel \
    --name mymodel \
    --verbose
```

| Option | Description |
|--------|-------------|
| `-o, --output` | Output directory |
| `-q, --quantize` | Quantization method |
| `--per-channel` | Use per-channel quantization |
| `-n, --name` | Model name for generated code |
| `--no-inference` | Skip inference function generation |

---

## Deploying to MCU

### Generated Files

| File | Description |
|------|-------------|
| `model_weights.h` | Quantized weight arrays |
| `model_config.h` | Model defines and layer info |
| `model_inference.c` | Inference function |

### Integration

1. Copy generated files to your project
2. Add include paths for EIF headers
3. Call inference function

```c
#include "model_config.h"
#include "eif_nn.h"

void run_inference(void) {
    int16_t input[MODEL_INPUT_SIZE];
    int16_t output[MODEL_OUTPUT_SIZE];
    
    // Read sensor data into input...
    preprocess(sensor_data, input);
    
    // Run inference
    model_inference(input, output);
    
    // Find result
    int predicted = argmax(output, MODEL_OUTPUT_SIZE);
}
```

---

## Troubleshooting

### Common Issues

**1. Unsupported layer type**

```
Warning: Unsupported layer type 'Lambda', skipping
```

**Solution**: Replace with supported layers, or add custom implementation.

**2. BatchNormalization not folded**

```
Note: BatchNorm followed by non-linear, cannot fold
```

**Solution**: Ensure BatchNorm immediately follows Conv/Dense.

**3. Model too large for MCU**

**Solutions**:
- Use `q7` instead of `q15` (halves weight size)
- Reduce model complexity
- Use depthwise separable convolutions

### Getting Help

- Check [SUPPORTED_LAYERS.md](SUPPORTED_LAYERS.md) for layer compatibility
- See [examples/converter_demos/](../../examples/converter_demos/) for working examples
- Open an issue on GitHub
