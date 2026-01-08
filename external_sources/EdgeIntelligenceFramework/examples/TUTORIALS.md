# EIF Tutorial Series

**20 tutorials · 48 demos · 3 complete projects**

## 📚 Tutorials by Track

### 🔊 Audio (3)
- [Audio Classifier](dsp_demos/audio_classifier/TUTORIAL.md) - KWS, MFCC
- [Signal Analysis](dsp_demos/signal_analysis/TUTORIAL.md) - FFT, vibration
- [KWS Demo](dsp_demos/kws_demo/TUTORIAL.md) - Wake word detection

### 🤖 Robotics (3)
- [IMU Fusion](filter_demos/imu_fusion/TUTORIAL.md) - EKF, quaternions
- [Robot SLAM](filter_demos/robot_slam/TUTORIAL.md) - EKF-SLAM
- [Drone Attitude](filter_demos/drone_attitude/TUTORIAL.md) - Madgwick, PID

### 📡 IoT (3)
- [Anomaly Detection](ml_demos/anomaly_detection/TUTORIAL.md) - Ensemble
- [Time Series](ml_demos/time_series/TUTORIAL.md) - Forecasting
- [Matrix Profile](ml_demos/matrix_profile/TUTORIAL.md) - Pattern discovery

### 📷 Computer Vision (3)
- [Edge Detection](cv_demos/edge_detection/TUTORIAL.md) - Sobel, Canny
- [Object Tracking](cv_demos/object_tracking/TUTORIAL.md) - Kalman
- [Feature Matching](cv_demos/feature_matching/TUTORIAL.md) - FAST, Harris

### 🧠 Edge Learning (4)
- [Federated Learning](el_demos/federated_learning/TUTORIAL.md) - FedAvg
- [EWC Learning](el_demos/ewc_learning/TUTORIAL.md) - Continual
- [Few-Shot Learning](el_demos/fewshot_learning/TUTORIAL.md) - Prototypes
- [Online Learning](el_demos/online_learning/TUTORIAL.md) - Streaming

### 🎮 AI/NN (3)
- [Gesture Recognition](ai_demos/gesture_recognition/TUTORIAL.md) - IMU
- [MNIST CNN](ai_demos/mnist_cnn/TUTORIAL.md) - LeNet
- [RL Gridworld](ai_demos/rl_gridworld/TUTORIAL.md) - Q-Learning

### 💬 NLP (1)
- [Transformer](nlp_demos/transformer_demo/TUTORIAL.md) - Attention

---

## 🔧 ESP32 Hardware Demos

| Demo | Target | Purpose |
|------|--------|---------|
| `esp32_cam_faces_demo` | ESP32-CAM | Face detection |
| `esp32_voice_cmd_demo` | ESP32 + mic | Voice commands |
| `smart_sensor_demo` | ESP32-mini | Multi-sensor hub |
| `motion_detect_demo` | ESP32-CAM | Motion + counting |

---

## 🚀 Complete Projects

| Project | Description |
|---------|-------------|
| [Smart Doorbell](projects/smart_doorbell/) | Motion → Person → Alert |
| [Fitness Tracker](projects/fitness_tracker/) | Activity, steps, calories |
| [Plant Monitor](projects/plant_monitor/) | Sensors → Health score |

---

## Quick Start

```bash
make -j4
./bin/drone_attitude_demo
./bin/smart_doorbell_demo
./bin/fitness_tracker_demo
./bin/plant_monitor_demo
```
