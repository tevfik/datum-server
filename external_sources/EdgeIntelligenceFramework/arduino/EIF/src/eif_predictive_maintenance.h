/**
 * @file eif_predictive_maintenance.h
 * @brief Predictive Maintenance Utilities
 *
 * Algorithms for condition monitoring and fault prediction:
 * - Remaining Useful Life (RUL) estimation
 * - Health indicator calculation
 * - Degradation modeling
 * - Threshold-based alerts
 *
 * Essential for industrial IoT and edge predictive maintenance.
 */

#ifndef EIF_PREDICTIVE_MAINTENANCE_H
#define EIF_PREDICTIVE_MAINTENANCE_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_PM_MAX_FEATURES 16
#define EIF_PM_HISTORY_SIZE 100

// =============================================================================
// Health Indicators
// =============================================================================

typedef enum {
  EIF_HEALTH_GOOD = 0,
  EIF_HEALTH_FAIR,
  EIF_HEALTH_WARNING,
  EIF_HEALTH_CRITICAL,
  EIF_HEALTH_FAILURE
} eif_health_state_t;

/**
 * @brief Health indicator with trend tracking
 */
typedef struct {
  float current_value;
  float baseline;
  float threshold_warning;
  float threshold_critical;
  float trend_slope; ///< Rate of change
  float alpha;       ///< EMA smoothing factor
  int samples_count;
  eif_health_state_t state;
} eif_health_indicator_t;

/**
 * @brief Initialize health indicator
 */
static inline void eif_health_init(eif_health_indicator_t *hi, float baseline,
                                   float warning_threshold,
                                   float critical_threshold) {
  hi->current_value = baseline;
  hi->baseline = baseline;
  hi->threshold_warning = warning_threshold;
  hi->threshold_critical = critical_threshold;
  hi->trend_slope = 0.0f;
  hi->alpha = 0.1f;
  hi->samples_count = 0;
  hi->state = EIF_HEALTH_GOOD;
}

/**
 * @brief Update health indicator
 */
static inline eif_health_state_t eif_health_update(eif_health_indicator_t *hi,
                                                   float new_value) {
  float prev_value = hi->current_value;

  // Smooth update
  hi->current_value =
      (1.0f - hi->alpha) * hi->current_value + hi->alpha * new_value;

  // Trend estimation
  float instant_slope = hi->current_value - prev_value;
  hi->trend_slope =
      (1.0f - hi->alpha) * hi->trend_slope + hi->alpha * instant_slope;

  hi->samples_count++;

  // Determine health state
  float deviation = fabsf(hi->current_value - hi->baseline) / hi->baseline;

  if (deviation < hi->threshold_warning) {
    hi->state = EIF_HEALTH_GOOD;
  } else if (deviation < hi->threshold_critical) {
    hi->state = hi->trend_slope > 0 ? EIF_HEALTH_WARNING : EIF_HEALTH_FAIR;
  } else {
    hi->state = EIF_HEALTH_CRITICAL;
  }

  return hi->state;
}

/**
 * @brief Get normalized health (0 = failure, 1 = perfect)
 */
static inline float eif_health_get_normalized(eif_health_indicator_t *hi) {
  float deviation = fabsf(hi->current_value - hi->baseline) / hi->baseline;
  float health = 1.0f - (deviation / hi->threshold_critical);
  if (health < 0.0f)
    health = 0.0f;
  if (health > 1.0f)
    health = 1.0f;
  return health;
}

// =============================================================================
// Remaining Useful Life (RUL) Estimation
// =============================================================================

/**
 * @brief Simple linear RUL estimator
 */
typedef struct {
  float history[EIF_PM_HISTORY_SIZE];
  int history_idx;
  int history_count;
  float failure_threshold;
  float sampling_interval; ///< Time between samples (hours, days, etc.)
} eif_rul_estimator_t;

/**
 * @brief Initialize RUL estimator
 */
static inline void eif_rul_init(eif_rul_estimator_t *rul,
                                float failure_threshold,
                                float sampling_interval) {
  rul->history_idx = 0;
  rul->history_count = 0;
  rul->failure_threshold = failure_threshold;
  rul->sampling_interval = sampling_interval;
}

/**
 * @brief Update RUL with new degradation value
 */
static inline void eif_rul_update(eif_rul_estimator_t *rul, float value) {
  rul->history[rul->history_idx] = value;
  rul->history_idx = (rul->history_idx + 1) % EIF_PM_HISTORY_SIZE;
  if (rul->history_count < EIF_PM_HISTORY_SIZE) {
    rul->history_count++;
  }
}

/**
 * @brief Estimate RUL using linear extrapolation
 * @return Estimated time to failure in same units as sampling_interval
 */
