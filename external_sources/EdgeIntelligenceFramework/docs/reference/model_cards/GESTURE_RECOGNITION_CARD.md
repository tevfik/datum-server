# Model Card: Gesture Recognition NN (`gesture_nn.eif`)

## Model Details
- **Model Date:** 2025-12-17
- **Version:** 1.0.0
- **Model Type:** Multi-Layer Perceptron (MLP) / Feed-Forward Neural Network
- **Input Format:** 300 inputs (Flattened vector: 6 axes × 50 time steps) taking valid float32 values.
    - Axes: Accel X, Y, Z, Gyro X, Y, Z
    - Sampling Rate: 50 Hz (1 second window)
- **Output:** Probability distribution over 6 classes (Softmax).
- **License:** MIT
- **Author/Owner:** Edge Intelligence Framework (EIF) Team / Tevfik

## Intended Use
- **Primary Use Case:** Real-time gesture recognition on low-power wearable devices (smartwatches, fitness trackers).
- **Supported Gestures:**
    1. Idle
    2. Wave
    3. Circle
    4. Tap
    5. Swipe Left
    6. Swipe Right
- **Target Platform:** ARM Cortex-M4/M7/M55, ESP32-S3, or similar MCUs with FPU.
- **Out of Scope:** Continuous activity recognition (e.g., walking/running classification) or complex sign language translation.

## Training Data
- **Dataset:** EIF Synthetic Gesture Dataset (refer to `tools/generate_models.py`)
- **Generation Method:** Procedurally generated sensor patterns simulating idealized hand movements.
- **Preprocessing:**
    - Input normalization (Mean=0, Std=1)
    - Flattening of `(50, 6)` signal to `(300,)` vector.
- **Data Split:** N/A (Procedural generation used for demonstration weights).

## Performance (Estimated)
- **Accuracy:** ~92% (on synthetic validation set)
- **Latency:**
    - **Cortex-M4F (64MHz):** < 0.8 ms
    - **ESP32-S3 (240MHz):** < 0.2 ms
- **Memory Footprint:**
    - **Weights (Flash):** ~41 KB (10,262 parameters)
    - **Activations (RAM):** ~1.5 KB (Scratch buffer for largest layer)
- **Robustness:**
    - **Orientation:** Model expects sensor to be roughly aligned with wrist orientation.
    - **Drift:** Input data should be high-pass filtered to remove gravity/drift before inference.

## Ethical Considerations & Limitations
- **Bias:** Synthetic training data implies "ideal" gestures. Real-world performance may vary significantly across different users (arm length, speed of motion).
- **Risks:** False positives (triggering a command unintendedly) could cause user frustration. Not suitable for safety-critical control (e.g., medical devices, heavy machinery).
- **Mitigations:**
    - Confidence thresholding (e.g., ignore predictions with score < 0.8).
    - Post-processing debouncing (require M consecutive predictions).

## EU AI Act Compliance Notes
- **Risk Category:** **Minimal Risk** (Article 52). This system is a standard input interface modification.
- **Transparency:** This Model Card satisfies technical documentation requirements.
- **Logging:** Application should log `EIF_LOG_INFO` on gesture detection events for debugging.
- **Human Oversight:** User can always override false detections via physical buttons or alternative UI.
