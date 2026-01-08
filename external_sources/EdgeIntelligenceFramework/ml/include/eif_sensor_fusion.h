/**
 * @file eif_sensor_fusion.h
 * @brief Multi-Sensor Fusion Framework for Edge Intelligence
 *
 * Algorithms for combining data from multiple sensors:
 * - Complementary filter
 * - Weighted sensor voting
 * - Sensor confidence estimation
 * - Fault-tolerant fusion
 *
 * Critical for edge devices with multiple sensing modalities.
 */

#ifndef EIF_SENSOR_FUSION_H
#define EIF_SENSOR_FUSION_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_FUSION_MAX_SENSORS 8

// =============================================================================
// Sensor State
// =============================================================================

/**
 * @brief Individual sensor state
 */
typedef struct {
  float value;             ///< Current sensor value
  float confidence;        ///< Confidence weight (0-1)
  float noise_estimate;    ///< Estimated noise level
  uint32_t last_update_ms; ///< Time of last update
  uint32_t timeout_ms;     ///< Maximum age before stale
  bool is_valid;           ///< Sensor data is valid
  bool is_calibrated;      ///< Sensor is calibrated
} eif_sensor_t;

/**
 * @brief Initialize sensor
 */
static inline void eif_sensor_init(eif_sensor_t *sensor, float noise_est,
                                   uint32_t timeout_ms) {
  sensor->value = 0.0f;
  sensor->confidence = 1.0f;
  sensor->noise_estimate = noise_est;
  sensor->last_update_ms = 0;
  sensor->timeout_ms = timeout_ms;
  sensor->is_valid = false;
  sensor->is_calibrated = false;
}

/**
 * @brief Update sensor value
 */
static inline void eif_sensor_update(eif_sensor_t *sensor, float value,
                                     uint32_t timestamp_ms) {
  sensor->value = value;
  sensor->last_update_ms = timestamp_ms;
  sensor->is_valid = true;
}

/**
 * @brief Check if sensor data is stale
 */
static inline bool eif_sensor_is_stale(eif_sensor_t *sensor,
                                       uint32_t current_time_ms) {
  if (!sensor->is_valid)
    return true;
  return (current_time_ms - sensor->last_update_ms) > sensor->timeout_ms;
}

// =============================================================================
// Complementary Filter
// =============================================================================

/**
 * @brief Complementary filter for fusing fast/noisy and slow/accurate sensors
 *
 * Classic IMU fusion: gyro (fast, drifts) + accelerometer (slow, accurate)
 */
typedef struct {
  float alpha;  ///< Blend factor (0 = favor sensor2, 1 = favor sensor1)
  float output; ///< Fused output
  float dt;     ///< Time step in seconds
} eif_complementary_t;

/**
 * @brief Initialize complementary filter
 */
static inline void eif_complementary_init(eif_complementary_t *cf, float alpha,
                                          float dt) {
  cf->alpha = alpha;
  cf->output = 0.0f;
  cf->dt = dt;
}

/**
 * @brief Update complementary filter
 *
 * @param rate_sensor Fast sensor giving rate (e.g., gyro rate)
 * @param abs_sensor Slow sensor giving absolute value (e.g., accel angle)
 */
static inline float eif_complementary_update(eif_complementary_t *cf,
                                             float rate_sensor,
                                             float abs_sensor) {
  // Integrate rate sensor and blend with absolute sensor
  cf->output = cf->alpha * (cf->output + rate_sensor * cf->dt) +
               (1.0f - cf->alpha) * abs_sensor;
  return cf->output;
}

// =============================================================================
// Weighted Sensor Fusion
// =============================================================================

/**
 * @brief Weighted fusion with automatic confidence adaptation
 */
typedef struct {
  eif_sensor_t sensors[EIF_FUSION_MAX_SENSORS];
  int num_sensors;
  float output;
  float total_confidence;
} eif_weighted_fusion_t;

/**
 * @brief Initialize weighted fusion
 */
static inline void eif_weighted_fusion_init(eif_weighted_fusion_t *wf,
                                            int num_sensors) {
  wf->num_sensors = num_sensors < EIF_FUSION_MAX_SENSORS
                        ? num_sensors
                        : EIF_FUSION_MAX_SENSORS;
  wf->output = 0.0f;
  wf->total_confidence = 0.0f;

  for (int i = 0; i < wf->num_sensors; i++) {
    eif_sensor_init(&wf->sensors[i], 0.1f, 1000);
  }
}

/**
 * @brief Update single sensor in fusion
 */
