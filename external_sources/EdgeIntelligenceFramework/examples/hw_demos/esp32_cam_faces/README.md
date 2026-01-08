# ESP32-CAM Face Detection

Simple face detection demo for ESP32-CAM boards.

## Hardware

- ESP32-CAM (AI-Thinker)
- ESP32-S3-CAM  
- OV2640 camera module

## Features

- Real-time face-like pattern detection
- Cascade filter approach
- Desktop simulation for testing
- LED flash on detection

## Desktop Build

```bash
# From main build directory
make esp32_cam_faces_demo
./build/bin/esp32_cam_faces_demo
```

## ESP32 Build

```bash
# With ESP-IDF
cd examples/hw_demos/esp32_cam_faces
idf.py build flash monitor
```

## Algorithm

Uses simplified Haar-like features:
1. Sliding window across frame
2. Compare center vs edge brightness
3. Face-like patterns have brighter center
4. Apply confidence threshold

## Memory

- Image buffer: 75 KB (320×240)
- Detection: 2 KB
- Total: ~80 KB RAM
