/**
 * @file eif_ts.h
 * @brief Time Series Algorithms
 */

#ifndef EIF_TS_H
#define EIF_TS_H

#include "eif_memory.h"
#include "eif_status.h"
#include "eif_types.h"

// ============================================================================
// ARIMA (AutoRegressive Integrated Moving Average)
// ============================================================================

typedef struct {
  int p; // AR order
  int d; // Differencing order
  int q; // MA order

  // Coefficients
  float32_t *ar_coeffs; // [p]
  float32_t *ma_coeffs; // [q]

  // State
  float32_t *history;      // Circular buffer for AR [p]
  float32_t *errors;       // Circular buffer for MA [q]
  float32_t *diff_history; // For integration [d] (stores last values)

  int history_idx;
  int error_idx;

} eif_ts_arima_t;

eif_status_t eif_ts_arima_init(eif_ts_arima_t *model, int p, int d, int q,
                               eif_memory_pool_t *pool);
eif_status_t eif_ts_arima_predict(eif_ts_arima_t *model, float32_t input,
                                  float32_t *prediction);
eif_status_t eif_ts_arima_fit(eif_ts_arima_t *model, const float32_t *data,
                              int length);

// ============================================================================
// Holt-Winters Exponential Smoothing
// ============================================================================

typedef enum { EIF_TS_HW_ADDITIVE, EIF_TS_HW_MULTIPLICATIVE } eif_ts_hw_type_t;

typedef struct {
  eif_ts_hw_type_t type;
  int season_length;

  float32_t alpha; // Level smoothing
  float32_t beta;  // Trend smoothing
  float32_t gamma; // Seasonal smoothing

  // State
  float32_t level;
  float32_t trend;
  float32_t *seasonals; // [season_length]

  bool initialized;

} eif_ts_hw_t;

eif_status_t eif_ts_hw_init(eif_ts_hw_t *model, int season_length,
                            eif_ts_hw_type_t type, eif_memory_pool_t *pool);
eif_status_t eif_ts_hw_update(eif_ts_hw_t *model, float32_t input);
eif_status_t eif_ts_hw_forecast(const eif_ts_hw_t *model, int steps,
                                float32_t *forecast);

// ============================================================================
// Seasonal Decomposition (STL-like simplified)
// ============================================================================

typedef struct {
  int season_length;   // Length of seasonal cycle
  float32_t *trend;    // Trend component
  float32_t *seasonal; // Seasonal component
  float32_t *residual; // Residual component
  int length;          // Data length
} eif_ts_decomposition_t;

eif_status_t eif_ts_decompose(const float32_t *data, int length,
                              int season_length, eif_ts_decomposition_t *result,
                              eif_memory_pool_t *pool);

// ============================================================================
// Change Point Detection (CUSUM-based)
// ============================================================================

typedef struct {
  float32_t threshold;   // Detection threshold
  float32_t drift;       // Allowable drift
  float32_t mean;        // Running mean
  float32_t sum_pos;     // Positive CUSUM
  float32_t sum_neg;     // Negative CUSUM
  int count;             // Sample count
  int last_change_point; // Last detected change point index
} eif_changepoint_t;

eif_status_t eif_changepoint_init(eif_changepoint_t *detector,
                                  float32_t threshold, float32_t drift);
int eif_changepoint_update(eif_changepoint_t *detector,
                           float32_t value); // Returns 1 if change detected
eif_status_t eif_changepoint_detect(const float32_t *data, int length,
                                    float32_t threshold, int *change_points,
                                    int *num_changes, int max_changes);

// ============================================================================
// Smoothing and Trend Extraction
// ============================================================================

eif_status_t eif_ts_moving_average(const float32_t *data, int length,
                                   int window, float32_t *output);
eif_status_t eif_ts_exponential_smoothing(const float32_t *data, int length,
                                          float32_t alpha, float32_t *output);

// ============================================================================
// Autocorrelation Analysis
// ============================================================================

eif_status_t eif_ts_acf(const float32_t *data, int length, float32_t *acf,
                        int max_lag);
eif_status_t eif_ts_pacf(const float32_t *data, int length, float32_t *pacf,
                         int max_lag);

// ============================================================================
// Correlation Analysis
// ============================================================================

float32_t eif_correlation_pearson(const float32_t *x, const float32_t *y,
                                  int length);
float32_t eif_correlation_spearman(const float32_t *x, const float32_t *y,
                                   int length, eif_memory_pool_t *pool);
eif_status_t eif_cross_correlation(const float32_t *x, const float32_t *y,
                                   int length, float32_t *result, int max_lag);

// ============================================================================
// Dynamic Time Warping (DTW)
// ============================================================================

/**
 * @brief Compute DTW distance between two sequences
 *
 * @param s1 First sequence
 * @param len1 Length of first sequence
 * @param s2 Second sequence
 * @param len2 Length of second sequence
 * @param window Window size (0 for no window/infinity) - Sakoe-Chiba band
 * @param pool Memory pool for cost matrix
 * @return float32_t DTW distance
 */
float32_t eif_ts_dtw_compute(const float32_t *s1, int len1, const float32_t *s2,
                             int len2, int window, eif_memory_pool_t *pool);

#endif // EIF_TS_H
