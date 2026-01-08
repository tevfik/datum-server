# Machine Learning Fundamentals for Embedded Developers

A conceptual guide to ML on microcontrollers.

> **For embedded developers**: You know sensors and ADCs. This guide explains
> how to turn sensor data into classifications and predictions.

---

## Table of Contents

1. [What is ML on Embedded?](#what-is-ml-on-embedded)
2. [The ML Pipeline](#the-ml-pipeline)
3. [Features: The Real Magic](#features-the-real-magic)
4. [Classifier Comparison](#classifier-comparison)
5. [Training vs Inference](#training-vs-inference)
6. [Anomaly Detection](#anomaly-detection)
7. [Model Selection Guide](#model-selection-guide)
8. [Common Mistakes](#common-mistakes)

---

## What is ML on Embedded?

Machine Learning is **pattern recognition**:
- Input: sensor data (accelerometer, audio, temperature)
- Output: classification (walking/running), prediction (failure in 5 days)

### The Traditional Approach (Threshold-Based)

```c
// Old way: hand-coded rules
if (accel_magnitude > 15.0) {
    activity = RUNNING;
} else if (accel_magnitude > 10.0) {
    activity = WALKING;
} else {
    activity = STATIONARY;
}
```

**Problem**: Works for simple cases, breaks with real-world complexity.

### The ML Approach

```c
// ML way: learned from data
float features[10];
extract_features(accel_buffer, features);
activity = classifier_predict(model, features);
```

**Advantage**: Automatically learns complex patterns from examples.

---

## The ML Pipeline

Every embedded ML system follows this pipeline:

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Sensor    │───►│   Buffer    │───►│  Features   │───►│ Classifier  │───► Output
│   Data      │    │  (Window)   │    │ Extraction  │    │  (Model)    │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
     1 sample        128 samples        10-30 values      1 prediction
```

### Stage 1: Sensor Data
Raw readings from ADC, I2C, SPI.

### Stage 2: Windowing
Collect N samples before processing. Why?
- Statistical features need multiple samples
- Gives context (is signal rising or falling?)
- Reduces noise impact

**Window size trade-off**:
- Too small: unstable features, noisy predictions
- Too large: slow response, uses more memory

### Stage 3: Feature Extraction
Transform raw samples into **meaningful numbers**. This is where 90% of the work happens.

### Stage 4: Classification
A mathematical function that maps features to classes.

---

## Features: The Real Magic

> "Give me good features and I'll give you a good classifier."

Features are **compressed representations** of your data that capture what matters.

### Why Not Use Raw Samples?

Raw samples:
- Too many dimensions (128 samples = 128 inputs)
- Contains noise
- Sensitive to timing variations
- Hard to compare across recordings

Features:
- Low dimensions (10-30 values)
- Noise averaged out
- Timing-independent (mean doesn't care about order)
- Comparable across recordings

### Essential Feature Types

#### Statistical Features

| Feature | What It Measures | Good For |
|---------|-----------------|----------|
| **Mean** | Average value | DC level, baseline |
| **Std Dev** | Spread of values | Activity intensity |
| **Min/Max** | Extremes | Range of motion |
| **RMS** | Root mean square | Energy/power |
| **Skewness** | Asymmetry | Direction bias |
| **Kurtosis** | Tail heaviness | Spike detection |

```c
// Example: Activity recognition features
features.mean_x = mean(accel_x, n);
features.std_x = std_dev(accel_x, n);
features.magnitude_mean = mean(magnitude, n);
features.magnitude_std = std_dev(magnitude, n);
```

#### Frequency Features

| Feature | What It Measures | Good For |
|---------|-----------------|----------|
| **Peak frequency** | Dominant frequency | Gait detection, motor speed |
| **Spectral energy** | Power in freq bands | Audio classification |
| **MFCC** | Audio characteristics | Speech, keyword spotting |
| **Zero crossings** | Rate of sign changes | Roughness, vibration |

#### Domain-Specific Features

| Domain | Features |
|--------|----------|
| **Accelerometer** | Magnitude, SMA, correlation between axes |
| **Audio** | MFCC, mel spectrogram, pitch |
| **Vibration** | Crest factor, kurtosis, RMS velocity |
| **ECG** | R-R interval, heart rate variability |

### Feature Extraction in EIF

```c
#include "eif_activity.h"

// Collect samples in window
eif_accel_sample_t samples[128];
// ... fill from accelerometer

// Extract features
eif_activity_features_t features;
eif_activity_extract_features(samples, 128, &features);

// Features now contains:
// - mean_x, mean_y, mean_z
// - std_x, std_y, std_z
// - magnitude_mean, magnitude_std
// - sma, energy, zero_crossings
// - peak_frequency
```

---

## Classifier Comparison

### Decision Tree

```
        [magnitude_std > 2.0?]
            /           \
          Yes            No
          /               \
    [peak_freq > 2.5?]   STATIONARY
        /       \
      Yes       No
      /           \
   RUNNING     WALKING
```

**Pros**: Fast, interpretable, no float needed
**Cons**: Prone to overfitting, limited accuracy
**Memory**: 1-10 KB
**Speed**: < 1 µs

### Random Forest

Many decision trees vote together.

**Pros**: More robust than single tree
**Cons**: More memory and computation
**Memory**: 10-100 KB
**Speed**: 10-100 µs

### SVM (Support Vector Machine)

Finds optimal boundary between classes.

**Pros**: Good accuracy, efficient inference
**Cons**: Training is complex, kernel choice matters
**Memory**: 1-50 KB (depends on support vectors)
**Speed**: 1-50 µs

### Naive Bayes

Probabilistic classifier using Bayes theorem.

**Pros**: Very fast, works with little data
**Cons**: Assumes feature independence
**Memory**: < 1 KB
**Speed**: < 1 µs

### Neural Network

Layers of learned weights.

**Pros**: Highest accuracy potential
**Cons**: Most memory and computation
**Memory**: 10 KB - 1 MB
**Speed**: 100 µs - 10 ms

### Comparison Table

| Classifier | Accuracy | Speed | Memory | Training Data |
|------------|----------|-------|--------|---------------|
| Decision Tree | ★★★ | ★★★★★ | ★★★★ | Low |
| Random Forest | ★★★★ | ★★★★ | ★★★ | Medium |
| SVM | ★★★★ | ★★★★ | ★★★ | Medium |
| Naive Bayes | ★★★ | ★★★★★ | ★★★★★ | Low |
| Neural Net | ★★★★★ | ★★ | ★★ | High |
| Rule-Based | ★★ | ★★★★★ | ★★★★★ | None |

---

## Training vs Inference

### Training (On PC)

1. Collect labeled data: "This is walking", "This is running"
2. Extract features from data
3. Train classifier to minimize errors
4. Export model weights

```python
# Python training (not on MCU)
X_train, y_train = load_dataset()
features = extract_features(X_train)
model = RandomForestClassifier()
model.fit(features, y_train)
export_to_c(model, "model_weights.h")
```

### Inference (On MCU)

1. Read sensor data
2. Extract features
3. Run model with pre-trained weights
4. Get prediction

```c
// C inference (on MCU)
#include "model_weights.h"

float features[10];
extract_features(sensor_buffer, features);
int prediction = model_predict(features);
```

### Key Insight

**Training is expensive**, but we do it once on a PC.
**Inference is cheap**, and we do it many times on MCU.

---

## Anomaly Detection

Anomaly detection is **different** from classification:
- Classification: "Is this A, B, or C?"
- Anomaly detection: "Is this normal or weird?"

### Why Anomaly Detection for Embedded?

- Don't need labeled data for all failure modes
- Automatically adapts to "normal"
- Catches unknown problems

### Z-Score Method

```
z = (value - mean) / std_dev

If |z| > 3, it's an anomaly (3-sigma rule)
```

**In EIF**:
```c
eif_z_threshold_t detector;
eif_z_threshold_init(&detector, 0.1f, 3.0f);  // alpha, threshold

// Feed normal data
for (int i = 0; i < 100; i++) {
    eif_z_threshold_check(&detector, normal_reading);
}

// Now detect anomalies
if (eif_z_threshold_check(&detector, reading)) {
    // This reading is abnormal!
}
```

### Adaptive Thresholds

The beauty of adaptive methods:
- Learns "normal" from data automatically
- Adjusts to slow drift (aging sensors)
- No manual tuning needed (usually)

---

## Model Selection Guide

### Decision Flow

```
                    Start
                      │
                      ▼
              ┌───────────────┐
              │ < 1KB memory? │
              └───────┬───────┘
                  Yes │  No
                      │  │
                      ▼  │
              Rule-based │
                 or      │
              Naive Bayes│
                         │
                         ▼
              ┌───────────────────┐
              │ Need high accuracy?│
              └───────┬───────────┘
                  Yes │  No
                      │  │
                      ▼  ▼
           Neural Net   Decision Tree
              or          or
           Random Forest   SVM
```

### Recommendations by Application

| Application | Recommended Model | Why |
|-------------|-------------------|-----|
| Activity Recognition | Random Forest or NN | Multiple features, good separation |
| Anomaly Detection | Z-score + EMA | Simple, fast, adaptive |
| Keyword Spotting | CNN or DS-CNN | Audio patterns |
| Gesture Recognition | SVM or DTW | Small classes, clean data |
| Predictive Maintenance | Regression + threshold | Trend analysis |
| Simple Classification | Decision Tree | Minimal resources |

---

## Common Mistakes

### Mistake 1: Skipping Feature Engineering

```c
// WRONG: Using raw samples
float input[128];
memcpy(input, accel_buffer, sizeof(accel_buffer));
prediction = neural_network(input);  // 128 inputs, huge network!

// RIGHT: Extract features first
float features[12];
extract_features(accel_buffer, features);
prediction = neural_network(features);  // 12 inputs, tiny network
```

### Mistake 2: Training on Different Features Than Inference

```python
# Python training
features = [mean, std, rms, max_value]  # 4 features
model.fit(features, labels)
```

```c
// C inference - MISMATCH!
float features[3] = {mean, std, rms};  // Only 3 features!
predict(features);  // WRONG!
```

### Mistake 3: Not Normalizing Features

```c
// WRONG: Features with different scales
features[0] = temperature;  // Range: 20-40
features[1] = acceleration; // Range: 0-20
features[2] = voltage;      // Range: 3.0-3.6

// RIGHT: Normalize to similar range
features[0] = (temperature - 20) / 20;  // Range: 0-1
features[1] = acceleration / 20;        // Range: 0-1
features[2] = (voltage - 3.0) / 0.6;    // Range: 0-1
```

### Mistake 4: Testing on Training Data

Your model will look great on training data but fail on new data.

**Always test on data the model has never seen.**

### Mistake 5: Ignoring Class Imbalance

If you have 90% "normal" and 10% "fault" samples:
- A dumb model that always says "normal" is 90% accurate!
- But it never detects faults.

**Balance your training data or use weighted metrics.**

---

## EIF ML Cheat Sheet

### Quick Function Reference

```
Sensor smoothing      → eif_ema_update()
Anomaly detection     → eif_z_threshold_check()
Activity recognition  → eif_activity_classify_rules()
Feature extraction    → eif_activity_extract_features()
Health monitoring     → eif_health_update()
RUL estimation        → eif_rul_estimate()
```

### Typical Memory Requirements

| Component | Memory |
|-----------|--------|
| Z-threshold detector | 48 B |
| Activity window (64) | 800 B |
| Activity features | 60 B |
| Small decision tree | 1-5 KB |
| Simple neural net | 10-50 KB |

---

## Next Steps

1. **Try the demos**: `./bin/activity_recognition_demo --batch`
2. **Read**: [DL_FUNDAMENTALS.md](DL_FUNDAMENTALS.md) for neural networks
3. **Practice**: Collect your own sensor data, extract features
4. **Experiment**: Compare rule-based vs learned classifiers
