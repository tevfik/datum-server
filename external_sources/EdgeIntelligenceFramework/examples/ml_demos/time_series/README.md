# Time Series Demo - Energy Consumption Forecasting

## Overview
This tutorial demonstrates **time series forecasting** for predicting energy consumption using ARIMA and Holt-Winters exponential smoothing.

## Scenario
A smart building predicts next-hour energy usage to:
- Optimize HVAC scheduling
- Reduce peak demand charges
- Enable demand response programs
- Improve energy efficiency

## Algorithms Used

### 1. ARIMA (Auto-Regressive Integrated Moving Average)
Classic statistical model for time series forecasting.

**Components:**
- **AR(p)** - Auto-Regressive: Uses past values
- **I(d)** - Integrated: Differencing for stationarity  
- **MA(q)** - Moving Average: Uses past forecast errors

**Model Equation:**
```
X_t = c + Σ(φᵢ × X_{t-i}) + Σ(θⱼ × ε_{t-j}) + ε_t
```

| Parameter | Meaning | Effect |
|-----------|---------|--------|
| p | AR order | How many past values to consider |
| d | Differencing | Removes trend/seasonality |
| q | MA order | How many past errors to consider |

**EIF API:**
```c
eif_ts_arima_t arima;
eif_ts_arima_init(&arima, p=2, d=0, q=1, &pool);
eif_ts_arima_fit(&arima, data, train_size);
eif_ts_arima_predict(&arima, last_value, &prediction);
```

### 2. Holt-Winters Exponential Smoothing
Captures level, trend, and seasonality.

**Three Components:**
```
Level:    L_t = α(Y_t - S_{t-m}) + (1-α)(L_{t-1} + T_{t-1})
Trend:    T_t = β(L_t - L_{t-1}) + (1-β)T_{t-1}
Season:   S_t = γ(Y_t - L_t) + (1-γ)S_{t-m}
```

**Forecast:**
```
Ŷ_{t+h} = L_t + h×T_t + S_{t+h-m}
```

| Parameter | Symbol | Range | Controls |
|-----------|--------|-------|----------|
| Level | α | 0-1 | Response to level changes |
| Trend | β | 0-1 | Response to trend changes |
| Season | γ | 0-1 | Response to seasonal changes |

**EIF API:**
```c
eif_ts_hw_t hw;
eif_ts_hw_init(&hw, season_length=24, EIF_TS_HW_ADDITIVE, &pool);
hw.alpha = 0.3f; hw.beta = 0.1f; hw.gamma = 0.2f;
for (int i = 0; i < train_size; i++) {
    eif_ts_hw_update(&hw, data[i]);
}
eif_ts_hw_forecast(&hw, horizon, predictions);
```

## Demo Walkthrough

1. **Data Generation** - 72 hours of hourly energy data with daily pattern
2. **Visualization** - Training data waveform
3. **ARIMA Training** - Fit ARIMA(2,0,1) model
4. **ARIMA Forecast** - Predict next 24 hours
5. **Holt-Winters Training** - Fit seasonal model
6. **Holt-Winters Forecast** - Predict with seasonality
7. **Comparison** - MAE metrics for both methods

## Forecast Accuracy Metrics

| Metric | Formula | Best For |
|--------|---------|----------|
| **MAE** | Σ\|y - ŷ\| / n | Overall accuracy |
| **RMSE** | √(Σ(y-ŷ)² / n) | Penalizes large errors |
| **MAPE** | Σ\|y-ŷ\|/y × 100% | Percentage interpretation |

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_ts_arima_init()` | Initialize ARIMA model |
| `eif_ts_arima_fit()` | Estimate AR/MA coefficients |
| `eif_ts_arima_predict()` | One-step forecast |
| `eif_ts_hw_init()` | Initialize Holt-Winters |
| `eif_ts_hw_update()` | Online update with new observation |
| `eif_ts_hw_forecast()` | Multi-step forecast |

## When to Use Each Method

| Scenario | Recommended |
|----------|-------------|
| No clear seasonality | ARIMA |
| Strong seasonal pattern | Holt-Winters |
| Need interpretability | Holt-Winters |
| Automated selection | ARIMA with AIC/BIC |
| Online/streaming | Both support online |

## Real-World Applications
- Smart grid load forecasting
- Retail demand prediction
- Stock price prediction
- Weather forecasting
- Server load prediction

## Run the Demo
```bash
cd build && ./bin/time_series_demo
```
