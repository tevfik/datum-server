# Raspberry Pi Pico Guide

Complete guide for deploying Edge Intelligence Framework on RP2040/Raspberry Pi Pico.

## RP2040 Specifications

| Feature | Value |
|---------|-------|
| **CPU** | Dual Cortex-M0+ @ 133MHz |
| **RAM** | 264KB SRAM |
| **Flash** | 2MB (Pico), 16MB (Pico W) |
| **FPU** | None (software float) |
| **ADC** | 12-bit, 4 channels |
| **PIO** | 2x Programmable I/O |

## Development Options

### 1. Arduino IDE (Easy)
Use the [arduino-pico](https://github.com/earlephilhower/arduino-pico) core.

### 2. Pico SDK (Recommended)
Native C/C++ SDK with CMake.

### 3. MicroPython
Use C modules for performance-critical code.

---

## Pico SDK Integration

### Project Structure
```
my_project/
├── CMakeLists.txt
├── pico_sdk_import.cmake
├── src/
│   └── main.c
└── lib/
    └── eif/
        ├── dsp/
        ├── ml/
        └── dl/
```

### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.13)

# Include Pico SDK
include(pico_sdk_import.cmake)

project(eif_demo C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# EIF include directories
set(EIF_PATH ${CMAKE_SOURCE_DIR}/lib/eif)
include_directories(
    ${EIF_PATH}/dsp/include
    ${EIF_PATH}/ml/include
    ${EIF_PATH}/dl/include
)

add_executable(main
    src/main.c
)

# Link libraries
target_link_libraries(main 
    pico_stdlib
    hardware_adc
    hardware_dma
)

# Enable USB serial output
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)

pico_add_extra_outputs(main)
```

---

## Fixed-Point for RP2040

RP2040 has no FPU, so use Q15 fixed-point for DSP:

```c
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "eif_dsp_fir_fixed.h"
#include "eif_dsp_biquad_fixed.h"

// Q15 FIR filter
eif_fir_q15_t fir;
int16_t coeffs[16];

int main() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(26);  // ADC0 on GPIO26
    adc_select_input(0);
    
    // Design lowpass FIR
    eif_fir_q15_design_lowpass(coeffs, 16, 0.1f);
    eif_fir_q15_init(&fir, coeffs, 16);
    
    while (true) {
        // Read ADC (12-bit) and convert to Q15
        uint16_t raw = adc_read();
        int16_t input = (raw - 2048) << 3;  // Center and scale
        
        // Filter
        int16_t output = eif_fir_q15_process(&fir, input);
        
        printf("Raw: %d, Filtered: %d\n", raw, (output >> 3) + 2048);
        sleep_ms(10);
    }
}
```

## Floating-Point (When Needed)

For ML that needs float, RP2040 uses software floating-point:

```c
#include "eif_adaptive_threshold.h"

eif_z_threshold_t detector;

int main() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    
    // Initialize detector (uses float)
    eif_z_threshold_init(&detector, 0.05f, 3.0f);
    
    while (true) {
        float value = adc_read() / 4095.0f;
        
        if (eif_z_threshold_check(&detector, value)) {
            printf("ANOMALY!\n");
        }
        
        sleep_ms(100);
    }
}
```

> **Note**: Floating-point is ~10-50x slower than fixed-point on RP2040.

---

## DMA for Audio

Use DMA for continuous audio sampling:

```c
#include "hardware/dma.h"
#include "hardware/adc.h"

#define SAMPLE_RATE 8000
#define BUFFER_SIZE 256

uint16_t audio_buffer[2][BUFFER_SIZE];
volatile int active_buffer = 0;

void dma_handler() {
    // Clear interrupt
    dma_hw->ints0 = 1u << dma_chan;
    
    // Process completed buffer
    process_audio(audio_buffer[active_buffer], BUFFER_SIZE);
    
    // Switch buffers
    active_buffer = 1 - active_buffer;
    
    // Restart DMA on other buffer
    dma_channel_set_write_addr(dma_chan, 
        audio_buffer[active_buffer], true);
}

void setup_audio_dma() {
    // Configure ADC
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(48000000 / SAMPLE_RATE);
    
    // Configure DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);
    
    dma_channel_configure(dma_chan, &cfg,
        audio_buffer[0],    // Write to buffer
        &adc_hw->fifo,      // Read from ADC FIFO
        BUFFER_SIZE,
        true                // Start immediately
    );
    
    // Enable interrupt
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    
    // Start ADC
    adc_run(true);
}
```

---

## Dual-Core Processing

Use both cores for parallel processing:

```c
#include "pico/multicore.h"
#include "eif_activity.h"

// Shared data
volatile float features[15];
volatile bool features_ready = false;

