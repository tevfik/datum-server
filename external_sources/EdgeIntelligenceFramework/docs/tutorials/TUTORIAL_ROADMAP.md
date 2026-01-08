# EIF Tutorial Roadmap: Zero to Hero

**Philosophy: "Theory follows Practice"**  
Run it first. See it work. Then understand how.

---

## 🎯 Learning Path Overview

```
Level 1: APPRENTICE          Level 2: CRAFTSMAN
   DSP & Math                   ML & Analysis
   ─────────────────────────────────────────────►
   │                             │
   │  FFT, Filters              │  Features, Anomaly
   │  Windowing                 │  Basic CV
   │                             │
   ▼                             ▼
   Project: Digital           Project: Smart
   Stethoscope               Vibration Sensor

Level 3: MASTER              Level 4: SAGE
   Deep Learning               Sensor Fusion
   ─────────────────────────────────────────────►
   │                             │
   │  CNN, RNN, LSTM            │  EKF, UKF, SLAM
   │  Attention, NLP            │  IMU Fusion
   │                             │
   ▼                             ▼
   Project: Keyword           Project: Drone
   Spotting                  Attitude Control
```

---

## Level 1: The Apprentice (DSP & Math)

**Focus**: `core/`, `dsp/`  
**Duration**: 1-2 weeks  
**Prerequisites**: Basic C, high school math

### Curriculum

| Week | Topic | Demo | Outcome |
|------|-------|------|---------|
| 1.1 | Vectors & Matrices | `api_tests/` | Understand EIF memory model |
| 1.2 | FFT & Spectrum | `signal_analysis_demo` | See frequency domain |
| 1.3 | Window Functions | `exp1_window_comparison` | Reduce spectral leakage |
| 1.4 | Digital Filters | `audio_classifier_demo` | Build FIR/IIR filters |

### 🔧 Capstone: Digital Stethoscope

Build a heart sound analyzer:
1. Record audio → MFCC features
2. Apply bandpass filter (20-200 Hz)
3. Detect heart rate via peak detection
4. Classify normal/abnormal

```bash
./bin/signal_analysis_demo --mode heart_sound
```

### Key APIs
- `eif_dsp_fft_f32()` - FFT
- `eif_dsp_filter_apply()` - Filtering  
- `eif_dsp_mfcc_compute()` - MFCC

---

## Level 2: The Craftsman (ML & Analysis)

**Focus**: `ml/`, `da/`, `cv/`  
**Duration**: 2-3 weeks  
**Prerequisites**: Level 1, basic statistics

### Curriculum

| Week | Topic | Demo | Outcome |
|------|-------|------|---------|
| 2.1 | Feature Extraction | `ml_algorithms_demo` | PCA, normalization |
| 2.2 | Anomaly Detection | `anomaly_detection_demo` | Isolation Forest, LOF |
| 2.3 | Time Series | `time_series_demo` | Forecasting, trending |
| 2.4 | Basic CV | `edge_detection_demo` | Sobel, Canny |
| 2.5 | Object Tracking | `object_tracking_demo` | Kalman tracker |

### 🔧 Capstone: Smart Vibration Sensor

Build bearing fault detector:
1. Read accelerometer at 10kHz
2. Compute FFT spectrum
3. Extract BPFO/BPFI frequencies
4. Classify fault type

```bash
./bin/bearing_fault_demo --json
```

### Key APIs
- `eif_ml_isolation_forest()` - Anomaly
- `eif_cv_sobel()` - Edge detection
- `eif_da_matrix_profile()` - Pattern discovery

---

## Level 3: The Master (Deep Learning & NLP)

**Focus**: `dl/`, `nlp/`  
**Duration**: 3-4 weeks  
**Prerequisites**: Level 2, linear algebra

### Curriculum

| Week | Topic | Demo | Outcome |
|------|-------|------|---------|
| 3.1 | Dense Layers | `gesture_recognition_demo` | Fully connected NN |
| 3.2 | CNNs | `mnist_cnn` | Convolution, pooling |
| 3.3 | RNN/LSTM | `voice_cmd_demo` | Sequence modeling |
| 3.4 | Attention | `transformer_demo` | Self-attention |
| 3.5 | Tokenization | `nlp_demo` | BPE, embeddings |

### 🔧 Capstone: Keyword Spotting

Build wake-word detector:
1. Audio → MFCC (40 bins, 25ms frames)
2. DS-CNN model (80KB INT8)
3. Streaming inference
4. Detect "hey_device", "yes", "no"

```bash
./bin/kws_demo --live
```

### Key APIs
- `eif_neural_invoke()` - Inference
- `eif_dl_context_init()` - Thread-safe context
- `eif_tokenizer_encode()` - Text processing

---

## Level 4: The Sage (Sensor Fusion & Robotics)

**Focus**: `bf/`  
**Duration**: 3-4 weeks  
**Prerequisites**: Level 3, probability basics

### Curriculum

| Week | Topic | Demo | Outcome |
|------|-------|------|---------|
| 4.1 | Kalman Filter | `imu_fusion_demo` | State estimation |
| 4.2 | EKF | `robot_slam_demo` | Nonlinear systems |
| 4.3 | Quaternions | `drone_attitude_demo` | 3D orientation |
| 4.4 | SLAM | `robot_slam_demo` | Mapping |
| 4.5 | Particle Filter | (advanced) | Non-Gaussian |

### 🔧 Capstone: Drone Attitude Control

Build quadcopter stabilization:
1. MPU6050 at 400Hz
2. Madgwick AHRS filter
3. PID control loop
4. Motor mixing (X-config)

```bash
./bin/drone_attitude_demo --json
```

### Key APIs
- `eif_ekf_init()` - Extended Kalman
- `eif_imu_fusion_update()` - IMU fusion
- `eif_slam_update()` - SLAM

---

## 📊 JSON Visualization Format

All demos support `--json` flag for real-time plotting:

```json
{
  "t": 1234,
  "type": "sensor",
  "signals": {
    "ax": 0.12, "ay": -0.05, "az": 9.81
  },
  "state": {
    "roll": 2.5, "pitch": -1.2, "yaw": 45.0
  },
  "prediction": "stable"
}
```

Visualize with:
```bash
./bin/imu_fusion_demo --json | python tools/visualizer/plot_realtime.py
```

---

## 🚀 Quick Start

```bash
# Build everything
mkdir build && cd build
cmake .. && make -j4

# Run your first demo
./bin/signal_analysis_demo

# See JSON output
./bin/imu_fusion_demo --json

# Interactive mode
./bin/interactive_demo
```

---

## 📁 Demo Organization

```
examples/
├── api_tests/         # Unit tests (Level 1)
├── dsp_demos/         # Signal processing (Level 1)
├── ml_demos/          # Machine learning (Level 2)
├── cv_demos/          # Computer vision (Level 2)
├── ai_demos/          # Neural networks (Level 3)
├── nlp_demos/         # NLP (Level 3)
├── filter_demos/      # Bayesian filters (Level 4)
├── el_demos/          # Edge learning (Advanced)
├── hw_demos/          # ESP32 hardware
├── projects/          # Complete applications
└── templates/         # Starter templates
```
