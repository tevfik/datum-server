# API Quick Reference

Fast lookup for common EIF functions.

## DSP - Smoothing

| Function | Purpose |
|----------|---------|
| `eif_ema_init(t*, a)` | Init EMA with alpha |
| `eif_ema_update(t*, x)` | Update and get smooth value |
| `eif_median_init(t*, n)` | Init median filter |
| `eif_median_update(t*, x)` | Update and get median |
| `eif_ma_init(t*, n)` | Init moving average |
| `eif_ma_update(t*, x)` | Update and get average |
| `eif_debounce_init(t*, n)` | Init debouncer |
| `eif_debounce_update(t*, b)` | Update and get stable bool |

## DSP - Filters

| Function | Purpose |
|----------|---------|
| `eif_fir_init(t*, coeffs, n)` | Init FIR filter |
| `eif_fir_process(t*, x)` | Process sample |
| `eif_fir_q15_init(t*, coeffs, n)` | Init Q15 FIR |
| `eif_fir_q15_process(t*, x)` | Process Q15 sample |
| `eif_biquad_lowpass(t*, fs, fc, q)` | Design lowpass |
| `eif_biquad_highpass(t*, fs, fc, q)` | Design highpass |
| `eif_biquad_bandpass(t*, fs, fc, q)` | Design bandpass |
| `eif_biquad_process(t*, x)` | Process sample |

## ML - Thresholds

| Function | Purpose |
|----------|---------|
| `eif_z_threshold_init(t*, a, th)` | Init Z-score detector |
| `eif_z_threshold_check(t*, x)` | Check for anomaly |
| `eif_z_threshold_get_bounds(t*, lo, hi)` | Get threshold bounds |

## ML - Sensor Fusion

| Function | Purpose |
|----------|---------|
| `eif_complementary_init(t*, a, dt)` | Init complementary filter |
| `eif_complementary_update(t*, gyro, accel)` | Update fusion |
| `eif_kalman_1d_init(t*, x0, p0, q, r)` | Init 1D Kalman |
| `eif_kalman_1d_update(t*, z)` | Update Kalman |

## ML - Activity Recognition

| Function | Purpose |
|----------|---------|
| `eif_activity_window_init(t*, hop)` | Init sliding window |
| `eif_activity_window_add(t*, x, y, z)` | Add sample |
| `eif_activity_extract_features(s, n, f)` | Extract features |
| `eif_activity_classify_rules(f)` | Rule-based classify |

## ML - Predictive Maintenance

| Function | Purpose |
|----------|---------|
| `eif_health_init(t*, base, warn, crit)` | Init health indicator |
| `eif_health_update(t*, x)` | Update health |
| `eif_health_get_normalized(t*)` | Get 0-1 health |
| `eif_rul_init(t*, threshold, interval)` | Init RUL estimator |
| `eif_rul_update(t*, x)` | Add degradation sample |
| `eif_rul_estimate(t*)` | Estimate time to failure |

## Benchmarking

| Function | Purpose |
|----------|---------|
| `eif_benchmark_init(t*, warmup)` | Init benchmark |
| `eif_benchmark_start(t*)` | Start timing |
| `eif_benchmark_stop(t*)` | Stop timing |
| `eif_benchmark_stats(t*, r)` | Calculate stats |
| `eif_benchmark_print(r)` | Print result |

## Types

| Type | Purpose |
|------|---------|
| `eif_ema_t` | EMA filter state |
| `eif_median_t` | Median filter state |
| `eif_fir_t` | FIR filter state |
| `eif_biquad_t` | Biquad filter state |
| `eif_z_threshold_t` | Z-score detector |
| `eif_complementary_t` | Complementary filter |
| `eif_kalman_1d_t` | 1D Kalman filter |
| `eif_activity_window_t` | Activity window buffer |
| `eif_activity_features_t` | Activity features |
| `eif_health_indicator_t` | Health indicator |
| `eif_rul_estimator_t` | RUL estimator |
