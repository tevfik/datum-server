# Gesture Recognition Tutorial: IMU-Based Activity Detection

## Learning Objectives

- IMU data preprocessing
- Feature extraction from accelerometer/gyroscope
- Real-time activity classification
- DTW pattern matching

**Level**: Beginner to Intermediate  
**Time**: 40 minutes

---

## 1. IMU Gestures

### Detectable Gestures

| Gesture | Accelerometer Pattern | Gyroscope Pattern |
|---------|----------------------|-------------------|
| Shake | High-frequency oscillation | Low |
| Flip | Z-axis reversal | High Y rotation |
| Tap | Sharp spike + decay | Minimal |
| Circle | Smooth sinusoidal X/Y | High Z rotation |
| Swipe | Single axis peak | Low |

---

## 2. Feature Extraction

### Time-Domain Features

```c
typedef struct {
    float mean[3];        // Mean acceleration
    float std[3];         // Standard deviation
    float max[3];         // Peak values
    float energy;         // Sum of squares
    float zcr;            // Zero-crossing rate
} imu_features_t;

void extract_features(const float* accel, int n, imu_features_t* feat) {
    // Mean
    for (int i = 0; i < n; i++) {
        feat->mean[0] += accel[i*3 + 0];
        feat->mean[1] += accel[i*3 + 1];
        feat->mean[2] += accel[i*3 + 2];
    }
    for (int j = 0; j < 3; j++) feat->mean[j] /= n;
    
    // Energy
    for (int i = 0; i < n * 3; i++) {
        feat->energy += accel[i] * accel[i];
    }
    
    // ... more features
}
```

### Frequency-Domain Features

```c
// Apply FFT to each axis
eif_dsp_rfft(accel_x, spectrum, window_size);

// Dominant frequency
int peak_bin = find_peak(spectrum, window_size/2);
float dom_freq = peak_bin * sample_rate / window_size;
```

---

## 3. Classification

### Simple Threshold Classifier

```c
int classify_gesture(const imu_features_t* f) {
    if (f->energy > 100.0f && f->zcr > 10) {
        return GESTURE_SHAKE;
    }
    if (fabsf(f->mean[2] - 9.8f) > 5.0f) {
        return GESTURE_FLIP;
    }
    if (f->max[0] > 15.0f || f->max[1] > 15.0f) {
        return GESTURE_TAP;
    }
    return GESTURE_NONE;
}
```

### Neural Network Classifier

```c
// Input: 15 features
// Output: 5 gesture classes
eif_nn_model_invoke(&model, features, output);
int gesture = argmax(output, 5);
```

---

## 4. ESP32 Implementation

```c
#include "driver/i2c.h"
#include "mpu6050.h"

void gesture_task(void* arg) {
    float window[WINDOW_SIZE * 6];  // 3 accel + 3 gyro
    int idx = 0;
    
    while (1) {
        // Read IMU at 100Hz
        float accel[3], gyro[3];
        mpu6050_read(&accel, &gyro);
        
        // Add to sliding window
        memcpy(&window[idx * 6], accel, 12);
        memcpy(&window[idx * 6 + 3], gyro, 12);
        idx = (idx + 1) % WINDOW_SIZE;
        
        // Classify every 500ms
        static int counter = 0;
        if (++counter >= 50) {
            imu_features_t feat;
            extract_features(window, WINDOW_SIZE, &feat);
            int gesture = classify_gesture(&feat);
            
            if (gesture != GESTURE_NONE) {
                printf("Gesture: %s\n", gesture_names[gesture]);
            }
            counter = 0;
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
```

---

## 5. Summary

### Key Features
- Sliding window (50-100 samples)
- Time + frequency features
- Threshold or NN classifier
- ~10ms inference on ESP32