static inline float eif_rul_estimate(eif_rul_estimator_t *rul) {
  if (rul->history_count < 10) {
    return -1.0f; // Not enough data
  }

  // Simple linear regression
  float sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
  int n = rul->history_count;
  int start =
      (rul->history_idx - n + EIF_PM_HISTORY_SIZE) % EIF_PM_HISTORY_SIZE;

  for (int i = 0; i < n; i++) {
    int idx = (start + i) % EIF_PM_HISTORY_SIZE;
    float x = (float)i;
    float y = rul->history[idx];

    sum_x += x;
    sum_y += y;
    sum_xy += x * y;
    sum_xx += x * x;
  }

  float slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
  float intercept = (sum_y - slope * sum_x) / n;

  if (slope <= 0) {
    return -1.0f; // Improving or stable, no failure predicted
  }

  // Time to reach failure threshold
  float current = intercept + slope * (n - 1);
  float remaining_degradation = rul->failure_threshold - current;
  float samples_to_failure = remaining_degradation / slope;

  if (samples_to_failure < 0) {
    return 0.0f; // Already at/past threshold
  }

  return samples_to_failure * rul->sampling_interval;
}

// =============================================================================
// Vibration-Based Condition Monitoring
// =============================================================================

/**
 * @brief Vibration metrics for rotating machinery
 */
typedef struct {
  float rms;          ///< Root Mean Square
  float peak;         ///< Maximum absolute value
  float crest_factor; ///< Peak / RMS
  float kurtosis;     ///< Peakedness (4th moment)
  float skewness;     ///< Asymmetry (3rd moment)
} eif_vibration_metrics_t;

/**
 * @brief Calculate vibration metrics
 */
static inline void eif_vibration_analyze(const float *samples, int n,
                                         eif_vibration_metrics_t *metrics) {
  // Mean
  float mean = 0;
  for (int i = 0; i < n; i++)
    mean += samples[i];
  mean /= n;

  // RMS and peak
  float sum_sq = 0;
  float peak = 0;
  for (int i = 0; i < n; i++) {
    float v = samples[i] - mean;
    sum_sq += v * v;
    if (fabsf(v) > peak)
      peak = fabsf(v);
  }
  metrics->rms = sqrtf(sum_sq / n);
  metrics->peak = peak;
  metrics->crest_factor = peak / (metrics->rms + 1e-10f);

  // Higher moments
  float m3 = 0, m4 = 0;
  float std = metrics->rms;
  for (int i = 0; i < n; i++) {
    float z = (samples[i] - mean) / (std + 1e-10f);
    float z2 = z * z;
    m3 += z2 * z;
    m4 += z2 * z2;
  }
  metrics->skewness = m3 / n;
  metrics->kurtosis = m4 / n - 3.0f; // Excess kurtosis
}

/**
 * @brief Interpret vibration metrics for bearing health
 */
static inline eif_health_state_t
eif_vibration_diagnose(eif_vibration_metrics_t *m) {
  // Typical thresholds for bearing health
  // These should be calibrated for specific machinery

  if (m->crest_factor > 6.0f || m->kurtosis > 5.0f) {
    return EIF_HEALTH_CRITICAL; // Severe peaking indicates damage
  }

  if (m->crest_factor > 4.5f || m->kurtosis > 3.0f) {
    return EIF_HEALTH_WARNING;
  }

  if (m->rms > 2.0f) { // Example threshold in mm/s
    return EIF_HEALTH_FAIR;
  }

  return EIF_HEALTH_GOOD;
}

// =============================================================================
// Maintenance Scheduling
// =============================================================================

/**
 * @brief Maintenance recommendation
 */
typedef struct {
  eif_health_state_t health;
  float rul_estimate;
  int urgency; ///< 0 = routine, 5 = immediate
  const char *action;
} eif_maintenance_rec_t;

/**
 * @brief Generate maintenance recommendation
 */
static inline void eif_maintenance_recommend(eif_health_indicator_t *health,
                                             eif_rul_estimator_t *rul,
                                             eif_maintenance_rec_t *rec) {
  rec->health = health->state;
  rec->rul_estimate = eif_rul_estimate(rul);

  if (health->state == EIF_HEALTH_CRITICAL) {
    rec->urgency = 5;
    rec->action = "Immediate replacement required";
  } else if (health->state == EIF_HEALTH_WARNING) {
    rec->urgency = 3;
    if (rec->rul_estimate > 0 && rec->rul_estimate < 24) {
      rec->action = "Schedule maintenance within 24 hours";
    } else {
      rec->action = "Schedule maintenance this week";
    }
  } else if (health->state == EIF_HEALTH_FAIR) {
    rec->urgency = 2;
    rec->action = "Monitor closely, plan maintenance";
  } else {
    rec->urgency = 0;
    rec->action = "Continue normal operation";
  }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_PREDICTIVE_MAINTENANCE_H
