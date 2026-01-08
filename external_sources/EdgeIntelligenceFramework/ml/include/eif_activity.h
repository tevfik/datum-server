/**
 * @file eif_activity.h
 * @brief Human Activity Recognition for Edge AI
 *
 * Features and classifiers for IMU-based activity recognition:
 * - Time-domain features
 * - Activity-specific features
 * - Lightweight classification
 *
 * Perfect for wearables, fitness trackers, gesture recognition.
 */

#ifndef EIF_ACTIVITY_H
#define EIF_ACTIVITY_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_ACTIVITY_WINDOW_SIZE 128
#define EIF_ACTIVITY_NUM_FEATURES 15

// =============================================================================
// Activity Types
// =============================================================================

typedef enum {
  EIF_ACTIVITY_UNKNOWN = 0,
  EIF_ACTIVITY_STATIONARY,
  EIF_ACTIVITY_WALKING,
  EIF_ACTIVITY_RUNNING,
  EIF_ACTIVITY_CYCLING,
  EIF_ACTIVITY_STAIRS_UP,
  EIF_ACTIVITY_STAIRS_DOWN,
  EIF_ACTIVITY_DRIVING,
  EIF_NUM_ACTIVITIES
} eif_activity_t;

/**
 * @brief Activity names for display
 */
static const char *eif_activity_names[] = {
    "Unknown", "Stationary", "Walking",     "Running",
    "Cycling", "Stairs Up",  "Stairs Down", "Driving"};

// =============================================================================
// Feature Extraction
// =============================================================================

/**
 * @brief 3-axis accelerometer sample
 */
typedef struct {
  float x, y, z;
} eif_accel_sample_t;

/**
 * @brief Activity features
 */
typedef struct {
  float mean_x, mean_y, mean_z;
  float std_x, std_y, std_z;
  float magnitude_mean;
  float magnitude_std;
  float sma; ///< Signal Magnitude Area
  float max_magnitude;
  float min_magnitude;
  float energy;
  float zero_crossings; ///< Normalized count
  float peak_frequency; ///< Dominant frequency
  float entropy;        ///< Signal entropy
} eif_activity_features_t;

/**
 * @brief Calculate magnitude: sqrt(x^2 + y^2 + z^2)
 */
static inline float eif_accel_magnitude(eif_accel_sample_t *s) {
  return sqrtf(s->x * s->x + s->y * s->y + s->z * s->z);
}

/**
 * @brief Extract activity features from window
 */
static inline void
eif_activity_extract_features(const eif_accel_sample_t *samples,
                              int window_size,
                              eif_activity_features_t *features) {
  // Calculate means
  float sum_x = 0, sum_y = 0, sum_z = 0;
  float sum_mag = 0;
  float max_mag = 0, min_mag = 1e10f;
  float sma = 0;
  float energy = 0;

  for (int i = 0; i < window_size; i++) {
    sum_x += samples[i].x;
    sum_y += samples[i].y;
    sum_z += samples[i].z;

    float mag =
        sqrtf(samples[i].x * samples[i].x + samples[i].y * samples[i].y +
              samples[i].z * samples[i].z);
    sum_mag += mag;

    sma += fabsf(samples[i].x) + fabsf(samples[i].y) + fabsf(samples[i].z);
    energy += samples[i].x * samples[i].x + samples[i].y * samples[i].y +
              samples[i].z * samples[i].z;

    if (mag > max_mag)
      max_mag = mag;
    if (mag < min_mag)
      min_mag = mag;
  }

  features->mean_x = sum_x / window_size;
  features->mean_y = sum_y / window_size;
  features->mean_z = sum_z / window_size;
  features->magnitude_mean = sum_mag / window_size;
  features->sma = sma / window_size;
  features->max_magnitude = max_mag;
  features->min_magnitude = min_mag;
  features->energy = energy / window_size;

  // Calculate standard deviations
  float var_x = 0, var_y = 0, var_z = 0, var_mag = 0;
  int zero_cross_x = 0;

  for (int i = 0; i < window_size; i++) {
    float dx = samples[i].x - features->mean_x;
    float dy = samples[i].y - features->mean_y;
    float dz = samples[i].z - features->mean_z;
    float mag =
        sqrtf(samples[i].x * samples[i].x + samples[i].y * samples[i].y +
              samples[i].z * samples[i].z);
    float dm = mag - features->magnitude_mean;

    var_x += dx * dx;
    var_y += dy * dy;
    var_z += dz * dz;
    var_mag += dm * dm;

    // Zero crossings (relative to mean)
    if (i > 0) {
      float prev = samples[i - 1].x - features->mean_x;
      float curr = samples[i].x - features->mean_x;
      if ((prev >= 0 && curr < 0) || (prev < 0 && curr >= 0)) {
        zero_cross_x++;
      }
    }
  }

  features->std_x = sqrtf(var_x / window_size);
  features->std_y = sqrtf(var_y / window_size);
  features->std_z = sqrtf(var_z / window_size);
  features->magnitude_std = sqrtf(var_mag / window_size);
  features->zero_crossings = (float)zero_cross_x / window_size;

  // Simple entropy estimate based on histogram
  // For full implementation, would need proper binning
  features->entropy = features->std_x + features->std_y + features->std_z;

  // Peak frequency (simplified - using zero crossing rate)
  // Full implementation would use FFT
  features->peak_frequency = features->zero_crossings * 100.0f; // Approx Hz
}

