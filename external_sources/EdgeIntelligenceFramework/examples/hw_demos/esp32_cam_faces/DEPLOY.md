# ESP32-CAM Face Detection - Deployment Guide

## Hardware Requirements

- ESP32-CAM (AI-Thinker) or ESP32-S3-CAM
- USB-TTL programmer (FTDI, CP2102)
- Micro USB cable

## Wiring (for programming)

| ESP32-CAM | USB-TTL |
|-----------|---------|
| 5V | 5V |
| GND | GND |
| U0R | TX |
| U0T | RX |
| IO0 | GND (during flash) |

## Setup

### 1. Install ESP-IDF

```bash
# Clone ESP-IDF v5.1
git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git ~/esp-idf
cd ~/esp-idf
./install.sh esp32

# Activate
source ~/esp-idf/export.sh
```

### 2. Create ESP-IDF Project

```bash
cd examples/hw_demos/esp32_cam_faces
mkdir -p esp32/main
cp main.c esp32/main/
cp esp32/CMakeLists.txt esp32/
```

### 3. Create main/CMakeLists.txt

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_camera driver
)
```

### 4. Build & Flash

```bash
cd esp32
idf.py set-target esp32  # or esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Camera Pin Configuration (AI-Thinker)

```c
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Flash fails | Connect IO0 to GND, press RST |
| No camera | Check OV2640 ribbon cable |
| Brown-out | Use external 5V supply |
