# Hardware Demos - ESP32 Ready Projects

Complete ESP32 demos with deployment scripts.

## Available Demos

| Demo | Hardware | Description |
|------|----------|-------------|
| [esp32_cam_faces](esp32_cam_faces/) | ESP32-CAM | Face-like detection |
| [esp32_voice_cmd](esp32_voice_cmd/) | ESP32 + I2S mic | Voice commands |
| [smart_sensor](smart_sensor/) | ESP32-mini | Multi-sensor hub |
| [motion_detect](motion_detect/) | ESP32-CAM | Motion & counting |

## Quick Start

### Desktop Simulation

```bash
# Build all
cd ../../build
make esp32_cam_faces_demo esp32_voice_cmd_demo smart_sensor_demo motion_detect_demo

# Run
./bin/esp32_cam_faces_demo
./bin/esp32_voice_cmd_demo
./bin/smart_sensor_demo
./bin/motion_detect_demo
```

### ESP32 Deployment

```bash
# Setup ESP-IDF
source ~/esp-idf/export.sh

# Choose demo
cd esp32_cam_faces

# Build and flash
./build_flash.sh /dev/ttyUSB0
```

## Files Per Demo

```
demo_name/
├── main.c           # Main source (works on desktop & ESP32)
├── README.md        # Quick overview
├── DEPLOY.md        # ESP32 deployment guide
├── CMakeLists.txt   # Desktop build
├── build_flash.sh   # ESP32 build+flash script
└── esp32/           # ESP-IDF project files
    └── CMakeLists.txt
```

## Hardware Summary

### ESP32-CAM Projects
- Face detection, motion detection
- Camera: OV2640 (2MP)
- Resolution: QVGA (320×240) recommended

### ESP32 + Microphone Projects
- Voice commands, keyword spotting
- Mic: INMP441 (I2S) or MAX4466 (analog)
- Sample rate: 16kHz

### ESP32 + Sensors Projects
- Smart sensor hub, plant monitor
- Sensors: DHT22, BH1750, soil moisture
