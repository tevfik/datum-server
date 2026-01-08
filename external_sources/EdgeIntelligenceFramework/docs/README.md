# Edge Intelligence Framework

**A unified, lightweight framework for deploying AI, ML, Signal Processing, and CV on edge devices.**

## Overview
The Edge Intelligence Framework (EIF) is designed to bring advanced intelligence to constrained embedded systems like ESP32, STM32, and Cortex-M devices. It provides efficient, clean C implementations of common algorithms without heavy external dependencies.

## Key Features

### 1. Signal Processing (DSP)
- **Filters**: Bi-Quad IIR (Low/High/Band-pass), FIR.
- **Transforms**: FFT (Radix-2), DCT, MFCC for audio.
- **Controllers**: PID (Float & Fixed-Point Q16.16).
- **Windows**: Hamming, Hanning, Blackman.

### 2. Machine Learning (ML)
- **Algorithms**: k-NN, k-Means, SVM (Linear), Gaussian Naive Bayes.
- **Edge Learning**: Online Learning, Federated Learning (FedAvg), Few-Shot Learning.
- **Anomaly Detection**: Statistical, Autoencoder-based.

### 3. Computer Vision (CV)
- **Image Processing**: Edge Detection (Canny/Sobel), Thresholding.
- **Features**: FAST Corner Detection, Template Matching.
- **Tracking**: Object Tracking (Centroid/Kalman).

### 4. Bayesian Filters
- **Kalman Filter**: Linear and Extended (EKF).
- **Fusion**: IMU Sensor Fusion (Accel + Gyro + Mag).

## Getting Started

1.  **Explore the Tutorials**: Select a category from the sidebar to view detailed demo documentation.
2.  **Build Examples**:
    ```bash
    mkdir build && cd build
    cmake .. && make
    ```
3.  **Run Demos**:
    ```bash
    ./bin/pid_interactive_demo
    ```

## Project Structure
- `core/`: Matrix math, utilities.
- `dsp/`: Signal processing library.
- `ml/`: Machine learning algorithms.
- `cv/`: Computer vision library.
- `hal/`: Hardware Abstraction Layer (Generic/Mock/ESP32).
- `examples/`: 50+ Demos and Tests.

[View Examples Overview](tutorials/examples_overview.html)
