/**
 * @file eif_adaptive_threshold.h
 * @brief Adaptive Threshold Algorithms for Edge Intelligence
 *
 * Self-tuning thresholds that adapt to changing conditions:
 * - Percentile-based thresholds
 * - Moving window statistics
 * - Automatic outlier detection
 * - Context-aware adaptation
 *
 * Essential for edge devices operating in dynamic environments.
 */

#ifndef EIF_ADAPTIVE_THRESHOLD_H
#define EIF_ADAPTIVE_THRESHOLD_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_ADAPT_MAX_WINDOW 256

// =============================================================================
// Running Statistics
// =============================================================================

/**
 * @brief Running statistics with exponential weighting
 */
typedef struct {
  float mean;
  float variance;
  float min_val;
  float max_val;
  float alpha; ///< Smoothing factor (0-1)
  int64_t count;
  bool initialized;
} eif_running_stats_t;

/**
 * @brief Initialize running statistics
 */
static inline void eif_running_stats_init(eif_running_stats_t *stats,
                                          float alpha) {
  stats->mean = 0.0f;
  stats->variance = 0.0f;
  stats->min_val = 0.0f;
  stats->max_val = 0.0f;
  stats->alpha = alpha;
  stats->count = 0;
  stats->initialized = false;
}

/**
 * @brief Update running statistics with new value
 */
static inline void eif_running_stats_update(eif_running_stats_t *stats,
                                            float value) {
  if (!stats->initialized) {
    stats->mean = value;
    stats->variance = 0.0f;
    stats->min_val = value;
    stats->max_val = value;
    stats->initialized = true;
    stats->count = 1;
    return;
  }

  stats->count++;

  // Exponential moving average for mean
  float delta = value - stats->mean;
  stats->mean += stats->alpha * delta;

  // Running variance (Welford-like with EMA)
  float delta2 = value - stats->mean;
  stats->variance =
      (1.0f - stats->alpha) * stats->variance + stats->alpha * delta * delta2;

  // Track range
  if (value < stats->min_val)
    stats->min_val = value;
  if (value > stats->max_val)
    stats->max_val = value;
}

/**
 * @brief Get standard deviation
 */
static inline float eif_running_stats_std(eif_running_stats_t *stats) {
  return sqrtf(stats->variance);
}

// =============================================================================
// Adaptive Z-Score Threshold
// =============================================================================

/**
 * @brief Adaptive threshold based on Z-score
 */
typedef struct {
  eif_running_stats_t stats;
  float z_threshold; ///< Number of standard deviations
  float min_std;     ///< Minimum std to prevent false positives
} eif_z_threshold_t;

/**
 * @brief Initialize Z-score threshold
 */
static inline void eif_z_threshold_init(eif_z_threshold_t *zt, float alpha,
                                        float z_threshold) {
  eif_running_stats_init(&zt->stats, alpha);
  zt->z_threshold = z_threshold;
  zt->min_std = 0.01f;
}

/**
 * @brief Update and check if value exceeds adaptive threshold
 */
static inline bool eif_z_threshold_check(eif_z_threshold_t *zt, float value) {
  if (!zt->stats.initialized) {
    eif_running_stats_update(&zt->stats, value);
    return false;
  }

  float std = fmaxf(eif_running_stats_std(&zt->stats), zt->min_std);
  float z_score = fabsf(value - zt->stats.mean) / std;

  bool exceeded = z_score > zt->z_threshold;

  // Only update stats with normal values (prevents threshold drift)
  if (!exceeded) {
    eif_running_stats_update(&zt->stats, value);
  }

  return exceeded;
}

/**
 * @brief Get current threshold values
 */
static inline void eif_z_threshold_get_bounds(eif_z_threshold_t *zt,
                                              float *lower, float *upper) {
  float std = fmaxf(eif_running_stats_std(&zt->stats), zt->min_std);
  *lower = zt->stats.mean - zt->z_threshold * std;
  *upper = zt->stats.mean + zt->z_threshold * std;
}

// =============================================================================
// Percentile-Based Adaptive Threshold
// =============================================================================

