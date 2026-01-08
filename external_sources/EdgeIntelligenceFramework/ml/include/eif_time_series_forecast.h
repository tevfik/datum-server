/**
 * @file eif_time_series.h
 * @brief Time Series Forecasting for Edge AI
 *
 * Lightweight forecasting algorithms:
 * - Exponential smoothing (Simple, Holt, Holt-Winters)
 * - Moving average models
 * - Seasonal decomposition
 * - Change point detection
 *
 * Designed for resource-constrained edge devices.
 */

#ifndef EIF_TIME_SERIES_H
#define EIF_TIME_SERIES_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_TS_MAX_SEASONAL 52 // Max seasonal period
#define EIF_TS_HISTORY_SIZE 128

// =============================================================================
// Simple Exponential Smoothing
// =============================================================================

/**
 * @brief Simple exponential smoothing
 */
typedef struct {
  float level; ///< Current level estimate
  float alpha; ///< Smoothing factor
  bool initialized;
} eif_ses_t;

/**
 * @brief Initialize SES
 */
static inline void eif_ses_init(eif_ses_t *ses, float alpha) {
  ses->alpha = alpha;
  ses->level = 0.0f;
  ses->initialized = false;
}

/**
 * @brief Update SES and get forecast
 */
static inline float eif_ses_update(eif_ses_t *ses, float value) {
  if (!ses->initialized) {
    ses->level = value;
    ses->initialized = true;
  } else {
    ses->level = ses->alpha * value + (1 - ses->alpha) * ses->level;
  }
  return ses->level;
}

/**
 * @brief Get n-step ahead forecast (all same for SES)
 */
static inline float eif_ses_forecast(eif_ses_t *ses, int steps) {
  (void)steps; // All forecasts same for SES
  return ses->level;
}

// =============================================================================
// Holt's Linear Trend
// =============================================================================

/**
 * @brief Holt's double exponential smoothing (level + trend)
 */
typedef struct {
  float level;
  float trend;
  float alpha; ///< Level smoothing
  float beta;  ///< Trend smoothing
  bool initialized;
} eif_holt_t;

/**
 * @brief Initialize Holt's method
 */
static inline void eif_holt_init(eif_holt_t *holt, float alpha, float beta) {
  holt->alpha = alpha;
  holt->beta = beta;
  holt->level = 0.0f;
  holt->trend = 0.0f;
  holt->initialized = false;
}

/**
 * @brief Update Holt's method
 */
static inline void eif_holt_update(eif_holt_t *holt, float value) {
  if (!holt->initialized) {
    holt->level = value;
    holt->trend = 0.0f;
    holt->initialized = true;
  } else {
    float prev_level = holt->level;
    holt->level =
        holt->alpha * value + (1 - holt->alpha) * (holt->level + holt->trend);
    holt->trend = holt->beta * (holt->level - prev_level) +
                  (1 - holt->beta) * holt->trend;
  }
}

/**
 * @brief Get n-step ahead forecast
 */
static inline float eif_holt_forecast(eif_holt_t *holt, int steps) {
  return holt->level + steps * holt->trend;
}

// =============================================================================
// Holt-Winters Seasonal
// =============================================================================

/**
 * @brief Holt-Winters triple exponential smoothing (additive seasonal)
 */
typedef struct {
  float level;
  float trend;
  float seasonal[EIF_TS_MAX_SEASONAL];
  float alpha;
  float beta;
  float gamma; ///< Seasonal smoothing
  int period;  ///< Seasonal period
  int idx;     ///< Current position in season
  int samples_count;
  bool initialized;
} eif_holt_winters_t;

/**
 * @brief Initialize Holt-Winters
 */
static inline void eif_holt_winters_init(eif_holt_winters_t *hw, float alpha,
                                         float beta, float gamma, int period) {
  hw->alpha = alpha;
  hw->beta = beta;
  hw->gamma = gamma;
  hw->period = period < EIF_TS_MAX_SEASONAL ? period : EIF_TS_MAX_SEASONAL;
  hw->level = 0.0f;
  hw->trend = 0.0f;
  hw->idx = 0;
  hw->samples_count = 0;
  hw->initialized = false;

  for (int i = 0; i < hw->period; i++) {
    hw->seasonal[i] = 0.0f;
  }
}

/**
 * @brief Update Holt-Winters
 */
static inline void eif_holt_winters_update(eif_holt_winters_t *hw,
                                           float value) {
  if (hw->samples_count < hw->period) {
    // Collect first season for initialization
    hw->seasonal[hw->samples_count] = value;
    hw->samples_count++;

    if (hw->samples_count == hw->period) {
      // Initialize after first season
      float sum = 0;
      for (int i = 0; i < hw->period; i++) {
        sum += hw->seasonal[i];
      }
      hw->level = sum / hw->period;
      hw->trend = 0;

      // Seasonal factors
      for (int i = 0; i < hw->period; i++) {
        hw->seasonal[i] = hw->seasonal[i] - hw->level;
      }
      hw->initialized = true;
    }
  } else {
    float prev_level = hw->level;
    float s = hw->seasonal[hw->idx];

    // Update level
    hw->level =
        hw->alpha * (value - s) + (1 - hw->alpha) * (hw->level + hw->trend);

    // Update trend
    hw->trend =
        hw->beta * (hw->level - prev_level) + (1 - hw->beta) * hw->trend;

    // Update seasonal
    hw->seasonal[hw->idx] =
        hw->gamma * (value - hw->level) + (1 - hw->gamma) * s;

    hw->samples_count++;
  }

  hw->idx = (hw->idx + 1) % hw->period;
}

