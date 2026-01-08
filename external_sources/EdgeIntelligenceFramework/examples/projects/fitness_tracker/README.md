# Fitness Tracker Project

IMU-based activity recognition and step counting for wearables.

## Features

- **Activity Recognition**: Idle, Walking, Running, Cycling
- **Step Counting**: Threshold-based pedometer
- **Calorie Estimation**: Activity-based MET calculation
- **Session Tracking**: Duration and breakdown

## Hardware

| Component | Purpose |
|-----------|---------|
| ESP32 | Processing |
| MPU6050/BMI160 | IMU sensor |
| Button | Start/stop session |
| OLED (optional) | Display stats |

## Algorithm

```
IMU Data (50Hz)
     │
     ▼
┌──────────────┐
│ Feature      │ Mean, Std, Magnitude
│ Extraction   │ per 2-second window
└──────────────┘
     │
     ▼
┌──────────────┐
│ Activity     │ Threshold/ML classifier
│ Recognition  │ 
└──────────────┘
     │
     ├──▶ Step Detection (magnitude peaks)
     │
     └──▶ Calorie Calculation (MET × time)
```

## EIF Modules

- `eif_dsp` - FFT for frequency features
- `eif_nn` - Activity classifier
- `eif_memory` - Memory management

## Build

```bash
make fitness_tracker_demo
./bin/fitness_tracker_demo
```
