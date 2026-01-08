# Audio Classification Demo - Voice Command Recognition (KWS)

## Overview
This tutorial demonstrates **Keyword Spotting** (KWS) for recognizing voice commands using MFCC features and neural network classification.

## Scenario
A smart device recognizes spoken commands:
- **"Yes"** - Confirmation
- **"No"** - Rejection
- **"Stop"** - Halt action
- **"Go"** - Start action

Always-on, low-power voice triggering for embedded devices.

## Algorithms Used

### 1. Audio Preprocessing

**Framing:**
```
Audio stream → Overlapping frames
Frame length: 25ms (400 samples at 16kHz)
Frame stride: 10ms (160 samples)
```

**Windowing:**
```c
window[i] = 0.5 × (1 - cos(2π × i / (N-1)))  // Hann window
```
Reduces spectral leakage at frame boundaries.

### 2. MFCC (Mel-Frequency Cepstral Coefficients)
Compact representation of audio spectrum mimicking human hearing.

**Pipeline:**
```
Audio → Pre-emphasis → Frame → Window → FFT → Mel Filter Bank → Log → DCT → MFCC
```

| Stage | Purpose |
|-------|---------|
| Pre-emphasis | Boost high frequencies |
| FFT | Time → Frequency domain |
| Mel Filter Bank | Perceptually-spaced frequency bands |
| Log | Compress dynamic range |
| DCT | Decorrelate, reduce dimensions |

**Mel Scale:**
```
Mel(f) = 2595 × log₁₀(1 + f/700)
```

**EIF API:**
```c
eif_mfcc_config_t cfg = {
    .num_mfcc = 13,
    .num_filters = 26,
    .fft_length = 512,
    .sample_rate = 16000
};
eif_dsp_mfcc_compute_f32(&cfg, fft_mag, mfcc_out, &pool);
```

### 3. Neural Network Classification
Simple dense network for command recognition.

**Architecture:**
```
Input: [num_frames × num_mfcc] = [49 × 13] = 637 features
Hidden: Dense(128, ReLU)
Output: Dense(4, Softmax) → [yes, no, stop, go]
```

**EIF API:**
```c
eif_neural_context_t ctx;
eif_neural_init(&ctx, &model, arena, &pool);
eif_neural_set_input(&ctx, mfcc_features);
eif_neural_invoke(&ctx);
eif_neural_get_output(&ctx, probabilities);
```

## Demo Walkthrough

1. **Audio Simulation** - Generate synthetic voice patterns
2. **Waveform Display** - ASCII visualization of audio
3. **MFCC Extraction** - Compute spectral features
4. **Spectrogram** - ASCII display of time-frequency features
5. **Classification** - Neural network inference
6. **Results** - Confidence bars for each command

## MFCC Visualization
```
Freq
  12 │.+##++.##+..++##..│
  11 │+###++####.+#####+│
  ...
   0 │#################+│ (C0 = log energy)
     └──────────────────┘ Time
```

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_audio_init()` | Initialize audio pipeline |
| `eif_audio_push()` | Push audio samples to buffer |
| `eif_dsp_mfcc_compute_f32()` | Extract MFCC features |
| `eif_dsp_window_hanning_f32()` | Apply window function |
| `eif_neural_invoke()` | Run inference |

## KWS System Design

| Component | Typical Value |
|-----------|---------------|
| Sample Rate | 16 kHz |
| Frame Length | 25 ms |
| Frame Stride | 10 ms |
| MFCC Coefficients | 13 |
| Mel Filters | 26-40 |
| Detection Window | 1 second |
| Inference Rate | 10 Hz |

## Power Considerations
- Feature extraction: ~1 MIPS
- Neural inference: ~5 MFLOPS
- Always-on power: <1 mW possible with DSP

## Real-World Applications
- Voice assistants (wake word)
- Smart home control
- Industrial voice interfaces
- Accessibility devices
- Toys and games

## Run the Demo
```bash
cd build && ./bin/audio_classifier_demo
```