/**
 * @brief Get n-step ahead forecast
 */
static inline float eif_holt_winters_forecast(eif_holt_winters_t *hw,
                                              int steps) {
  if (!hw->initialized)
    return 0.0f;

  int future_idx = (hw->idx + steps - 1) % hw->period;
  return hw->level + steps * hw->trend + hw->seasonal[future_idx];
}

// =============================================================================
// Moving Average
// =============================================================================

/**
 * @brief Simple moving average
 */
typedef struct {
  float buffer[EIF_TS_HISTORY_SIZE];
  int size;
  int idx;
  int count;
  float sum;
} eif_ma_ts_t;

/**
 * @brief Initialize moving average
 */
static inline void eif_ma_ts_init(eif_ma_ts_t *ma, int window_size) {
  ma->size =
      window_size < EIF_TS_HISTORY_SIZE ? window_size : EIF_TS_HISTORY_SIZE;
  ma->idx = 0;
  ma->count = 0;
  ma->sum = 0.0f;
}

/**
 * @brief Update and get MA value
 */
static inline float eif_ma_ts_update(eif_ma_ts_t *ma, float value) {
  if (ma->count >= ma->size) {
    ma->sum -= ma->buffer[ma->idx];
  }

  ma->buffer[ma->idx] = value;
  ma->sum += value;
  ma->idx = (ma->idx + 1) % ma->size;

  if (ma->count < ma->size) {
    ma->count++;
  }

  return ma->sum / ma->count;
}

// =============================================================================
// Change Point Detection
// =============================================================================

/**
 * @brief CUSUM change point detector
 */
typedef struct {
  float target;    ///< Expected mean
  float threshold; ///< Detection threshold
  float slack;     ///< Allowable slack
  float pos_cusum; ///< Positive CUSUM
  float neg_cusum; ///< Negative CUSUM
  bool change_detected;
} eif_cusum_t;

/**
 * @brief Initialize CUSUM detector
 */
static inline void eif_cusum_init(eif_cusum_t *cs, float target,
                                  float threshold, float slack) {
  cs->target = target;
  cs->threshold = threshold;
  cs->slack = slack;
  cs->pos_cusum = 0.0f;
  cs->neg_cusum = 0.0f;
  cs->change_detected = false;
}

/**
 * @brief Update CUSUM and detect change
 */
static inline bool eif_cusum_update(eif_cusum_t *cs, float value) {
  float deviation = value - cs->target;

  // Positive shift detection
  cs->pos_cusum = fmaxf(0.0f, cs->pos_cusum + deviation - cs->slack);

  // Negative shift detection
  cs->neg_cusum = fmaxf(0.0f, cs->neg_cusum - deviation - cs->slack);

  cs->change_detected =
      (cs->pos_cusum > cs->threshold) || (cs->neg_cusum > cs->threshold);

  return cs->change_detected;
}

/**
 * @brief Reset CUSUM after detected change
 */
static inline void eif_cusum_reset(eif_cusum_t *cs, float new_target) {
  cs->target = new_target;
  cs->pos_cusum = 0.0f;
  cs->neg_cusum = 0.0f;
  cs->change_detected = false;
}

// =============================================================================
// Forecast Accuracy Metrics
// =============================================================================

/**
 * @brief Calculate Mean Absolute Error
 */
static inline float eif_forecast_mae(const float *actual, const float *forecast,
                                     int n) {
  float sum = 0.0f;
  for (int i = 0; i < n; i++) {
    sum += fabsf(actual[i] - forecast[i]);
  }
  return sum / n;
}

/**
 * @brief Calculate Mean Absolute Percentage Error
 */
static inline float eif_forecast_mape(const float *actual,
                                      const float *forecast, int n) {
  float sum = 0.0f;
  int valid = 0;
  for (int i = 0; i < n; i++) {
    if (fabsf(actual[i]) > 1e-6f) {
      sum += fabsf((actual[i] - forecast[i]) / actual[i]);
      valid++;
    }
  }
  return valid > 0 ? 100.0f * sum / valid : 0.0f;
}

/**
 * @brief Calculate Root Mean Square Error
 */
static inline float eif_forecast_rmse(const float *actual,
                                      const float *forecast, int n) {
  float sum_sq = 0.0f;
  for (int i = 0; i < n; i++) {
    float diff = actual[i] - forecast[i];
    sum_sq += diff * diff;
  }
  return sqrtf(sum_sq / n);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_TIME_SERIES_H
