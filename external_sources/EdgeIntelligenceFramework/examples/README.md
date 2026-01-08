# EIF Examples

This directory contains examples demonstrating the Edge Intelligence Framework capabilities.

## Structure

```
examples/
├── common/              # Shared utilities (ascii_plot.h)
├── api_tests/           # Low-level API verification tests
├── ai_demos/            # Machine learning demos
├── dsp_demos/           # Signal processing demos
├── filter_demos/        # Kalman/Bayesian filter demos
├── ml_demos/            # Classical ML demos
├── cv_demos/            # Computer vision demos
├── hw_demos/            # Hardware/ESP32 demos
└── projects/            # Complete project examples
```

## Quick Start

```bash
# Build all examples
cd build && cmake .. && make

# Run a demo
./bin/signal_analysis_demo
```

## JSON-Enabled Demos 🆕

These demos support `--json` output for real-time visualization with `tools/eif_plotter.py`:

### Signal/ML Demos

| Demo | JSON Fields | Description |
|------|-------------|-------------|
| `kws_demo` | probs, mfcc_energy, prediction | Keyword spotting |
| `gesture_recognition_demo` | probs, x/y/z_energy, prediction | Accelerometer gesture |
| `drone_attitude_demo` | roll/pitch/yaw, motors, imu | Drone AHRS + PID |
| `anomaly_detection_demo` | temp/vib/curr, anomaly_score | Predictive maintenance |
| `motion_detect_demo` | motion_percent, blobs, activity | Video motion detection |
| `time_series_demo` | actual, arima, holt_winters | Energy forecasting |
| `signal_analysis_demo` | rms, peak_freq, centroid | Vibration diagnostics |
| `imu_fusion_demo` | imu, gps, mag, ekf | Sensor fusion |
| `bearing_fault_demo` | vibration, spectrum | Fault detection |
| `mnist_cnn_demo` | prediction, confidence, probs | Image classification |

### CV Demos

| Demo | Output | Description |
|------|--------|-------------|
| `edge_detection_demo` | Pipeline stages + files | Canny, Sobel |
| `object_tracking_demo` | Tracking positions + error | Multi-object tracking |
| `feature_matching_demo` | Keypoints + match score | FAST/Harris/Template |
| `robot_slam_demo` | Map + robot/landmark pos | EKF-SLAM |

### Usage

```bash
# Standard signal plot
./bin/kws_demo --json | python3 tools/eif_plotter.py --stdin

# CV pipeline visualization (shows output images)
./bin/edge_detection_demo --json | python3 tools/eif_plotter.py --stdin --cv

# Tracking visualization (trajectory + error plot)
./bin/object_tracking_demo --json | python3 tools/eif_plotter.py --stdin --tracking

# Just run with duration/frames control
./bin/drone_attitude_demo --json --duration 10
./bin/motion_detect_demo --json --frames 100
```

### Common CLI Options

| Option | Description |
|--------|-------------|
| `--json` | Output JSON for real-time plotting |
| `--continuous` | Run without interactive pauses |
| `--help` | Show usage and examples |

### Plotter Modes

| Mode | Command | Description |
|------|---------|-------------|
| Standard | `--stdin` | Real-time signal plots |
| CV Pipeline | `--stdin --cv` | Display output images from CV demos |
| Tracking | `--stdin --tracking` | Trajectory + error visualization |

## API Tests

Low-level tests verifying individual API functions:

| Test | Module | Purpose |
|------|--------|---------|
| dsp_test | DSP | FFT, filters, transforms |
| matrix_test | Core | Matrix operations |
| kalman_test | Bayesian | Kalman filter |
| ekf_test | Bayesian | Extended Kalman filter |

## Shared Utilities

### ascii_plot.h
Header-only library for ASCII visualizations:
- `ascii_plot_waveform()` - Time-domain signals
- `ascii_plot_spectrum()` - Frequency domain
- `ascii_bar_chart()` - Comparison charts
- `ascii_heatmap()` - 2D data visualization
- `ascii_progress_bar()` - Progress indicators

