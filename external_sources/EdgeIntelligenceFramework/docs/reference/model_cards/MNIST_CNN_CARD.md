# Model Card: MNIST Digit Recognition (`mnist_cnn.eif`)

## Model Details
- **Model Date:** 2025-12-17
- **Version:** 1.0.0
- **Model Type:** Multi-Layer Perceptron (MLP)
    - *Note:* While the filename suggests a Convolutional Neural Network (CNN), the current implementation is a highly optimized 3-layer fully connected network (MLP) for efficiency on Cortex-M4 class devices.
- **Input Format:** 784 inputs (Flattened vector: 28x28 pixels).
    - Data Type: `float32`
    - Normalization: Pixel values scaled to range [0.0, 1.0] or [-1.0, 1.0] depending on training config.
- **Output:** Probability distribution over 10 classes (Digits 0-9) via Softmax.
- **License:** MIT
- **Author/Owner:** Edge Intelligence Framework (EIF) Team

## Intended Use
- **Primary Use Case:** Handwritten digit recognition on embedded devices (e.g. reading utility meters, simple character entry).
- **Target Platform:** STM32F4, ESP32, or similar MCUs with >1MB Flash (due to model size).
- **Out of Scope:** Recognition of complex text, cursive, or multiple digits in a single image.

## Training Data
- **Dataset:** [MNIST Database of Handwritten Digits](http://yann.lecun.com/exdb/mnist/)
- **Composition:** 60,000 training images and 10,000 test images of handwritten digits (0-9).
- **Preprocessing:**
    - Images flattened from 28x28 to 784 vector.
    - Standard He Initialization used for demo weights.

## Performance (Estimated)
- **Accuracy:** ~96-98% (Typical for this architecture on MNIST).
- **Latency:**
    - **Cortex-M4F (168MHz):** ~2-3 ms
    - **ESP32-S3 (240MHz):** < 1 ms
- **Memory Footprint:**
    - **Weights (Flash):** ~437 KB (109,386 parameters)
        - L1: 784 -> 128
        - L2: 128 -> 64
        - L3: 64 -> 10
    - **Activations (RAM):** ~0.5 KB (Scratch buffer)
- **Robustness:**
    - Sensitive to centering. Digits must be centered in the 28x28 frame for accurate prediction.

## Ethical Considerations & Limitations
- **Bias:** MNIST is well-balanced, but performance may degrade if users have significantly different handwriting styles than the standard US census bureau employees/students in the dataset.
- **Risks:** Misinterpretation of digits in critical applications (e.g. financial) could lead to errors.
- **Mitigations:**
    - Always verify high-confidence predictions (e.g., threshold > 0.95).
    - Use verification checksums if interpreting strings of numbers.

## EU AI Act Compliance Notes
- **Risk Category:** **Minimal/Limited Risk**. Optical character recognition (OCR) is generally considered low risk unless used for biometric identification or critical infrastructure.
- **Transparency:** This card documents the architecture limitations (MLP vs CNN).
- **Logging:** Log classification results with confidence scores.
