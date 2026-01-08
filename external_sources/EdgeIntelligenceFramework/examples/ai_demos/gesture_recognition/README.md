# Gesture Recognition Demo - Accelerometer Gesture Detection

## Overview
This tutorial demonstrates **gesture recognition** using 3-axis accelerometer data and pattern-based classification for wearable devices.

## Scenario
A smartwatch or wearable recognizes hand gestures:
- **Circle** - Circular hand motion
- **Swipe Left** - Quick leftward movement
- **Swipe Right** - Quick rightward movement
- **Shake** - Rapid back-and-forth motion

## Algorithms Used

### 1. Signal Preprocessing

**Noise Filtering:**
```c
// Simple low-pass filter
filtered[i] = α × raw[i] + (1-α) × filtered[i-1]
```

**Gravity Removal:**
```c
// High-pass filter to remove DC component
accel_dynamic = accel_raw - gravity_estimate
```

### 2. Feature Extraction

| Feature | Formula | Purpose |
|---------|---------|---------|
| **RMS Energy** | √(Σx²/N) | Motion intensity |
| **Peak Count** | Count local maxima | Oscillation detection |
| **Zero Crossing Rate** | Σ(sign(x[i]) ≠ sign(x[i-1])) | Frequency indicator |
| **Correlation** | Σ(x·y)/√(Σx²·Σy²) | Phase relationships |

**EIF API:**
```c
float rms = eif_dsp_rms_f32(accel_x, length);
float zcr = eif_dsp_zcr_f32(accel_x, length);
```

### 3. Gesture Classification

**Rule-Based Classification:**
```
CIRCLE:      High X+Y energy, high X-Y correlation (90° phase shift)
SWIPE LEFT:  High X energy, low Y energy, few peaks
SWIPE RIGHT: High X energy, low Y energy, few peaks
SHAKE:       High ZCR, many peaks, high frequency content
```

**Feature Vector:**
```c
struct features {
    float x_energy, y_energy, z_energy;
    float x_peaks, y_peaks;
    float x_zcr;
    float xy_correlation;
};
```

## Demo Walkthrough

1. **Gesture Simulation** - Generate 3-axis accelerometer patterns
2. **3-Axis Visualization** - ASCII display of X, Y, Z channels
3. **Feature Extraction** - Compute RMS, peaks, ZCR, correlation
4. **Feature Display** - Show extracted values
5. **Classification** - Apply decision rules
6. **Results** - Confidence bars for each gesture

## Gesture Signatures

| Gesture | X Pattern | Y Pattern | Key Feature |
|---------|-----------|-----------|-------------|
| Circle | Sine wave | Cosine wave | 90° phase shift |
| Swipe Left | Negative pulse | Low | Single peak, negative |
| Swipe Right | Positive pulse | Low | Single peak, positive |
| Shake | High-freq oscillation | Low | Many zero crossings |

## 3-Axis Visualization
```
│ X │ ▄▆█▇▅▃▁▁▃▅▇█▇▅▃▁ │  (Red)
│ Y │ ▁▃▅▇█▇▅▃▁▁▃▅▇█▇▅ │  (Green) 
│ Z │ ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄ │  (Blue - gravity)
```

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_dsp_rms_f32()` | RMS energy calculation |
| `eif_dsp_zcr_f32()` | Zero crossing rate |
| `eif_dsp_peak_detect()` | Find local maxima |
| `eif_dsp_fir_f32()` | Low-pass filtering |
| `eif_dsp_correlation_f32()` | Cross-correlation |

## Gesture Detection Pipeline

```
┌─────────────┐    ┌──────────────┐    ┌───────────────┐
│ Accel Data  │───►│ Preprocessing│───►│ Segmentation  │
│  (50 Hz)    │    │ (Filter, DC) │    │ (Window)      │
└─────────────┘    └──────────────┘    └───────┬───────┘
                                               │
┌─────────────┐    ┌──────────────┐    ┌───────▼───────┐
│  Output     │◄───│Classification│◄───│   Features    │
│  Gesture    │    │  (Rules/NN)  │    │  Extraction   │
└─────────────┘    └──────────────┘    └───────────────┘
```

## System Parameters

| Parameter | Typical Value | Notes |
|-----------|---------------|-------|
| Sample Rate | 50 Hz | Power vs responsiveness |
| Window Size | 2 seconds | Gesture duration |
| Overlap | 50% | Smoothness |
| Features | 7-10 | Complexity vs accuracy |

## Real-World Applications
- Smartwatch gesture control
- Fitness activity recognition
- Gaming controllers
- Sign language translation
- Industrial safety (fall detection)

## Extensions (Not in Demo)
- **DTW** - Dynamic Time Warping for template matching
- **HMM** - Hidden Markov Models for sequences
- **LSTM** - Recurrent networks for temporal patterns
- **1D CNN** - Convolutional feature learning

## Run the Demo
```bash
cd build && ./bin/gesture_recognition_demo
```
