# Motion Detection - Deployment Guide

## Hardware

- ESP32-CAM or ESP32-S3-CAM
- Optional: PIR sensor (HC-SR501)
- Optional: SD Card for recording

## Camera Pins (AI-Thinker)

Same as esp32_cam_faces - see that DEPLOY.md

## PIR Sensor (Optional)

| PIR | ESP32-CAM |
|-----|-----------|
| VCC | 5V |
| GND | GND |
| OUT | GPIO13 |

## Features

- Background subtraction
- Motion blob detection
- Entry/exit counting
- Configurable sensitivity

## Configuration

```c
#define MOTION_THRESHOLD 30    // Pixel difference
#define MIN_BLOB_SIZE 20       // Minimum motion area
#define COOLDOWN_FRAMES 10     // Ignore after detection
```

## Output Options

1. **Serial** - Print to console
2. **MQTT** - Send to broker
3. **HTTP** - POST to webhook
4. **SD Card** - Save clips

## Build & Flash

```bash
source ~/esp-idf/export.sh
./build_flash.sh /dev/ttyUSB0
```

## Web Stream (Optional)

Add camera web server for live view:
```c
#include "esp_http_server.h"
// See ESP-IDF camera example
```