/**
 * @brief Sliding window for percentile calculation
 */
typedef struct {
  float buffer[EIF_ADAPT_MAX_WINDOW];
  int size;
  int idx;
  int count;
  float percentile_low;  ///< e.g., 0.05 for 5th percentile
  float percentile_high; ///< e.g., 0.95 for 95th percentile
} eif_percentile_threshold_t;

/**
 * @brief Initialize percentile threshold
 */
static inline void eif_percentile_threshold_init(eif_percentile_threshold_t *pt,
                                                 int window_size,
                                                 float percentile_low,
                                                 float percentile_high) {
  pt->size =
      window_size < EIF_ADAPT_MAX_WINDOW ? window_size : EIF_ADAPT_MAX_WINDOW;
  pt->idx = 0;
  pt->count = 0;
  pt->percentile_low = percentile_low;
  pt->percentile_high = percentile_high;
}

/**
 * @brief Simple insertion sort for small arrays
 */
static inline void sort_floats(float *arr, int n) {
  for (int i = 1; i < n; i++) {
    float key = arr[i];
    int j = i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

/**
 * @brief Update and check if value exceeds percentile bounds
 */
static inline bool
eif_percentile_threshold_check(eif_percentile_threshold_t *pt, float value,
                               float *lower, float *upper) {
  // Add to buffer
  pt->buffer[pt->idx] = value;
  pt->idx = (pt->idx + 1) % pt->size;
  if (pt->count < pt->size)
    pt->count++;

  if (pt->count < 3) {
    *lower = value - 1.0f;
    *upper = value + 1.0f;
    return false;
  }

  // Sort copy of buffer
  float sorted[EIF_ADAPT_MAX_WINDOW];
  for (int i = 0; i < pt->count; i++) {
    sorted[i] = pt->buffer[i];
  }
  sort_floats(sorted, pt->count);

  // Calculate percentiles
  int low_idx = (int)(pt->percentile_low * (pt->count - 1));
  int high_idx = (int)(pt->percentile_high * (pt->count - 1));

  *lower = sorted[low_idx];
  *upper = sorted[high_idx];

  return (value < *lower || value > *upper);
}

// =============================================================================
// Concept Drift Detection (ADWIN-inspired)
// =============================================================================

/**
 * @brief Simple drift detector based on mean shift
 */
typedef struct {
  float window1_mean;
  float window2_mean;
  float alpha;
  float threshold;
  int window_count;
  bool drift_detected;
} eif_drift_detector_t;

/**
 * @brief Initialize drift detector
 */
static inline void eif_drift_init(eif_drift_detector_t *dd, float alpha,
                                  float threshold) {
  dd->window1_mean = 0.0f;
  dd->window2_mean = 0.0f;
  dd->alpha = alpha;
  dd->threshold = threshold;
  dd->window_count = 0;
  dd->drift_detected = false;
}

/**
 * @brief Update and check for concept drift
 */
static inline bool eif_drift_update(eif_drift_detector_t *dd, float value) {
  dd->window_count++;

  // Update windows with different learning rates
  float alpha_fast = dd->alpha * 2.0f; // Faster window
  float alpha_slow = dd->alpha * 0.5f; // Slower window

  if (dd->window_count == 1) {
    dd->window1_mean = value;
    dd->window2_mean = value;
    return false;
  }

  dd->window1_mean =
      (1.0f - alpha_slow) * dd->window1_mean + alpha_slow * value;
  dd->window2_mean =
      (1.0f - alpha_fast) * dd->window2_mean + alpha_fast * value;

  // Check for significant divergence
  float diff = fabsf(dd->window1_mean - dd->window2_mean);
  dd->drift_detected = diff > dd->threshold;

  return dd->drift_detected;
}

/**
 * @brief Reset drift detector after adaptation
 */
static inline void eif_drift_reset(eif_drift_detector_t *dd) {
  dd->window1_mean = dd->window2_mean;
  dd->window_count = 0;
  dd->drift_detected = false;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ADAPTIVE_THRESHOLD_H
