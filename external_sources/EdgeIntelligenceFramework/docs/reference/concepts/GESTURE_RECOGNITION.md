# Gesture Recognition on MCUs

A complete guide to building gesture recognition systems on microcontrollers.

> **Why Gesture Recognition?** It's one of the most interactive ML applications - 
> users can literally wave at their devices! Perfect for wearables, game controllers,
> and smart home devices.

---

## Table of Contents

1. [What is Gesture Recognition?](#what-is-gesture-recognition)
2. [Sensing Gestures](#sensing-gestures)
3. [Template Matching with DTW](#template-matching-with-dtw)
4. [Feature-Based Classification](#feature-based-classification)
5. [Building a Gesture Library](#building-a-gesture-library)
6. [Memory and Performance](#memory-and-performance)
7. [Common Gestures](#common-gestures)
8. [Troubleshooting](#troubleshooting)

---

## What is Gesture Recognition?

Gesture recognition converts motion patterns into discrete commands:

```
[Accelerometer Data] → [Feature Extraction] → [Classification] → "Shake!"
     ↓                       ↓                      ↓
  Raw X,Y,Z           Magnitude, peaks        Match to library
```

### Two Approaches

| Approach | How It Works | Best For |
|----------|--------------|----------|
| **Template Matching** | Compare gesture to stored templates | Few gestures, personalized |
| **Feature Classification** | Extract features, use ML classifier | Many gestures, general use |

---

## Sensing Gestures

### Accelerometer Basics

```
         Z ↑
           |    
           |   Gravity = 9.81 m/s² (1g)
           |   
    Y ←────●────→ X
           |
           |
```

When you move the device, acceleration changes:

| Motion | X | Y | Z |
|--------|---|---|---|
| Stationary (flat) | 0 | 0 | +9.81 |
| Shake left | -20 | 0 | +9.81 |
| Shake right | +20 | 0 | +9.81 |
| Lift up | 0 | 0 | +15 |
| Tap | spike | spike | spike |

### Magnitude (Motion Intensity)

```c
float magnitude = sqrtf(ax*ax + ay*ay + az*az);

// At rest: magnitude ≈ 9.81 (just gravity)
// During shake: magnitude > 15 (gravity + motion)
```

---

## Template Matching with DTW

### Dynamic Time Warping (DTW)

DTW measures similarity between two time series that may be **stretched or compressed**.

**Why not just compare point-by-point?**

```
Template:    ─╱╲──────
Gesture:     ──╱╲─────    (slightly delayed)
              ↑ offset causes mismatch!
```

DTW allows warping:

```
Template:    ─╱╲──────
              │╲
              │ ╲ warp
              │  ╲
Gesture:     ──╱╲─────    ✓ matches!
```

### DTW Algorithm

```c
// Cost matrix: compare every point in template to every point in gesture
for (int i = 0; i < template_len; i++) {
    for (int j = 0; j < gesture_len; j++) {
        float cost = fabsf(template[i] - gesture[j]);
        
        // Minimum of three paths: horizontal, vertical, diagonal
        dtw[i][j] = cost + min3(
            dtw[i-1][j],     // insertion
            dtw[i][j-1],     // deletion  
            dtw[i-1][j-1]    // match
        );
    }
}

float distance = dtw[template_len-1][gesture_len-1];
```

### Memory-Efficient DTW

Full DTW needs `O(n*m)` memory. For MCUs, use **FastDTW** with constraints:

```c
// Sakoe-Chiba band: only compare within window
#define DTW_WINDOW 10

for (int i = 0; i < template_len; i++) {
    int j_start = max(0, i - DTW_WINDOW);
    int j_end = min(gesture_len, i + DTW_WINDOW);
    
    for (int j = j_start; j < j_end; j++) {
        // ... DTW calculation
    }
}
// Memory: O(n * 2*window) instead of O(n*m)
```

### In EIF

```c
#include "eif_gesture.h"

// Store template
float tap_template[32]; // Recorded tap gesture
int tap_len = 32;

// Compare new gesture
float gesture[40];
float distance = eif_dtw_distance(tap_template, tap_len, 
                                   gesture, 40);

if (distance < THRESHOLD) {
    printf("Detected: TAP!\n");
}
```

---

## Feature-Based Classification

Instead of comparing raw signals, extract **features** and classify.

### Key Features for Gestures

| Feature | What It Measures | Good For |
|---------|------------------|----------|
| **Duration** | How long the gesture lasts | Short tap vs long press |
| **Peak magnitude** | Maximum acceleration | Intensity |
| **Peak count** | Number of peaks | Single vs double tap |
| **Zero crossings** | Direction changes | Shake detection |
| **Energy** | Total motion intensity | Active vs passive |
| **Peak frequency** | Dominant vibration | Rhythmic gestures |

### Feature Extraction

```c
typedef struct {
    float duration_ms;
    float peak_magnitude;
    int peak_count;
    int zero_crossings;
    float energy;
    float peak_frequency;
} gesture_features_t;

void extract_gesture_features(float* mag, int n, float sample_rate,
                               gesture_features_t* f) {
    f->duration_ms = n * 1000.0f / sample_rate;
    
    // Find peaks
    f->peak_magnitude = 0;
    f->peak_count = 0;
    float threshold = 12.0f;  // Above 1.2g
    
    for (int i = 1; i < n-1; i++) {
        if (mag[i] > mag[i-1] && mag[i] > mag[i+1] && mag[i] > threshold) {
            f->peak_count++;
            if (mag[i] > f->peak_magnitude) {
                f->peak_magnitude = mag[i];
            }
        }
    }
    
    // Energy
    f->energy = 0;
    for (int i = 0; i < n; i++) {
        f->energy += mag[i] * mag[i];
    }
    f->energy /= n;
    
    // Zero crossings (around mean)
    float mean = 9.81f;
    f->zero_crossings = 0;
    for (int i = 1; i < n; i++) {
        if ((mag[i] - mean) * (mag[i-1] - mean) < 0) {
            f->zero_crossings++;
        }
    }
}
```

### Rule-Based Classification

```c
typedef enum {
    GESTURE_NONE,
    GESTURE_TAP,
    GESTURE_DOUBLE_TAP,
    GESTURE_SHAKE,
    GESTURE_FLIP,
    GESTURE_ROTATE
} gesture_t;

gesture_t classify_gesture(gesture_features_t* f) {
    // Shake: many zero crossings, moderate peaks
    if (f->zero_crossings > 8 && f->peak_magnitude > 15.0f) {
        return GESTURE_SHAKE;
    }
    
    // Double tap: exactly 2 peaks, short duration
    if (f->peak_count == 2 && f->duration_ms < 500) {
        return GESTURE_DOUBLE_TAP;
    }
    
    // Single tap: 1 peak, very short
    if (f->peak_count == 1 && f->duration_ms < 200) {
        return GESTURE_TAP;
    }
    
    // High energy, few peaks = flip
    if (f->energy > 200 && f->peak_count <= 2) {
        return GESTURE_FLIP;
    }
    
    return GESTURE_NONE;
}
```

---

## Building a Gesture Library

### Recording Templates

```c
// Recording mode
#define MAX_GESTURE_LEN 100
float recording[MAX_GESTURE_LEN];
int rec_idx = 0;
bool recording_active = false;

void gesture_record_sample(float mag) {
    if (recording_active && rec_idx < MAX_GESTURE_LEN) {
        recording[rec_idx++] = mag;
    }
}

void gesture_start_recording(void) {
    rec_idx = 0;
    recording_active = true;
    printf("Recording... perform gesture now\n");
}

void gesture_stop_recording(void) {
    recording_active = false;
    printf("Recorded %d samples\n", rec_idx);
    // Save to flash/EEPROM
}
```

### Gesture State Machine

```
     idle ──[motion start]──▶ detecting
       ▲                         │
       │                    [timeout]
       │                         │
       └───[gesture matched]─────▼
                             recognizing
```

```c
typedef enum {
    STATE_IDLE,
    STATE_DETECTING,
    STATE_RECOGNIZING
} gesture_state_t;

gesture_state_t state = STATE_IDLE;
int idle_counter = 0;
float gesture_buffer[100];
int buf_idx = 0;

void gesture_process(float ax, float ay, float az) {
    float mag = sqrtf(ax*ax + ay*ay + az*az);
    
    switch (state) {
    case STATE_IDLE:
        if (mag > 11.0f) {  // Motion threshold (> 1.1g)
            state = STATE_DETECTING;
            buf_idx = 0;
        }
        break;
        
    case STATE_DETECTING:
        gesture_buffer[buf_idx++] = mag;
        
        if (mag < 10.5f) {
            idle_counter++;
            if (idle_counter > 10) {  // Motion ended
                state = STATE_RECOGNIZING;
            }
        } else {
            idle_counter = 0;
        }
        
        if (buf_idx >= 100) {  // Buffer full
            state = STATE_RECOGNIZING;
        }
        break;
        
    case STATE_RECOGNIZING:
        recognize_gesture(gesture_buffer, buf_idx);
        state = STATE_IDLE;
        break;
    }
}
```

---

## Memory and Performance

### Memory Budget

| Component | RAM | Notes |
|-----------|-----|-------|
| Gesture buffer | 400 B | 100 samples × 4 bytes |
| Template (one) | 200 B | 50 samples |
| DTW matrix row | 400 B | 100 × 4 (only 2 rows needed) |
| Features | 24 B | 6 floats |
| **Total** | ~1 KB | For basic system |

### Performance

| Operation | Cortex-M0 @ 16MHz | Cortex-M4 @ 80MHz |
|-----------|-------------------|-------------------|
| Feature extraction (100 samples) | ~5 ms | ~0.5 ms |
| DTW (50 vs 100 samples) | ~20 ms | ~2 ms |
| Classification | < 1 ms | < 0.1 ms |

### Optimization Tips

1. **Use fixed-point** for memory-constrained devices
2. **Limit gesture length** to 100 samples max
3. **Use Sakoe-Chiba band** for DTW (window = 10-20)
4. **Precompute template features** instead of storing raw

---

## Common Gestures

### Gesture Signatures

```
TAP:           ___╱╲___
               Quick spike, immediate return

DOUBLE TAP:    ___╱╲_╱╲___
               Two spikes, 100-300ms apart

SHAKE:         ╱╲╱╲╱╲╱╲
               Multiple oscillations, sustained

FLIP:          ___╱‾‾‾╲___
               Slow rise, plateau, slow fall

PICK UP:       ___╱‾‾‾‾‾‾‾
               Rise and stay elevated

PUT DOWN:      ‾‾‾‾‾‾╲___
               Fall and stay low
```

### Detection Parameters

```c
typedef struct {
    const char* name;
    float min_peak;       // Minimum peak magnitude
    float max_peak;       // Maximum peak magnitude
    int min_peaks;        // Minimum number of peaks
    int max_peaks;        // Maximum number of peaks
    int min_duration_ms;  // Minimum gesture duration
    int max_duration_ms;  // Maximum gesture duration
    int min_zero_cross;   // Minimum zero crossings
} gesture_profile_t;

const gesture_profile_t GESTURE_PROFILES[] = {
    // name          minP  maxP  minN maxN minD maxD minZ
    {"tap",          12,   30,   1,   1,   50,  200, 0},
    {"double_tap",   12,   30,   2,   2,   100, 500, 2},
    {"shake",        15,   40,   4,   20,  300, 2000, 8},
    {"flip",         18,   50,   1,   3,   300, 1000, 2},
};
```

---

## Troubleshooting

### Problem: False Positives

**Symptom**: Detecting gestures when user didn't intend to

**Solutions**:
1. Increase motion threshold
2. Add minimum duration requirement
3. Require confirmation (e.g., double-tap to confirm)
4. Add "anti-gesture" patterns that reject noise

### Problem: Missed Gestures

**Symptom**: User performs gesture but not detected

**Solutions**:
1. Lower detection threshold
2. Increase gesture buffer size
3. Train templates with actual user gestures
4. Add variations of same gesture

### Problem: Confused Gestures

**Symptom**: Tap recognized as shake, etc.

**Solutions**:
1. Add more discriminative features (duration, peak count)
2. Increase DTW threshold between gesture classes
3. Use hierarchical classification (fast/slow first, then specific)

### Debug Visualization

```c
void debug_print_gesture(float* mag, int len) {
    printf("Gesture: ");
    for (int i = 0; i < len; i += len/40) {
        int bar = (int)((mag[i] - 8) * 2);
        if (bar < 0) bar = 0;
        if (bar > 20) bar = 20;
        for (int j = 0; j < bar; j++) printf("█");
        printf("\n         ");
    }
    printf("\n");
}
```

---

## EIF Gesture API Quick Reference

```c
#include "eif_gesture.h"

// Initialize
eif_gesture_t gesture;
eif_gesture_init(&gesture, 50.0f);  // 50 Hz sample rate

// Process samples (call at sample rate)
eif_gesture_result_t result = eif_gesture_process(&gesture, ax, ay, az);

if (result.detected) {
    printf("Gesture: %s (confidence: %.1f%%)\n", 
           result.name, result.confidence * 100);
}

// Record custom template
eif_gesture_start_recording(&gesture, "my_gesture");
// ... user performs gesture ...
eif_gesture_stop_recording(&gesture);

// Get template for saving
float* template = eif_gesture_get_template(&gesture, "my_gesture");
```

---

## Next Steps

1. **Try the demo**: `./bin/gesture_demo`
2. **Arduino example**: See `examples/GestureController`
3. **Add custom gestures**: Record your own templates
4. **Optimize**: Try fixed-point for smaller MCUs
