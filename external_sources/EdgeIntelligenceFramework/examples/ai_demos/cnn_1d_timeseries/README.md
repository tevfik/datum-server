# 1D CNN Time Series Demo

This demo illustrates using **1D Convolutional Neural Networks (Conv1D)** for time-series classification, typically used in Human Activity Recognition (HAR) or sensor data analysis.

## Algorithm
1. **Input**: Multi-channel time series (e.g., Accelerometer X, Y, Z).
2. **Conv1D Layer**: Extracts temporal features (e.g., spikes, patterns).
3. **MaxPooling**: Downsamples features.
4. **Dense Layer**: Classifies features into "Standing" or "Walking".

## Usage
```bash
./bin/cnn_1d_timeseries          # ASCII output
./bin/cnn_1d_timeseries --json   # JSON output
```

## JSON Output
```json
{
  "type": "cnn_1d",
  "prediction": "Walking",
  "probabilities": {"standing": 0.05, "walking": 0.95},
  "signal_sample": [0.0, 0.99, 0.0]
}
```
