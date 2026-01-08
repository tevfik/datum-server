# KWS Demo - Keyword Spotting Pipeline

## Overview
This demo demonstrates a complete **Keyword Spotting (KWS)** pipeline combining audio preprocessing with neural network classification for real-time voice command detection.

## Demo Mode
The current demo runs with a **mock classifier** for demonstration purposes. It shows:
- Audio streaming and buffering
- MFCC feature extraction visualization
- Classification output display

```bash
./kws_demo
```

## Pipeline Architecture

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  Audio       │───►│    MFCC      │───►│   Neural     │───►│  Detection   │
│  Stream      │    │  Extraction  │    │   Network    │    │   Output     │
│  (16kHz)     │    │  (13 coeffs) │    │  (DS-CNN)    │    │  (keyword)   │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
```

---

## Building a Real KWS System

### Step 1: Get Training Data

**Option A: Google Speech Commands Dataset** (Recommended)
```bash
# Download dataset (~2GB)
wget http://download.tensorflow.org/data/speech_commands_v0.02.tar.gz
tar -xzf speech_commands_v0.02.tar.gz -C ./data/speech_commands
```

Keywords available: yes, no, up, down, left, right, on, off, stop, go

**Option B: Custom Recording**
```python
import sounddevice as sd
import soundfile as sf

# Record 1-second samples at 16kHz
audio = sd.rec(16000, samplerate=16000, channels=1)
sd.wait()
sf.write('keyword_sample.wav', audio, 16000)
```

### Step 2: Train the Model (TensorFlow/Keras)

```python
import tensorflow as tf
from tensorflow.keras import layers, models

# DS-CNN architecture (optimized for MCUs)
def create_ds_cnn(input_shape=(49, 13, 1), num_classes=12):
    model = models.Sequential([
        # First conv block
        layers.Conv2D(64, (10, 4), strides=(2, 2), padding='same', 
                      input_shape=input_shape),
        layers.BatchNormalization(),
        layers.ReLU(),
        
        # Depthwise separable conv blocks
        layers.DepthwiseConv2D((3, 3), padding='same'),
        layers.BatchNormalization(),
        layers.ReLU(),
        layers.Conv2D(64, (1, 1)),
        layers.BatchNormalization(),
        layers.ReLU(),
        
        layers.DepthwiseConv2D((3, 3), padding='same'),
        layers.BatchNormalization(),
        layers.ReLU(),
        layers.Conv2D(64, (1, 1)),
        layers.BatchNormalization(),
        layers.ReLU(),
        
        # Global pooling and classifier
        layers.GlobalAveragePooling2D(),
        layers.Dense(num_classes, activation='softmax')
    ])
    return model

# Create and train
model = create_ds_cnn()
model.compile(optimizer='adam',
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])

# Train (assumes X_train, y_train are MFCC features)
model.fit(X_train, y_train, epochs=30, validation_split=0.1)

# Save Keras model
model.save('kws_model.keras')
```

### Step 3: MFCC Feature Extraction (Training)

```python
import librosa
import numpy as np

def extract_mfcc(audio_path, n_mfcc=13, n_fft=512, hop_length=256):
    """Extract MFCC features matching EIF audio config"""
    audio, sr = librosa.load(audio_path, sr=16000)
    
    # Pad/trim to 1 second
    if len(audio) < 16000:
        audio = np.pad(audio, (0, 16000 - len(audio)))
    else:
        audio = audio[:16000]
    
    # Extract MFCC
    mfcc = librosa.feature.mfcc(
        y=audio, sr=sr,
        n_mfcc=n_mfcc,
        n_fft=n_fft,
        hop_length=hop_length
    )
    
    return mfcc.T  # Shape: (frames, coeffs)

# Process dataset
X_train = []
for audio_file in training_files:
    mfcc = extract_mfcc(audio_file)
    X_train.append(mfcc)
X_train = np.array(X_train)[..., np.newaxis]  # Add channel dim
```

### Step 4: Convert to TFLite

```python
# Convert with quantization (optional, for smaller model)
converter = tf.lite.TFLiteConverter.from_keras_model(model)

# Full integer quantization (best for MCUs)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_data_gen
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()

with open('kws_model.tflite', 'wb') as f:
    f.write(tflite_model)
```

### Step 5: Convert to EIF

```bash
python tools/tflite_to_eif.py kws_model.tflite kws_model.eif
```

### Step 6: Modify Demo for Real Model

```c
#include "eif_neural.h"
#include "eif_audio.h"

// Load model from file
uint8_t* model_buf = load_file("kws_model.eif", &model_size);
eif_model_t model;
eif_model_deserialize(&model, model_buf, model_size, &pool);

// Initialize context
eif_neural_context_t ctx;
eif_neural_init(&ctx, &model, &pool);

// In audio loop:
if (eif_audio_is_ready(&audio_ctx)) {
    const float* features = eif_audio_get_features(&audio_ctx);
    
    // Set input (MFCC features)
    eif_neural_set_input(&ctx, 0, features, feature_size);
    
    // Run inference
    eif_neural_invoke(&ctx);
    
    // Get output
    float output[NUM_KEYWORDS];
    eif_neural_get_output(&ctx, 0, output, sizeof(output));
    
    // Find keyword
    int keyword = argmax(output, NUM_KEYWORDS);
    if (output[keyword] > 0.8f) {
        printf("Detected: %s\n", keyword_names[keyword]);
    }
}
```

---

## Audio Configuration

| Parameter | Value | Description |
|-----------|-------|-------------|
| Sample Rate | 16000 Hz | Standard for voice |
| Frame Length | 512 samples | 32 ms window |
| Frame Stride | 256 samples | 16 ms hop |
| MFCC Coefficients | 13 | Cepstral features |
| Mel Filters | 26 | Filter bank size |
| Frequency Range | 20-4000 Hz | Voice band |
| Output Frames | 49 | ~1 second context |

## Model Architectures

| Architecture | Size | Accuracy | Latency |
|--------------|------|----------|---------|
| DS-CNN-S | 20 KB | 94% | 5 ms |
| DS-CNN-M | 80 KB | 96% | 10 ms |
| DS-CNN-L | 500 KB | 97% | 25 ms |
| CRNN | 200 KB | 95% | 15 ms |

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_audio_init()` | Initialize audio preprocessor |
| `eif_audio_push()` | Stream audio samples |
| `eif_audio_is_ready()` | Check if features ready |
| `eif_audio_get_features()` | Get MFCC features |
| `eif_model_deserialize()` | Load .eif model |
| `eif_neural_init()` | Initialize inference |
| `eif_neural_invoke()` | Run inference |

## Performance Targets

| Metric | Target |
|--------|--------|
| Latency | < 100 ms |
| Accuracy | > 95% |
| False Accepts | < 1/day |
| Power | < 1 mW |

## References

- [Google Speech Commands Dataset](https://www.tensorflow.org/datasets/catalog/speech_commands)
- [DS-CNN Paper](https://arxiv.org/abs/1711.07128)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
