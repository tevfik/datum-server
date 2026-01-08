# Gesture Recognition Demo

An interactive demo showcasing gesture recognition using accelerometer data.

## Features

- 🎮 **7 Gesture Types**: Tap, double-tap, shake, swipe (left/right/up/down)
- 📊 **ASCII Waveform Visualization**: See the accelerometer patterns
- 🎯 **Real-time Classification**: DTW-based gesture matching
- 🔬 **Educational**: Shows how gesture detection works

## Quick Start

```bash
# Build
cd build && make gesture_demo

# Run in batch mode
./bin/gesture_demo --batch

# Interactive mode
./bin/gesture_demo --interactive
```

## How It Works

1. **Simulate Gesture** → Generate accelerometer data pattern
2. **Detect Motion** → Find start/end of gesture
3. **Extract Features** → Analyze motion characteristics
4. **Classify** → Match to known gesture types

## Gesture Patterns

```
TAP:         ___╱╲___     Single spike
DOUBLE TAP:  ___╱╲_╱╲___  Two spikes
SHAKE:       ╱╲╱╲╱╲╱╲     Oscillations
SWIPE LEFT:  ←←←←         Negative X impulse
SWIPE RIGHT: →→→→         Positive X impulse
```

## Arduino

See `GestureController.ino` for real-hardware implementation.

## API

```c
#include "eif_gesture.h"

eif_simple_gesture_t detector;
eif_simple_gesture_init(&detector, 12.0f);  // 1.2g threshold

// In loop
eif_gesture_type_t result = eif_simple_gesture_update(&detector, ax, ay, az);
if (result != EIF_GESTURE_NONE) {
    printf("Gesture: %s\n", eif_gesture_names[result]);
}
```
