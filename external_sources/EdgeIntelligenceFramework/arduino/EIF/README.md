# Edge Intelligence Framework - Arduino Library

Lightweight AI/ML library for Arduino and embedded systems.

## Features

- **DSP Filters**: EMA, Median, FIR, IIR/Biquad (float and Q15 fixed-point)
- **Anomaly Detection**: Adaptive Z-score thresholds
- **Sensor Fusion**: Complementary filter, weighted fusion, voting
- **Activity Recognition**: IMU-based human activity detection
- **Predictive Maintenance**: Health indicators, RUL estimation

## Supported Boards

| Board | RAM | Features |
|-------|-----|----------|
| Uno/Mega | 2-8KB | DSP only (fixed-point) |
| Due/Zero | 32-96KB | Full ML |
| Nano 33 BLE | 256KB | TinyML |
| ESP32 | 520KB | Full Edge AI |

## Installation

1. Download or clone this repository
2. Copy the `EIF` folder to your Arduino libraries folder
3. Restart Arduino IDE
4. Include with `#include <EIF.h>`

## Quick Start

```cpp
#include <EIF.h>

eif_ema_t smoother;

void setup() {
  eif_ema_init(&smoother, 0.2f);
}

void loop() {
  float raw = analogRead(A0);
  float smooth = eif_ema_update(&smoother, raw);
}
```

## Examples

- **SensorSmoothing** - Basic EMA filter
- **ButtonDebounce** - Debounce mechanical buttons
- **AnomalyDetection** - Adaptive threshold detection
- **ActivityRecognition** - IMU-based HAR
- **RealtimeFilter** - Biquad lowpass filter

## Documentation

See `ARDUINO_GUIDE.md` for complete documentation.

## License

MIT License
