# Activity Recognition Demo

Demonstrates human activity recognition using accelerometer data.

## Features

- **Feature Extraction** - Calculate statistical features from IMU data
- **Rule-Based Classification** - Classify activities without ML model
- **Streaming Detection** - Real-time activity monitoring

## Supported Activities

| Activity | Detection Method |
|----------|------------------|
| Stationary | Low magnitude variance |
| Walking | Medium variance, ~2 Hz peak |
| Running | High variance, ~3 Hz peak |
| Cycling | Sustained medium motion |
| Stairs | Vertical acceleration pattern |

## Running

```bash
./build/bin/activity_recognition_demo --batch
```

## Sample Output

```
╔══════════════════════════════════════╗
║  EIF Activity Recognition Demo       ║
╚══════════════════════════════════════╝

--- Feature Extraction ---
Feature              Value
---------            -----
Mean X:              0.05 m/s²
Mean Y:              0.10 m/s²
Mean Z:              9.81 m/s²
Std X:               0.32 m/s²
Magnitude Mean:      9.85 m/s²
Magnitude Std:       0.58 m/s²

Detected Activity: WALKING

--- Streaming Classification ---
Window 1: STATIONARY
Window 2: WALKING
Window 3: WALKING
Window 4: RUNNING
Window 5: RUNNING
```

## API Reference

```c
// Sliding window
eif_activity_window_init(&window, hop_size);
bool ready = eif_activity_window_add(&window, ax, ay, az);

// Feature extraction
eif_activity_extract_features(samples, n, &features);

// Classification
eif_activity_t activity = eif_activity_classify_rules(&features);
```

## Arduino Integration

See `ARDUINO_TUTORIAL.md` for complete Arduino examples.