/**
 * @brief Convert features to array for classifier
 */
static inline void eif_activity_features_to_array(eif_activity_features_t *f,
                                                  float *array) {
  array[0] = f->mean_x;
  array[1] = f->mean_y;
  array[2] = f->mean_z;
  array[3] = f->std_x;
  array[4] = f->std_y;
  array[5] = f->std_z;
  array[6] = f->magnitude_mean;
  array[7] = f->magnitude_std;
  array[8] = f->sma;
  array[9] = f->max_magnitude;
  array[10] = f->min_magnitude;
  array[11] = f->energy;
  array[12] = f->zero_crossings;
  array[13] = f->peak_frequency;
  array[14] = f->entropy;
}

// =============================================================================
// Simple Rule-Based Classifier
// =============================================================================

/**
 * @brief Rule-based activity classifier (no ML needed)
 */
static inline eif_activity_t
eif_activity_classify_rules(eif_activity_features_t *f) {
  // Stationary: low magnitude std and SMA
  if (f->magnitude_std < 0.1f && f->sma < 1.5f) {
    return EIF_ACTIVITY_STATIONARY;
  }

  // Driving: stationary-like but different z-axis
  if (f->magnitude_std < 0.3f && fabsf(f->mean_z - 9.8f) > 2.0f) {
    return EIF_ACTIVITY_DRIVING;
  }

  // Running: high magnitude std and high SMA
  if (f->magnitude_std > 2.0f && f->sma > 15.0f) {
    return EIF_ACTIVITY_RUNNING;
  }

  // Stairs: moderate activity with vertical component
  if (f->magnitude_std > 0.5f && f->magnitude_std < 2.0f) {
    if (f->mean_z > 10.5f) {
      return EIF_ACTIVITY_STAIRS_UP;
    }
    if (f->mean_z < 9.0f) {
      return EIF_ACTIVITY_STAIRS_DOWN;
    }
  }

  // Walking: moderate activity
  if (f->magnitude_std > 0.3f && f->magnitude_std < 2.0f) {
    return EIF_ACTIVITY_WALKING;
  }

  // Cycling: periodic pattern, less impact than running
  if (f->zero_crossings > 0.3f && f->magnitude_std < 1.5f) {
    return EIF_ACTIVITY_CYCLING;
  }

  return EIF_ACTIVITY_UNKNOWN;
}

// =============================================================================
// Sliding Window Buffer
// =============================================================================

/**
 * @brief Sliding window for activity recognition
 */
typedef struct {
  eif_accel_sample_t buffer[EIF_ACTIVITY_WINDOW_SIZE];
  int idx;
  int count;
  int hop_size; ///< Samples between classifications
  int samples_since_hop;
} eif_activity_window_t;

/**
 * @brief Initialize activity window
 */
static inline void eif_activity_window_init(eif_activity_window_t *win,
                                            int hop_size) {
  win->idx = 0;
  win->count = 0;
  win->hop_size = hop_size;
  win->samples_since_hop = 0;
}

/**
 * @brief Add sample to window
 * @return true if window is ready for classification
 */
static inline bool eif_activity_window_add(eif_activity_window_t *win, float x,
                                           float y, float z) {
  win->buffer[win->idx].x = x;
  win->buffer[win->idx].y = y;
  win->buffer[win->idx].z = z;

  win->idx = (win->idx + 1) % EIF_ACTIVITY_WINDOW_SIZE;
  if (win->count < EIF_ACTIVITY_WINDOW_SIZE) {
    win->count++;
  }

  win->samples_since_hop++;

  if (win->count >= EIF_ACTIVITY_WINDOW_SIZE &&
      win->samples_since_hop >= win->hop_size) {
    win->samples_since_hop = 0;
    return true;
  }

  return false;
}

/**
 * @brief Get ordered samples from circular buffer
 */
static inline void
eif_activity_window_get_samples(eif_activity_window_t *win,
                                eif_accel_sample_t *ordered) {
  int start = (win->idx - win->count + EIF_ACTIVITY_WINDOW_SIZE) %
              EIF_ACTIVITY_WINDOW_SIZE;

  for (int i = 0; i < win->count; i++) {
    int buf_idx = (start + i) % EIF_ACTIVITY_WINDOW_SIZE;
    ordered[i] = win->buffer[buf_idx];
  }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ACTIVITY_H
