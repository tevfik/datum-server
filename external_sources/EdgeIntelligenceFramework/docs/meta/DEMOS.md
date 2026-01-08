# Edge Intelligence Framework - Demo Reference

Complete list of all demonstration applications.

---

## DSP Demos (`examples/dsp_demos/`)

| Demo | Description | Features |
|------|-------------|----------|
| **smooth_filters** | Smoothing and noise reduction | EMA, Median, MA, Rate Limiter, Hysteresis, Debounce |
| **control_systems** | Control system utilities | Deadzone, Differentiator, Integrator, Zero-Crossing, Peak Detector |
| **biquad_eq** ✨NEW | Parametric audio equalizer | Lowpass, Peaking, Shelving, Butterworth cascade |
| **fir_filter** ✨NEW | FIR filter design | Window functions, LP/HP/BP design |
| **filter_benchmark** ✨NEW | Float vs Fixed-point comparison | FIR, Biquad, Q15 performance |
| **audio_classifier** | Voice command recognition | MFCC, neural network classification |
| **kws_demo** | Keyword spotting | Real-time audio, streaming |
| **signal_analysis** | Spectral analysis | FFT, power spectrum |
| **audio_eq** | Audio equalizer | Band filters, visualization |
| **pid_sim** | PID controller simulation | Step response, tuning |
| **pid_interactive** | Interactive PID tuning | Real-time parameter adjustment |
| **pid_fixed** | Fixed-point PID | Embedded optimization |

---

## AI Demos (`examples/ai_demos/`)

| Demo | Description | Features |
|------|-------------|----------|
| **mnist_cnn** | Digit recognition | CNN inference, model loading |
| **gesture_recognition** | Accelerometer gestures | Pattern matching, classification |
| **rl_gridworld** | Reinforcement learning | Q-learning, visualization |
| **model_conversion** | Model format conversion | TFLite → EIF |

---

## DL Demos (`examples/dl_demos/`) ✨NEW

| Demo | Description | Features |
|------|-------------|----------|
| **rnn_sequence** ✨NEW | GRU/LSTM sequence modeling | Time series, hidden state |
| **attention** ✨NEW | Self-attention mechanism | Transformer-style, positional encoding |

---

## NLP Demos (`examples/nlp_demos/`) ✨NEW

| Demo | Description | Features |
|------|-------------|----------|
| **tokenizer_demo** | Text tokenization | Vocabulary, encoding |
| **transformer_demo** | Transformer inference | Attention, embeddings |
| **phoneme_cmd** ✨NEW | Phoneme processing | ARPABET, edit distance |

---

## Filter Demos (`examples/filter_demos/`)

| Demo | Description | Features |
|------|-------------|----------|
| **drone_attitude** | Drone orientation | IMU fusion, Kalman filter |
| **robot_slam** | Robot localization | EKF-SLAM, mapping |
| **imu_fusion** | IMU sensor fusion | Complementary filter |

---

## ML Demos (`examples/ml_demos/`)

| Demo | Description | Features |
|------|-------------|----------|
| **anomaly_detector** | Outlier detection | Statistical methods |
| **ml_algorithms** | Classical ML showcase | K-NN, K-Means, PCA |
| **time_series** | Time series analysis | DTW, Matrix Profile |

---

## Hardware Demos (`examples/hw_demos/`)

| Demo | Description | Target |
|------|-------------|--------|
| **esp32_voice_cmd** | Voice commands | ESP32 |
| **esp32_cam_faces** | Face detection | ESP32-CAM |
| **stm32_motor** | Motor control | STM32 |

---

## Running Demos

All demos support standard CLI arguments:

```bash
./demo_name --help      # Show usage
./demo_name --batch     # Non-interactive mode
./demo_name --json      # JSON output
```

### Quick Start

```bash
# Build all demos
mkdir build && cd build
cmake ..
make -j4

# Run a demo
./bin/smooth_filters_demo --batch
./bin/control_systems_demo --help
./bin/gesture_recognition_demo --json
```

---

## Demo Categories by Skill Level

### Beginner
- `smooth_filters_demo` - Basic signal filtering
- `control_systems_demo` - Control utilities
- `pid_sim` - PID basics

### Intermediate
- `gesture_recognition_demo` - Pattern recognition
- `audio_classifier_demo` - Audio processing
- `anomaly_detector_demo` - Anomaly detection

### Advanced
- `robot_slam` - SLAM algorithms
- `rl_gridworld_demo` - Reinforcement learning
- `mnist_cnn_demo` - Neural network inference

---

## Creating New Demos

1. Create directory in appropriate category
2. Add `main.c` with standard CLI support
3. Add `CMakeLists.txt`
4. Add `README.md` or `TUTORIAL.md`
5. Update parent `CMakeLists.txt`
