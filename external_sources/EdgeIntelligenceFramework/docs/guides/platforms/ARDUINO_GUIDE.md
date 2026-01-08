# Arduino Integration Guide

Complete guide for using Edge Intelligence Framework with Arduino.

## Supported Arduino Boards

| Board | MCU | RAM | Flash | FPU | Best For |
|-------|-----|-----|-------|-----|----------|
| **Uno R3** | ATmega328P | 2KB | 32KB | No | Simple DSP only |
| **Mega 2560** | ATmega2560 | 8KB | 256KB | No | Fixed-point DSP |
| **Due** | SAM3X8E | 96KB | 512KB | No | Full ML/DL |
| **Zero** | SAMD21 | 32KB | 256KB | No | Moderate ML |
| **Nano 33 BLE** | nRF52840 | 256KB | 1MB | Yes | TinyML, BLE |
| **Nano 33 IoT** | SAMD21 | 32KB | 256KB | No | IoT, WiFi |
| **Portenta H7** | STM32H747 | 1MB | 2MB | Yes | Full Edge AI |
| **Nano RP2040** | RP2040 | 264KB | 16MB | No | Moderate ML |

### ESP32 with Arduino
| Board | RAM | Flash | FPU | Best For |
|-------|-----|-------|-----|----------|
| ESP32 | 520KB | 4MB | Yes | Full Edge AI |
| ESP32-S3 | 512KB | 8MB | Yes | AI + Camera |
| ESP32-C3 | 400KB | 4MB | No | Low-power AI |

---

## Installation

### Method 1: Copy Headers
```bash
# Copy EIF headers to your Arduino libraries folder
cp -r edge-intelligence-framework/dsp/include/* ~/Arduino/libraries/EIF/src/
cp -r edge-intelligence-framework/ml/include/* ~/Arduino/libraries/EIF/src/
cp -r edge-intelligence-framework/dl/include/* ~/Arduino/libraries/EIF/src/
```

### Method 2: Arduino Library Manager (Coming Soon)
Search for "EdgeIntelligence" in Library Manager.

### Method 3: Manual Installation
1. Download ZIP from GitHub
2. Arduino IDE → Sketch → Include Library → Add .ZIP Library

---

## Basic Usage

### Include Headers
```cpp
// Arduino sketch (.ino)
#include <EIF.h>

// Or include specific modules
#include "eif_dsp_smooth.h"
#include "eif_adaptive_threshold.h"
```

### Sensor Smoothing Example
```cpp
#include "eif_dsp_smooth.h"

eif_ema_t ema;

void setup() {
  Serial.begin(115200);
  eif_ema_init(&ema, 0.2f);  // alpha = 0.2
}

void loop() {
  int raw = analogRead(A0);
  float smoothed = eif_ema_update(&ema, (float)raw);
  
  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print(" Smoothed: ");
  Serial.println(smoothed);
  
  delay(50);
}
```

---

## Platform-Specific Notes

### AVR (Uno, Mega) - Very Limited RAM

**Best practices:**
```cpp
// Use fixed-point for AVR (no floating point unit)
#include "eif_dsp_fir_fixed.h"

eif_fir_q15_t fir;
int16_t coeffs[8];

void setup() {
  eif_fir_q15_design_ma(coeffs, 8);
  eif_fir_q15_init(&fir, coeffs, 8);
}

void loop() {
  int16_t input = analogRead(A0) - 512;  // Center around 0
  int16_t output = eif_fir_q15_process(&fir, input);
}
```

**Limitations:**
- 2KB RAM: Only simple filters, no ML
- 32KB Flash: Limited model size
- No FPU: Use Q15 fixed-point

**Recommended features:**
- EMA smoothing
- Median filter
- Fixed-point FIR/IIR
- Simple thresholding

### ARM Cortex-M (Due, Zero, Nano 33)

**Best practices:**
```cpp
// Full floating point support on ARM
#include "eif_dsp_smooth.h"
#include "eif_adaptive_threshold.h"

eif_z_threshold_t anomaly;

void setup() {
  Serial.begin(115200);
  eif_z_threshold_init(&anomaly, 0.1f, 3.0f);  // 3-sigma
}

void loop() {
  float value = analogRead(A0) / 1023.0f;
  
  if (eif_z_threshold_check(&anomaly, value)) {
    Serial.println("ANOMALY DETECTED!");
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
  
  delay(100);
}
```

