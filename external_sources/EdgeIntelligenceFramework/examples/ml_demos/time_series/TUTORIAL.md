# Time Series Tutorial: Forecasting & Pattern Detection

## Learning Objectives

- Time series decomposition (trend, seasonality, residual)
- Moving averages and smoothing
- Autocorrelation analysis
- Pattern detection with Matrix Profile

**Level**: Beginner to Intermediate  
**Time**: 40 minutes

---

## 1. Time Series Components

```
Data = Trend + Seasonal + Residual

┌──────────────────────────────────────┐
│  Original Signal                      │
│    ╱╲    ╱╲    ╱╲    ╱╲              │ → Raw data
├──────────────────────────────────────┤
│  Trend                                │
│    ───────────────────▸               │ → Long-term direction
├──────────────────────────────────────┤
│  Seasonal                             │
│    ∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿               │ → Repeating pattern
├──────────────────────────────────────┤
│  Residual                             │
│    ·:·.·:··.:.·:·.·.·                 │ → Noise/anomalies
└──────────────────────────────────────┘
```

---

## 2. Moving Averages

### Simple Moving Average (SMA)

```c
// EIF implementation
eif_moving_avg_t ma;
eif_moving_avg_init(&ma, window_size, &pool);

for (int i = 0; i < n_samples; i++) {
    float smoothed = eif_moving_avg_update(&ma, data[i]);
}
```

### Exponential Moving Average (EMA)

```c
// More weight to recent values
EMA_t = α × x_t + (1-α) × EMA_{t-1}

eif_ema_t ema;
eif_ema_init(&ema, 0.1f);  // α = 0.1
float smoothed = eif_ema_update(&ema, value);
```

---

## 3. Autocorrelation

Measures self-similarity at different lags:

```c
// Compute ACF for lags 0 to max_lag
eif_ts_acf(data, n_samples, acf, max_lag);

// Find periodicity
for (int lag = 1; lag < max_lag; lag++) {
    if (acf[lag] > 0.5f) {
        printf("Period detected at lag %d\n", lag);
    }
}
```

---

## 4. STL Decomposition

```c
eif_stl_result_t result;
eif_stl_decompose(data, n_samples, period, &result, &pool);

// Access components
float* trend = result.trend;
float* seasonal = result.seasonal;
float* residual = result.residual;
```

---

## 5. ESP32 IoT Example

```c
// Temperature monitoring with trend detection
void sensor_task(void* arg) {
    eif_ema_t ema;
    eif_ema_init(&ema, 0.1f);
    
    while (1) {
        float temp = read_temperature();
        float smoothed = eif_ema_update(&ema, temp);
        
        // Detect upward trend
        static float prev_smoothed = 0;
        if (smoothed > prev_smoothed + 0.5f) {
            mqtt_publish("alert/temp_rising", smoothed);
        }
        prev_smoothed = smoothed;
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
```

---

## Summary

### Key APIs
- `eif_moving_avg_*()` - SMA
- `eif_ema_*()` - Exponential smoothing
- `eif_ts_acf()` - Autocorrelation
- `eif_stl_decompose()` - Decomposition
