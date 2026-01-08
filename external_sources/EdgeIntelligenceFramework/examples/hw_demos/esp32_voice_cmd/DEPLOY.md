# ESP32 Voice Command - Deployment Guide

## Hardware Requirements

- ESP32, ESP32-S3, or ESP32-mini
- INMP441 I2S MEMS Microphone
- Jumper wires

## Wiring (INMP441)

| INMP441 | ESP32 |
|---------|-------|
| VDD | 3.3V |
| GND | GND |
| WS (LRCLK) | GPIO25 |
| SCK (BCLK) | GPIO26 |
| SD (Data) | GPIO22 |
| L/R | GND (left) or 3.3V (right) |

## Alternative: MAX4466 Analog Microphone

| MAX4466 | ESP32 |
|---------|-------|
| VCC | 3.3V |
| GND | GND |
| OUT | GPIO34 (ADC1) |

## Setup

### 1. ESP-IDF Setup

```bash
source ~/esp-idf/export.sh
```

### 2. Create ESP-IDF Project

```bash
cd examples/hw_demos/esp32_voice_cmd
mkdir -p esp32/main
cp main.c esp32/main/
```

### 3. main/CMakeLists.txt

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc
)
```

### 4. I2S Configuration

```c
#include "driver/i2s.h"

i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_RX,
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
};

i2s_pin_config_t pin_config = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_in_num = 22,
    .data_out_num = -1
};

i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
i2s_set_pin(I2S_NUM_0, &pin_config);
```

### 5. Build & Flash

```bash
./build_flash.sh /dev/ttyUSB0
```

## Keywords

Default keywords: "yes", "no", "on", "off"

To add custom keywords, train a model using Edge Impulse or TensorFlow Lite Micro.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No audio | Check WS/SCK/SD connections |
| Noise | Add 100nF capacitor on VDD |
| Low sensitivity | Check L/R pin connection |