static inline void eif_weighted_fusion_update_sensor(eif_weighted_fusion_t *wf,
                                                     int sensor_idx,
                                                     float value,
                                                     uint32_t timestamp_ms) {
  if (sensor_idx < 0 || sensor_idx >= wf->num_sensors)
    return;
  eif_sensor_update(&wf->sensors[sensor_idx], value, timestamp_ms);
}

/**
 * @brief Compute fused output
 */
static inline float eif_weighted_fusion_compute(eif_weighted_fusion_t *wf,
                                                uint32_t current_time_ms) {
  float weighted_sum = 0.0f;
  float weight_sum = 0.0f;

  for (int i = 0; i < wf->num_sensors; i++) {
    if (eif_sensor_is_stale(&wf->sensors[i], current_time_ms)) {
      continue;
    }

    float weight = wf->sensors[i].confidence;
    weighted_sum += wf->sensors[i].value * weight;
    weight_sum += weight;
  }

  if (weight_sum > 0.0f) {
    wf->output = weighted_sum / weight_sum;
    wf->total_confidence = weight_sum / wf->num_sensors;
  }

  return wf->output;
}

// =============================================================================
// Sensor Voting (Fault-Tolerant)
// =============================================================================

/**
 * @brief Majority voting configuration
 */
typedef struct {
  float values[EIF_FUSION_MAX_SENSORS];
  float tolerance; ///< Values within tolerance are considered equal
  int num_sensors;
  float voted_value;
  int votes_for_value;
} eif_sensor_voting_t;

/**
 * @brief Initialize voting
 */
static inline void eif_voting_init(eif_sensor_voting_t *sv, int num_sensors,
                                   float tolerance) {
  sv->num_sensors = num_sensors < EIF_FUSION_MAX_SENSORS
                        ? num_sensors
                        : EIF_FUSION_MAX_SENSORS;
  sv->tolerance = tolerance;
  sv->voted_value = 0.0f;
  sv->votes_for_value = 0;
}

/**
 * @brief Update sensor values and compute voted result
 */
static inline float eif_voting_compute(eif_sensor_voting_t *sv,
                                       const float *values, int n) {
  if (n > sv->num_sensors)
    n = sv->num_sensors;

  // Copy values
  for (int i = 0; i < n; i++) {
    sv->values[i] = values[i];
  }

  // Find value with most agreement
  int best_votes = 0;
  float best_value = values[0];

  for (int i = 0; i < n; i++) {
    int votes = 1;
    float sum = values[i];

    for (int j = 0; j < n; j++) {
      if (i != j && fabsf(values[i] - values[j]) <= sv->tolerance) {
        votes++;
        sum += values[j];
      }
    }

    if (votes > best_votes) {
      best_votes = votes;
      // Average of agreeing sensors
      best_value = sum / votes;
    }
  }

  sv->voted_value = best_value;
  sv->votes_for_value = best_votes;

  return sv->voted_value;
}

/**
 * @brief Check if there's consensus
 */
static inline bool eif_voting_has_consensus(eif_sensor_voting_t *sv) {
  return sv->votes_for_value > sv->num_sensors / 2;
}

// =============================================================================
// Simple Kalman-style Sensor Fusion
// =============================================================================

/**
 * @brief Simple 1D Kalman filter for sensor fusion
 */
typedef struct {
  float estimate;       ///< Current estimate
  float error_estimate; ///< Error in estimate
  float error_measure;  ///< Measurement noise
  float q;              ///< Process noise
} eif_kalman_1d_t;

/**
 * @brief Initialize 1D Kalman filter
 */
static inline void eif_kalman_1d_init(eif_kalman_1d_t *kf,
                                      float initial_estimate,
                                      float error_estimate, float error_measure,
                                      float process_noise) {
  kf->estimate = initial_estimate;
  kf->error_estimate = error_estimate;
  kf->error_measure = error_measure;
  kf->q = process_noise;
}

/**
 * @brief Update Kalman filter with new measurement
 */
static inline float eif_kalman_1d_update(eif_kalman_1d_t *kf,
                                         float measurement) {
  // Prediction step
  kf->error_estimate += kf->q;

  // Update step
  float gain = kf->error_estimate / (kf->error_estimate + kf->error_measure);
  kf->estimate += gain * (measurement - kf->estimate);
  kf->error_estimate *= (1.0f - gain);

  return kf->estimate;
}

/**
 * @brief Fuse two sensors with different noise characteristics
 */
static inline float eif_kalman_fuse_two(float val1, float noise1, float val2,
                                        float noise2) {
  float var1 = noise1 * noise1;
  float var2 = noise2 * noise2;
  float gain = var1 / (var1 + var2);
  return val1 + gain * (val2 - val1);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_SENSOR_FUSION_H
