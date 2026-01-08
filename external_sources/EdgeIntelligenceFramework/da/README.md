# Data Analysis Module

The **Data Analysis Module** provides tools for statistical analysis, online learning, and time series forecasting.

## Features

*   **Online Learning**:
    *   **Incremental K-Means**: Clustering data streams in real-time.
    *   **Online Linear Regression**: Updating regression models with new data points (Recursive Least Squares).
    *   **Online Anomaly Detection**: Z-score based outlier detection for streaming data.
*   **Time Series Analysis**:
    *   **ARIMA**: AutoRegressive Integrated Moving Average for forecasting.
    *   **Holt-Winters**: Exponential smoothing with trend and seasonality support.
*   **Basic Statistics**:
    *   Mean, Variance, Standard Deviation.
    *   Min/Max, Normalization (MinMax, Z-Score).
    *   Distance Metrics (Euclidean, Manhattan, Cosine).

## Usage

### Online Linear Regression
```c
#include "eif_data_analysis.h"

eif_linreg_online_t lr;
eif_linreg_online_init(&lr);

// Update with new point (x, y)
eif_linreg_online_update(&lr, x, y);

// Predict
float32_t y_pred = eif_linreg_online_predict(&lr, x_new);
```

### Time Series Forecasting (ARIMA)
```c
#include "eif_timeseries.h"

eif_ts_arima_t arima;
eif_ts_arima_init(&arima, 1, 0, 0, &pool); // AR(1)

// Predict next value
float32_t prediction;
eif_ts_arima_predict(&arima, current_value, &prediction);
```
