# Anomaly Detection Demo - Predictive Maintenance

Multi-method anomaly detection for industrial IoT and predictive maintenance.

## Features

| Method | Description | Best For |
|--------|-------------|----------|
| **Statistical** | Z-score based | Simple, fast, univariate |
| **Isolation Forest** | Tree-based | Multivariate, unknown patterns |
| **Time Series** | EWMA + CUSUM | Temporal patterns, drift |
| **Ensemble** | Combined | Best accuracy |

## Usage

```bash
cd build && make anomaly_demo && ./bin/anomaly_demo
```

## Example Output

```
=== Demo 4: Ensemble Detector (Combined) ===
  Idx    Temp    Vib   Curr   Score  Status
  0      61.2   0.21   5.8    0.12  normal
  15     92.3   0.18   6.2    0.87  ANOMALY!
  ...
  Ensemble detected: 5 / 5 true anomalies
```

## Use Cases

- Equipment health monitoring
- Manufacturing quality control
- Sensor drift detection
- Network intrusion detection
