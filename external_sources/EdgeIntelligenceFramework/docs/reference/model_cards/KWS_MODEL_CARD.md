# Model Card: Keyword Spotting (`kws_model.eif`)

## Model Details
- **Model Date:** 2025-12-17
- **Version:** 1.0.0
- **Model Type:** Deep Feed-Forward Neural Network (DNN)
- **Input Format:** 637 inputs (Flattened feature vector).
    - Source: Audio MFCC (Mel-Frequency Cepstral Coefficients)
    - Structure: 13 MFCC coefficients × 49 time frames (approx 1 second audio)
- **Output:** Probability distribution over 5 classes via Softmax.
- **License:** MIT
- **Author/Owner:** Edge Intelligence Framework (EIF) Team

## Intended Use
- **Primary Use Case:** Voice command activation ("Wake Word") for smart edge devices.
- **Supported Keywords:**
    1. Silence (Background noise)
    2. Unknown (Speech but not a keyword)
    3. "Yes"
    4. "No"
    5. "Hey"
- **Target Platform:** Cortex-M4 (e.g. Arduino Nano 33 BLE Sense), ESP32-S3.
- **Out of Scope:** Continuous speech recognition, large vocabulary dictation, or speaker identification.

## Training Data
- **Dataset:** [Google Speech Commands Dataset v2](https://arxiv.org/abs/1804.03209)
- **Preprocessing:**
    - Audio resampling to 16kHz.
    - MFCC extraction: Window size 40ms, stride 20ms.
    - Normalization of coefficients.

## Performance (Estimated)
- **Accuracy:** ~90-95% (on target keywords under quiet conditions).
- **Latency:**
    - **Cortex-M4F (64MHz):** ~1.5 ms
    - **ESP32-S3:** < 0.5 ms
- **Memory Footprint:**
    - **Weights (Flash):** ~172 KB (43,077 parameters)
        - Architecture: 637 -> 64 -> 32 -> 5
    - **Activations (RAM):** ~2.5 KB
- **Robustness:**
    - Noise: Performance degrades in high-noise environments (>60dB SNR).
    - Accents: May struggle with strong accents not represented in the Google Speech Commands dataset.

## Ethical Considerations & Limitations
- **Bias:** The dataset is predominantly English speakers. Performance may be lower for non-native speakers or specific accents.
- **Privacy:** Audio is processed **entirely on-device**. No audio data is recorded or transmitted to the cloud, ensuring user privacy by design.
- **Risks:** False activation could be annoying (device waking up randomly).
- **Mitigations:**
    - "Silence" class specifically trained to reject background noise.
    - "Unknown" class trained on random words to reduce false positives.

## EU AI Act Compliance Notes
- **Risk Category:** **Minimal Risk**. Voice assistants operating locally are standard consumer features.
- **Record Keeping:**
    - System does *not* store raw audio (privacy preservation).
    - It logs only metadata: `EIF_LOG_INFO("Keyword detected: 'Hey', conf: 0.98")`.
- **Transparency:** The use of a standard open dataset allows for bias auditing.