**Capabilities:**
- Due (96KB): Full ML inference
- Zero (32KB): Moderate ML
- Nano 33 BLE (256KB): TinyML models

### ESP32 with Arduino

**Best practices:**
```cpp
#include <WiFi.h>
#include "eif_activity.h"
#include "eif_sensor_fusion.h"

// Use PSRAM for large buffers (if available)
eif_accel_sample_t *samples;

void setup() {
  // Allocate in PSRAM on ESP32-S3
  #ifdef CONFIG_SPIRAM_SUPPORT
    samples = (eif_accel_sample_t*)ps_malloc(
      EIF_ACTIVITY_WINDOW_SIZE * sizeof(eif_accel_sample_t)
    );
  #else
    samples = (eif_accel_sample_t*)malloc(
      EIF_ACTIVITY_WINDOW_SIZE * sizeof(eif_accel_sample_t)
    );
  #endif
}
```

**ESP32 specific:**
- Use `DRAM_ATTR` for frequently accessed data
- Use `PROGMEM` for constant weights
- Enable FreeRTOS tasks for background processing

---

## Complete Examples

### 1. Button Debouncing
```cpp
#include "eif_dsp_smooth.h"

eif_debounce_t db;
const int BUTTON_PIN = 2;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  eif_debounce_init(&db, 5);  // 5 consecutive samples
}

void loop() {
  bool raw = !digitalRead(BUTTON_PIN);  // Active low
  bool stable = eif_debounce_update(&db, raw);
  
  if (stable) {
    Serial.println("Button pressed!");
  }
  
  delay(10);  // Debounce at 100Hz
}
```

### 2. Accelerometer Smoothing (MPU6050)
```cpp
#include <Wire.h>
#include "eif_dsp_smooth.h"

eif_ema_t ema_x, ema_y, ema_z;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Initialize MPU6050 (simplified)
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);  // PWR_MGMT_1
  Wire.write(0);     // Wake up
  Wire.endTransmission(true);
  
  // Initialize smoothers with alpha=0.2
  eif_ema_init(&ema_x, 0.2f);
  eif_ema_init(&ema_y, 0.2f);
  eif_ema_init(&ema_z, 0.2f);
}

void loop() {
  int16_t ax, ay, az;
  
  // Read accelerometer
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 6, true);
  
  ax = Wire.read() << 8 | Wire.read();
  ay = Wire.read() << 8 | Wire.read();
  az = Wire.read() << 8 | Wire.read();
  
  // Smooth
  float sx = eif_ema_update(&ema_x, ax / 16384.0f);
  float sy = eif_ema_update(&ema_y, ay / 16384.0f);
  float sz = eif_ema_update(&ema_z, az / 16384.0f);
  
  Serial.print(sx); Serial.print(",");
  Serial.print(sy); Serial.print(",");
  Serial.println(sz);
  
  delay(20);
}
```

### 3. Anomaly Detection (Temperature Sensor)
```cpp
#include "eif_adaptive_threshold.h"

eif_z_threshold_t anomaly;
const int TEMP_PIN = A0;
const int LED_PIN = LED_BUILTIN;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // 3-sigma threshold, alpha=0.05 for slow adaptation
  eif_z_threshold_init(&anomaly, 0.05f, 3.0f);
}

void loop() {
  // Read temperature (example: TMP36)
  int raw = analogRead(TEMP_PIN);
  float voltage = raw * (5.0f / 1023.0f);
  float tempC = (voltage - 0.5f) * 100.0f;
  
  // Check for anomaly
  if (eif_z_threshold_check(&anomaly, tempC)) {
    Serial.print("! ANOMALY: ");
    Serial.println(tempC);
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.print("Normal: ");
    Serial.println(tempC);
    digitalWrite(LED_PIN, LOW);
  }
  
  delay(1000);
}
```