// Core 1: Feature extraction
void core1_entry() {
    eif_activity_window_t window;
    eif_activity_window_init(&window, 64);
    
    while (true) {
        // Read IMU, add to window
        float x, y, z;
        read_imu(&x, &y, &z);
        
        if (eif_activity_window_add(&window, x, y, z)) {
            eif_accel_sample_t samples[128];
            eif_activity_features_t f;
            
            eif_activity_window_get_samples(&window, samples);
            eif_activity_extract_features(samples, 128, &f);
            
            eif_activity_features_to_array(&f, (float*)features);
            features_ready = true;
        }
        
        sleep_ms(20);
    }
}

// Core 0: Classification + main loop
int main() {
    stdio_init_all();
    
    // Launch Core 1
    multicore_launch_core1(core1_entry);
    
    while (true) {
        if (features_ready) {
            features_ready = false;
            
            // Classify (runs on Core 0)
            eif_activity_t activity = classify_from_features(features);
            printf("Activity: %s\n", eif_activity_names[activity]);
        }
        
        sleep_ms(10);
    }
}
```

---

## Power Optimization

### Sleep Modes
```c
#include "pico/sleep.h"

// Light sleep - keep timers running
sleep_ms(1000);

// Deep sleep - minimal power
sleep_goto_dormant_until_pin(WAKEUP_PIN, true, false);

// Clock gating
clock_stop(clk_adc);  // Stop ADC clock when not needed
```

### Reduce Clock Speed
```c
// Reduce to 48MHz for power savings
set_sys_clock_48mhz();

// Or even lower
set_sys_clock_khz(12000, true);  // 12 MHz
```

---

## PIO for Sensors

Use PIO for precise timing:

```c
// pio_i2s.pio - I2S input for microphone
.program i2s_input
.side_set 1

.wrap_target
    in pins, 1          side 0
    in pins, 1          side 1
.wrap

// C code to use PIO
PIO pio = pio0;
uint sm = 0;
uint offset = pio_add_program(pio, &i2s_input_program);
pio_sm_config c = i2s_input_program_get_default_config(offset);
sm_config_set_in_pins(&c, DATA_PIN);
sm_config_set_sideset_pins(&c, CLK_PIN);
pio_sm_init(pio, sm, offset, &c);
pio_sm_set_enabled(pio, sm, true);
```

---

## Complete Example: Activity Recognition

```c
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "eif_activity.h"

#define LED_PIN 25

void read_mpu6050(float *x, float *y, float *z) {
    uint8_t buf[6];
    uint8_t reg = 0x3B;
    
    i2c_write_blocking(i2c0, 0x68, &reg, 1, true);
    i2c_read_blocking(i2c0, 0x68, buf, 6, false);
    
    int16_t raw_x = (buf[0] << 8) | buf[1];
    int16_t raw_y = (buf[2] << 8) | buf[3];
    int16_t raw_z = (buf[4] << 8) | buf[5];
    
    *x = raw_x / 16384.0f * 9.81f;
    *y = raw_y / 16384.0f * 9.81f;
    *z = raw_z / 16384.0f * 9.81f;
}

int main() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // I2C setup for MPU6050
    i2c_init(i2c0, 400000);
    gpio_set_function(4, GPIO_FUNC_I2C);  // SDA
    gpio_set_function(5, GPIO_FUNC_I2C);  // SCL
    
    // Wake up MPU6050
    uint8_t data[] = {0x6B, 0x00};
    i2c_write_blocking(i2c0, 0x68, data, 2, false);
    
    // Activity recognition
    eif_activity_window_t window;
    eif_activity_window_init(&window, 64);
    
    printf("Activity Recognition on Pico\n");
    
    while (true) {
        float x, y, z;
        read_mpu6050(&x, &y, &z);
        
        if (eif_activity_window_add(&window, x, y, z)) {
            eif_accel_sample_t samples[128];
            eif_activity_features_t features;
            
            eif_activity_window_get_samples(&window, samples);
            eif_activity_extract_features(samples, 128, &features);
            
            eif_activity_t activity = eif_activity_classify_rules(&features);
            
            printf("Activity: %s (mag_std: %.2f)\n", 
                   eif_activity_names[activity], features.magnitude_std);
            
            // LED on when moving
            gpio_put(LED_PIN, activity != EIF_ACTIVITY_STATIONARY);
        }
        
        sleep_ms(20);  // 50 Hz
    }
}
```

---

## Build & Flash

```bash
# Build
mkdir build && cd build
cmake ..
make

# Flash via USB bootloader
# Hold BOOTSEL, plug in USB, release
cp main.uf2 /media/RPI-RP2/

# Or via picotool
picotool load main.uf2
picotool reboot
```

---

## Memory Budget

| Usage | Size |
|-------|------|
| EIF headers | ~0 (inline) |
| Activity window | ~1 KB |
| App code | ~20 KB |
| Stack | ~4 KB per core |
| **Available** | ~235 KB |

The RP2040's 264KB RAM is sufficient for most edge AI tasks!
