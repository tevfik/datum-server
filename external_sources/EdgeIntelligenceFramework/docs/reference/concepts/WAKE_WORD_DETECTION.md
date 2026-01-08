# Wake Word Detection on MCUs

A complete guide to building keyword spotting (wake word detection) systems on microcontrollers.

> **What is Wake Word Detection?** It's how Alexa hears "Alexa", Siri hears "Hey Siri",
> or your custom device hears whatever phrase you train it to recognize - all running
> locally on tiny hardware!

---

## Table of Contents

1. [Overview](#overview)
2. [Audio Signal Processing](#audio-signal-processing)
3. [Feature Extraction](#feature-extraction)
4. [Classification Approaches](#classification-approaches)
5. [Memory-Efficient Implementation](#memory-efficient-implementation)
6. [Training Your Own Wake Word](#training-your-own-wake-word)
7. [EIF Implementation](#eif-implementation)

---

## Overview

Wake word detection converts audio into a binary decision:

```
[Microphone] → [Preprocessing] → [Features] → [Classifier] → "Wake word detected!"
     ↓              ↓               ↓             ↓
  16-bit PCM    Filtering       MFCCs        NN/DTW/HMM
  @ 16kHz       Noise gate      13 coeffs
```

### Challenges on MCUs

| Challenge | Solution |
|-----------|----------|
| Limited RAM | Streaming processing, small buffers |
| No FPU | Fixed-point MFCC |
| Real-time | Efficient feature extraction |
| Battery | Wake-on-threshold circuits |

---

## Audio Signal Processing

### Sampling

Most speech is in 300 Hz - 3400 Hz range. Nyquist says sample at 2x highest frequency:

```c
#define SAMPLE_RATE 16000   // 16 kHz - standard for speech
#define FRAME_SIZE 512      // 32 ms frames
#define HOP_SIZE 160        // 10 ms hop (overlap)
```

### Pre-emphasis

Boost high frequencies (speech has more energy in low frequencies):

```c
// y[n] = x[n] - α * x[n-1],  α ≈ 0.97
void pre_emphasis(int16_t* audio, int len, float alpha) {
    for (int i = len - 1; i > 0; i--) {
        audio[i] = audio[i] - (int16_t)(alpha * audio[i-1]);
    }
}
```

### Windowing

Apply Hamming window to reduce spectral leakage:

```c
// w[n] = 0.54 - 0.46 * cos(2π * n / (N-1))
void apply_hamming(float* frame, int len) {
    for (int i = 0; i < len; i++) {
        float w = 0.54f - 0.46f * cosf(2.0f * 3.14159f * i / (len - 1));
        frame[i] *= w;
    }
}
```

---

## Feature Extraction

### Mel-Frequency Cepstral Coefficients (MFCCs)

MFCCs are the standard features for speech recognition:

```
Audio → FFT → |Magnitude|² → Mel Filterbank → Log → DCT → MFCCs
```

#### 1. FFT (Fast Fourier Transform)

```c
// Convert time domain to frequency domain
// Use real-valued FFT for efficiency
void fft_magnitude(float* frame, int n, float* mags) {
    // ... FFT implementation
    // mags[i] = sqrt(real[i]² + imag[i]²)
}
```

#### 2. Mel Filterbank

Human ear perceives pitch logarithmically. Mel scale approximates this:

```c
// mel = 2595 * log10(1 + f/700)
float hz_to_mel(float hz) {
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

float mel_to_hz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}
```

Apply triangular filters spaced on mel scale:

```
         /\           /\           /\
        /  \         /  \         /  \
       /    \       /    \       /    \
------/------\-----/------\-----/------\------
    200Hz   400Hz   600Hz   1000Hz   2000Hz...
     (Filters overlap and are spaced on mel scale)
```

#### 3. Log Energy

```c
for (int i = 0; i < NUM_MEL_FILTERS; i++) {
    mel_energies[i] = logf(mel_energies[i] + 1e-10f);
}
```

#### 4. DCT (Discrete Cosine Transform)

Compute cepstral coefficients:

```c
#define NUM_MFCC 13

void dct(float* mel_log, int n_mel, float* mfcc) {
    for (int i = 0; i < NUM_MFCC; i++) {
        mfcc[i] = 0;
        for (int j = 0; j < n_mel; j++) {
            mfcc[i] += mel_log[j] * cosf(3.14159f * i * (j + 0.5f) / n_mel);
        }
    }
}
```

### Complete MFCC Frame

```c
typedef struct {
    float mfcc[13];      // Cepstral coefficients
    float delta[13];     // First derivative
    float delta2[13];    // Second derivative
    float energy;        // Frame energy
} mfcc_frame_t;
```

---

## Classification Approaches

### 1. Template Matching with DTW

Compare extracted MFCCs to stored templates:

```c
float dtw_distance(mfcc_frame_t* query, int q_len,
                   mfcc_frame_t* template, int t_len) {
    // Similar to gesture DTW but with MFCC vectors
    // Use Sakoe-Chiba band for efficiency
}

bool detect_wake_word(mfcc_frame_t* audio, int len) {
    float dist = dtw_distance(audio, len, wake_word_template, template_len);
    return dist < THRESHOLD;
}
```

**Pros**: Simple, works with few examples, easy to add new words
**Cons**: Slower, sensitive to speaking speed

### 2. Small Neural Network

A tiny neural network running on MCU:

```
[39 MFCCs] → [Dense 64] → [Dense 32] → [Dense 2] → [Softmax]
                ↓              ↓            ↓
             ReLU          ReLU       wake/not_wake
```

**Quantized Model Size**:
- Input: 39 × 20 frames = 780 features
- Layer 1: 780 × 64 = 50K params → 50 KB (8-bit)
- Layer 2: 64 × 32 = 2K params → 2 KB
- Layer 3: 32 × 2 = 64 params → 64 B
- **Total: ~52 KB flash, ~5 KB RAM**

### 3. Convolutional Neural Network

Process 2D spectrograms:

```
[Spectrogram 40×49] → Conv2D → MaxPool → Conv2D → Dense → Output
```

Even a tiny CNN can achieve 95%+ accuracy.

---

## Memory-Efficient Implementation

### Streaming Processing

Process audio in overlapping windows:

```c
#define RING_BUFFER_SIZE 1024  // ~64ms of audio

typedef struct {
    int16_t buffer[RING_BUFFER_SIZE];
    int write_idx;
    int read_idx;
    
    // MFCC history for classification
    mfcc_frame_t mfcc_history[20];  // ~200ms of features
    int mfcc_idx;
} wake_word_detector_t;

void process_audio_chunk(wake_word_detector_t* det, int16_t* samples, int n) {
    // Add to ring buffer
    for (int i = 0; i < n; i++) {
        det->buffer[det->write_idx] = samples[i];
        det->write_idx = (det->write_idx + 1) % RING_BUFFER_SIZE;
    }
    
    // Every hop_size, extract new MFCC frame
    // Shift history, add new frame
    // Run classifier on history
}
```

### Fixed-Point MFCC

For MCUs without FPU:

```c
// Q15 format: values in [-1, 1) with 15 fractional bits
typedef int16_t q15_t;

#define Q15_ONE 32767
#define Q15_MUL(a, b) ((((int32_t)(a) * (b)) >> 15))

void mfcc_fixed_point(q15_t* frame, int len, q15_t* mfcc) {
    // FFT in fixed-point
    q15_t fft_out[FRAME_SIZE];
    fft_q15(frame, fft_out, len);
    
    // Mel filterbank (precomputed Q15 coefficients)
    // Log approximation
    // DCT
}
```

### Energy-Based Voice Activity Detection

Don't run full classifier on silence:

```c
bool is_voice_activity(int16_t* frame, int len) {
    int32_t energy = 0;
    for (int i = 0; i < len; i++) {
        energy += abs(frame[i]);
    }
    return (energy / len) > ENERGY_THRESHOLD;
}
```

---

## Training Your Own Wake Word

### Data Collection

Record ~50-100 samples of your wake word:

```python
# Record with Python
import sounddevice as sd
import numpy as np

def record_sample(duration=1.5, sr=16000):
    print("Say the wake word...")
    audio = sd.rec(int(duration * sr), samplerate=sr, channels=1)
    sd.wait()
    return audio.flatten()

# Record positive samples
positives = [record_sample() for _ in range(50)]

# Record negative samples (other speech, noise)
negatives = [record_sample() for _ in range(50)]
```

### Training a Simple Model

```python
import tensorflow as tf
from tensorflow import keras

# Extract MFCCs (using librosa)
import librosa

def extract_mfcc(audio, sr=16000):
    return librosa.feature.mfcc(y=audio, sr=sr, n_mfcc=13, 
                                  n_fft=512, hop_length=160)

# Build tiny model
model = keras.Sequential([
    keras.layers.Input(shape=(13, 20)),  # 13 MFCCs, 20 frames
    keras.layers.Flatten(),
    keras.layers.Dense(64, activation='relu'),
    keras.layers.Dense(32, activation='relu'),
    keras.layers.Dense(2, activation='softmax')
])

# Train
model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', 
              metrics=['accuracy'])
model.fit(X_train, y_train, epochs=20, validation_split=0.2)

# Quantize and export
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()
```

---

## EIF Implementation

### Simple Wake Word Detector

```c
#include "eif_wake_word.h"

// Initialize
eif_wake_word_t detector;
eif_wake_word_init(&detector, 16000);  // 16 kHz

// Load wake word template (from flash or recorded)
eif_wake_word_load_template(&detector, "hey_device", template_data, len);

// In audio callback
void audio_callback(int16_t* samples, int n) {
    bool detected = eif_wake_word_process(&detector, samples, n);
    
    if (detected) {
        printf("Wake word detected! Listening...\n");
        start_command_recognition();
    }
}
```

### API Reference

```c
// Initialize detector
void eif_wake_word_init(eif_wake_word_t* ww, int sample_rate);

// Load a wake word template
bool eif_wake_word_load_template(eif_wake_word_t* ww, 
                                  const char* name,
                                  const float* mfcc_data, 
                                  int num_frames);

// Process audio chunk (returns true if wake word detected)
bool eif_wake_word_process(eif_wake_word_t* ww, 
                           int16_t* samples, 
                           int n_samples);

// Set detection threshold
void eif_wake_word_set_threshold(eif_wake_word_t* ww, float threshold);

// Get detection confidence (0-1)
float eif_wake_word_get_confidence(eif_wake_word_t* ww);
```

---

## Performance Comparison

| Approach | Accuracy | Latency | RAM | Flash |
|----------|----------|---------|-----|-------|
| DTW Template | 85-90% | 50-100ms | 2 KB | 5 KB |
| Tiny NN (8-bit) | 92-95% | 20-50ms | 5 KB | 50 KB |
| CNN (8-bit) | 95-98% | 30-80ms | 10 KB | 100 KB |
| TensorFlow Lite Micro | 96-99% | 50-150ms | 50 KB | 200 KB |

---

## Next Steps

1. **Try the demo**: `./bin/wake_word_demo`
2. **Record your wake word**: Use the training script
3. **Deploy to Arduino**: See `WakeWordDetector.ino`
4. **Optimize**: Try fixed-point MFCCs