### 4. Activity Recognition (Nano 33 BLE)
```cpp
#include <Arduino_LSM9DS1.h>
#include "eif_activity.h"

eif_activity_window_t window;
eif_activity_features_t features;
eif_accel_sample_t samples[EIF_ACTIVITY_WINDOW_SIZE];

void setup() {
  Serial.begin(115200);
  IMU.begin();
  eif_activity_window_init(&window, 64);  // Hop 64 samples
}

void loop() {
  float x, y, z;
  
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    
    // Scale to m/s² (LSM9DS1 default is ±2g)
    x *= 9.81f; y *= 9.81f; z *= 9.81f;
    
    if (eif_activity_window_add(&window, x, y, z)) {
      // Window ready - classify
      eif_activity_window_get_samples(&window, samples);
      eif_activity_extract_features(samples, EIF_ACTIVITY_WINDOW_SIZE, &features);
      
      eif_activity_t activity = eif_activity_classify_rules(&features);
      
      Serial.print("Activity: ");
      Serial.println(eif_activity_names[activity]);
    }
  }
  
  delay(20);  // 50 Hz
}
```

### 5. Real-Time Filter (Audio)
```cpp
#include "eif_dsp_biquad.h"

eif_biquad_t lowpass;
const int MIC_PIN = A0;
const int OUT_PIN = 9;  // PWM output

void setup() {
  // 1kHz lowpass at 8kHz sample rate
  eif_biquad_lowpass(&lowpass, 8000.0f, 1000.0f, 0.707f);
  
  // Fast ADC for audio
  #if defined(__AVR__)
    ADCSRA = (ADCSRA & 0xF8) | 0x04;  // Prescaler 16
  #endif
}

void loop() {
  // Read at ~8kHz
  int sample = analogRead(MIC_PIN) - 512;
  float filtered = eif_biquad_process(&lowpass, sample / 512.0f);
  
  // Output
  int output = (int)(filtered * 127 + 128);
  analogWrite(OUT_PIN, constrain(output, 0, 255));
}
```

---

## Memory Optimization

### For AVR (2KB RAM)
```cpp
// Use fixed-point
#include "eif_dsp_fir_fixed.h"
#include "eif_dsp_biquad_fixed.h"

// Use PROGMEM for constants
const int16_t PROGMEM coeffs[] = {1000, 2000, 3000, 2000, 1000};

// Read from PROGMEM
int16_t c0 = pgm_read_word(&coeffs[0]);
```

### For ARM/ESP32 (More RAM)
```cpp
// Full floating point is fine
#include "eif_dsp_smooth.h"
#include "eif_activity.h"

// Use dynamic allocation for large buffers
float *buffer = (float*)malloc(1024 * sizeof(float));
```

---

## Troubleshooting

### Compilation Errors
```
error: 'sqrtf' was not declared
```
**Solution:** Add `#include <math.h>` before EIF headers.

### Out of Memory
```
Error: section .data will not fit in region `data'
```
**Solution:** Reduce buffer sizes or use smaller models.

### Slow Performance
- Use fixed-point on AVR
- Reduce sample rate
- Use smaller window sizes

---

## Arduino Library Structure

To package as Arduino library:

```
EIF/
├── src/
│   ├── EIF.h              # Main include
│   ├── eif_dsp_smooth.h
│   ├── eif_dsp_fir.h
│   ├── eif_dsp_biquad.h
│   ├── eif_dsp_fir_fixed.h
│   ├── eif_dsp_biquad_fixed.h
│   ├── eif_adaptive_threshold.h
│   ├── eif_sensor_fusion.h
│   ├── eif_activity.h
│   └── eif_predictive_maintenance.h
├── examples/
│   ├── SensorSmoothing/
│   ├── ButtonDebounce/
│   ├── AnomalyDetection/
│   └── ActivityRecognition/
├── library.properties
├── keywords.txt
└── README.md
```

---

## Quick Reference

| Task | Include | Function |
|------|---------|----------|
| Smooth sensor | `eif_dsp_smooth.h` | `eif_ema_update()` |
| Debounce button | `eif_dsp_smooth.h` | `eif_debounce_update()` |
| Filter noise | `eif_dsp_biquad.h` | `eif_biquad_process()` |
| Detect anomaly | `eif_adaptive_threshold.h` | `eif_z_threshold_check()` |
| Fuse sensors | `eif_sensor_fusion.h` | `eif_complementary_update()` |
| Activity | `eif_activity.h` | `eif_activity_classify_rules()` |
