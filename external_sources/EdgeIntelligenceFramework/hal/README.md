# EIF Hardware Abstraction Layer (HAL)

Portable hardware interface for embedded platforms with PC-based mock testing.

## Quick Start

```c
#include "eif_hal.h"  // Auto-detects platform

// IMU
eif_imu_config_t cfg = {.sample_rate_hz = 100.0f};
eif_hal_imu_init(&cfg);

eif_imu_data_t data;
eif_hal_imu_read(&data);
printf("ax=%.2f az=%.2f\n", data.ax, data.az);

// Timer
eif_hal_delay_ms(100);
uint32_t t = eif_hal_timer_ms();
```

## Compile Options

```bash
# PC/Mock testing
gcc -DEIF_USE_MOCK_HAL -I hal/include app.c hal/platforms/generic/eif_hal_impl_mock.c -lm

# ESP32
idf.py build  # Uses real hardware
```

## API Summary

| Module | Functions |
|--------|-----------|
| **IMU** | `imu_init`, `imu_read`, `imu_calibrate` |
| **Audio** | `audio_init`, `audio_read`, `audio_read_float` |
| **GPIO** | `gpio_init`, `gpio_write`, `gpio_read`, `gpio_toggle` |
| **Timer** | `timer_us`, `timer_ms`, `delay_us`, `delay_ms` |
| **Serial** | `serial_init`, `serial_write`, `serial_readline` |
| **LED** | `led_init`, `led_on`, `led_off`, `led_blink` |

## Mock HAL Features

For PC-based testing without hardware:

- **IMU**: Synthetic sine waves + noise
- **Audio**: Tone generation (440Hz)
- **GPIO**: Virtual pins in memory
- **Timer**: System clock  
- **Serial**: stdin/stdout

## Files

```
hal/
├── include/
│   ├── eif_hal.h           # Unified interface
│   └── eif_hal_mock.h      # Mock configuration
└── platforms/
    └── generic/
        └── eif_hal_impl_mock.c  # Mock implementation
```

## Platform Support

| Platform | Status |
|----------|--------|
| Linux/PC | ✅ Mock HAL |
| ESP32 | ✅ Full |
| STM32 | 🔄 Partial |
