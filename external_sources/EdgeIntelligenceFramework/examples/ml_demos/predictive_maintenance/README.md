# Predictive Maintenance Demo

Demonstrates industrial IoT predictive maintenance using EIF.

## Features

- **Health Indicator Monitoring** - Track equipment health over time with trend analysis
- **Remaining Useful Life (RUL)** - Estimate time to failure using linear extrapolation
- **Vibration Analysis** - Calculate RMS, crest factor, kurtosis for bearing health
- **Maintenance Recommendations** - Automated alerts and scheduling

## Running

```bash
./build/bin/predictive_maintenance_demo --batch
```

## Sample Output

```
╔══════════════════════════════════╗
║  1. Health Indicator Monitoring  ║
╚══════════════════════════════════╝

  Day   Reading   Health    State      Trend
  ----  --------  --------  ---------  -----
    1      1.02     99.6%   GOOD       +0.000
    5      1.08     96.6%   GOOD       +0.002
   10      1.25     82.4%   GOOD       +0.007
   15      1.58     53.1%   WARNING    +0.016

╔══════════════════════════════════╗
║  2. Remaining Useful Life (RUL)  ║
╚══════════════════════════════════╝

  Estimated RUL: 18.3 days
  ⚠️ Schedule maintenance within 18 days
```

## Use Cases

- Motor condition monitoring
- Bearing fault prediction
- Pump degradation tracking
- Industrial equipment maintenance

## API Reference

```c
// Health indicator
eif_health_init(&hi, baseline, warning_threshold, critical_threshold);
eif_health_update(&hi, reading);
float health = eif_health_get_normalized(&hi);

// RUL estimation
eif_rul_init(&rul, failure_threshold, sampling_interval);
eif_rul_update(&rul, degradation_value);
float rul = eif_rul_estimate(&rul);  // Time to failure
```
