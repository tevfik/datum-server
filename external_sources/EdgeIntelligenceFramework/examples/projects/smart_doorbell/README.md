# Smart Doorbell Project

Complete end-to-end smart doorbell application for ESP32-CAM.

## Features

- **Motion Detection**: Background subtraction
- **Person Detection**: CNN-based classification
- **State Machine**: IDLE → MOTION → ANALYZE → ALERT → COOLDOWN
- **Notifications**: MQTT push notifications
- **Image Capture**: Upload to cloud storage

## Hardware

| Component | Purpose |
|-----------|---------|
| ESP32-CAM | Camera + processing |
| PIR Sensor (optional) | Low-power trigger |
| Button | Manual doorbell |
| LED | Status indicator |

## Architecture

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Camera    │────▶│   Motion    │────▶│   Person    │
│   Capture   │     │   Detect    │     │   Detect    │
└─────────────┘     └─────────────┘     └─────────────┘
                                               │
                                               ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Upload    │◀────│   Capture   │◀────│   Alert     │
│   Cloud     │     │   Image     │     │   Notify    │
└─────────────┘     └─────────────┘     └─────────────┘
```

## Build

```bash
# Desktop simulation
make smart_doorbell_demo
./bin/smart_doorbell_demo

# ESP32
cd examples/projects/smart_doorbell
idf.py build flash monitor
```

## EIF Modules Used

- `eif_cv` - Motion detection, image processing
- `eif_nn` - Person detection CNN
- `eif_memory` - Memory pool management

## Customization

| Parameter | Default | Description |
|-----------|---------|-------------|
| `MOTION_THRESHOLD` | 25 | Pixel difference for motion |
| `PERSON_CONFIDENCE` | 0.6 | Min confidence for person |
| `COOLDOWN_SECONDS` | 5 | Wait after alert |
