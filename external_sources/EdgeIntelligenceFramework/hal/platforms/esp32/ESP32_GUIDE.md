# ESP32 Deployment Guide

This guide covers deploying EIF on ESP32 family devices.

## Supported Boards

| Board | CPU | RAM | Flash | Camera | Best For |
|-------|-----|-----|-------|--------|----------|
| **ESP32-CAM** | 240MHz | 520KB | 4MB | OV2640 | CV demos |
| **ESP32-S3-CAM** | 240MHz | 512KB | 8MB | OV2640 | CV + AI |
| **ESP32-MINI** | 240MHz | 520KB | 4MB | No | DSP, filters |

## Setup

### 1. Install ESP-IDF

```bash
# Install ESP-IDF v5.x
git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32,esp32s3
source export.sh
```

### 2. Create Project

```bash
idf.py create-project eif_demo
cd eif_demo
```

### 3. Add EIF as Component

```bash
mkdir -p components/eif
# Copy EIF source files
cp -r path/to/eif/core components/eif/
cp -r path/to/eif/dsp components/eif/
cp -r path/to/eif/bf components/eif/
cp -r path/to/eif/cv components/eif/
```

### 4. CMakeLists.txt

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES eif
)
```

## Memory Configuration

Edit `sdkconfig`:
```
CONFIG_ESP32_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096
```

## Example: KWS on ESP32

```c
#include "eif_dsp.h"
#include "driver/i2s.h"

// I2S config for INMP441 microphone
i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_RX,
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
};

void app_main(void) {
    // Init I2S
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    
    // Main loop
    while (1) {
        // Read audio
        i2s_read(I2S_NUM_0, audio_buf, sizeof(audio_buf), &bytes_read, portMAX_DELAY);
        
        // Extract MFCC
        eif_dsp_mfcc_compute(audio_buf, 400, 16000, 13, mfcc);
        
        // Classify
        int cmd = classify(mfcc);
        ESP_LOGI("KWS", "Command: %d", cmd);
    }
}
```

## Example: Edge Detection on ESP32-CAM

```c
#include "eif_cv.h"
#include "esp_camera.h"

void app_main(void) {
    // Init camera
    camera_config_t config = CAMERA_CONFIG_DEFAULT();
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    esp_camera_init(&config);
    
    // Allocate edge buffer
    eif_image_t edges;
    eif_image_create(&edges, 320, 240, EIF_IMAGE_GRAY);
    
    while (1) {
        camera_fb_t* fb = esp_camera_fb_get();
        
        // Create image wrapper
        eif_image_t img = {
            .data = fb->buf,
            .width = fb->width,
            .height = fb->height,
            .format = EIF_IMAGE_GRAY
        };
        
        // Canny edge detection
        eif_cv_canny(&img, &edges, 50, 150);
        
        // Send to web server or UART
        send_image(&edges);
        
        esp_camera_fb_return(fb);
    }
}
```

## Performance Tips

1. **Use PSRAM**: Enable SPIRAM for large buffers
2. **DMA**: Use DMA for I2S and SPI transfers
3. **Fixed-point**: Use INT8 quantized models
4. **Cache**: Align buffers to 32-byte boundaries

## Pin Configurations

### ESP32-CAM
| Function | GPIO |
|----------|------|
| Camera SIOD | 26 |
| Camera SIOC | 27 |
| Camera VSYNC | 25 |
| Camera HREF | 23 |
| Camera PCLK | 22 |
| LED Flash | 4 |

### I2S Microphone (INMP441)
| Function | GPIO |
|----------|------|
| WS (LRCLK) | 25 |
| SCK (BCLK) | 26 |
| SD (Data) | 22 |
