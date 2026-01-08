/**
 * @file eif_gesture.h
 * @brief Gesture Recognition for Edge AI
 *
 * IMU-based gesture recognition:
 * - Template matching with DTW
 * - Feature-based classification
 * - Continuous gesture detection
 *
 * Suitable for wearables, game controllers, smart home interfaces.
 */

#ifndef EIF_GESTURE_H
#define EIF_GESTURE_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_GESTURE_MAX_TEMPLATES 10
#define EIF_GESTURE_MAX_LENGTH 64
#define EIF_GESTURE_FEATURE_DIM 6 // ax, ay, az, gx, gy, gz

// =============================================================================
// Gesture Types
// =============================================================================

typedef enum {
  EIF_GESTURE_NONE = 0,
  EIF_GESTURE_SWIPE_LEFT,
  EIF_GESTURE_SWIPE_RIGHT,
  EIF_GESTURE_SWIPE_UP,
  EIF_GESTURE_SWIPE_DOWN,
  EIF_GESTURE_CIRCLE_CW,
  EIF_GESTURE_CIRCLE_CCW,
  EIF_GESTURE_TAP,
  EIF_GESTURE_DOUBLE_TAP,
  EIF_GESTURE_SHAKE,
  EIF_GESTURE_CUSTOM_1,
  EIF_GESTURE_CUSTOM_2,
  EIF_GESTURE_CUSTOM_3,
  EIF_NUM_GESTURES
} eif_gesture_type_t;

static const char *eif_gesture_names[] = {
    "None",      "Swipe Left", "Swipe Right", "Swipe Up",   "Swipe Down",
    "Circle CW", "Circle CCW", "Tap",         "Double Tap", "Shake",
    "Custom 1",  "Custom 2",   "Custom 3"};

// =============================================================================
// Gesture Template
// =============================================================================

/**
 * @brief Gesture template for matching
 */
typedef struct {
  eif_gesture_type_t type;
  float samples[EIF_GESTURE_MAX_LENGTH][EIF_GESTURE_FEATURE_DIM];
  int length;
  float threshold; ///< Maximum distance for match
} eif_gesture_template_t;

// =============================================================================
// Gesture Detector
// =============================================================================

/**
 * @brief Gesture detection state
 */
typedef struct {
  // Recording buffer
  float buffer[EIF_GESTURE_MAX_LENGTH][EIF_GESTURE_FEATURE_DIM];
  int buffer_idx;
  int buffer_count;

  // Templates
  eif_gesture_template_t templates[EIF_GESTURE_MAX_TEMPLATES];
  int num_templates;

  // Detection state
  bool recording;
  float motion_threshold;
  float last_magnitude;
  int still_count;
  int motion_count;

  // Result
  eif_gesture_type_t last_gesture;
  float last_distance;
} eif_gesture_detector_t;

/**
 * @brief Initialize gesture detector
 */
static inline void eif_gesture_init(eif_gesture_detector_t *det,
                                    float motion_threshold) {
  det->buffer_idx = 0;
  det->buffer_count = 0;
  det->num_templates = 0;
  det->recording = false;
  det->motion_threshold = motion_threshold;
  det->last_magnitude = 0;
  det->still_count = 0;
  det->motion_count = 0;
  det->last_gesture = EIF_GESTURE_NONE;
  det->last_distance = 0;
}

/**
 * @brief Add a template gesture
 */
static inline bool
eif_gesture_add_template(eif_gesture_detector_t *det, eif_gesture_type_t type,
                         const float samples[][EIF_GESTURE_FEATURE_DIM],
                         int length, float threshold) {
  if (det->num_templates >= EIF_GESTURE_MAX_TEMPLATES)
    return false;
  if (length > EIF_GESTURE_MAX_LENGTH)
    length = EIF_GESTURE_MAX_LENGTH;

  eif_gesture_template_t *t = &det->templates[det->num_templates];
  t->type = type;
  t->length = length;
  t->threshold = threshold;

  for (int i = 0; i < length; i++) {
    for (int j = 0; j < EIF_GESTURE_FEATURE_DIM; j++) {
      t->samples[i][j] = samples[i][j];
    }
  }

  det->num_templates++;
  return true;
}

/**
 * @brief Simple DTW distance between two sequences
 */
static inline float
eif_gesture_dtw_distance(const float a[][EIF_GESTURE_FEATURE_DIM], int len_a,
                         const float b[][EIF_GESTURE_FEATURE_DIM], int len_b) {
  // Simplified DTW with reduced memory
  // Use two rows instead of full matrix
  float prev_row[EIF_GESTURE_MAX_LENGTH + 1];
  float curr_row[EIF_GESTURE_MAX_LENGTH + 1];

  // Initialize first row
  for (int j = 0; j <= len_b; j++) {
    prev_row[j] = 1e30f;
  }
  prev_row[0] = 0;

  for (int i = 1; i <= len_a; i++) {
    curr_row[0] = 1e30f;

    for (int j = 1; j <= len_b; j++) {
      // Euclidean distance
      float dist = 0;
      for (int k = 0; k < EIF_GESTURE_FEATURE_DIM; k++) {
        float d = a[i - 1][k] - b[j - 1][k];
        dist += d * d;
      }
      dist = sqrtf(dist);

      // DTW recurrence
      float min_prev = prev_row[j - 1];
      if (prev_row[j] < min_prev)
        min_prev = prev_row[j];
      if (curr_row[j - 1] < min_prev)
        min_prev = curr_row[j - 1];

      curr_row[j] = dist + min_prev;
    }

    // Swap rows
    for (int j = 0; j <= len_b; j++) {
      prev_row[j] = curr_row[j];
    }
  }

  return prev_row[len_b];
}

/**
 * @brief Update gesture detector with new sample
 */
static inline eif_gesture_type_t eif_gesture_update(eif_gesture_detector_t *det,
                                                    float ax, float ay,
                                                    float az, float gx,
                                                    float gy, float gz) {
  float magnitude = sqrtf(ax * ax + ay * ay + az * az);
  float delta = fabsf(magnitude - det->last_magnitude);
  det->last_magnitude = magnitude;

  // Motion detection
  bool is_motion = delta > det->motion_threshold;

  if (is_motion) {
    det->motion_count++;
    det->still_count = 0;
  } else {
    det->still_count++;
    det->motion_count = 0;
  }

  // Start recording on motion
  if (!det->recording && det->motion_count > 3) {
    det->recording = true;
    det->buffer_idx = 0;
    det->buffer_count = 0;
  }

  // Record samples
  if (det->recording) {
    if (det->buffer_count < EIF_GESTURE_MAX_LENGTH) {
      int idx = det->buffer_count;
      det->buffer[idx][0] = ax;
      det->buffer[idx][1] = ay;
      det->buffer[idx][2] = az;
      det->buffer[idx][3] = gx;
      det->buffer[idx][4] = gy;
      det->buffer[idx][5] = gz;
      det->buffer_count++;
    }

    // Stop recording after still period
    if (det->still_count > 10 && det->buffer_count > 5) {
      det->recording = false;

      // Match against templates
      det->last_gesture = EIF_GESTURE_NONE;
      det->last_distance = 1e30f;

      for (int t = 0; t < det->num_templates; t++) {
        eif_gesture_template_t *tmpl = &det->templates[t];
        float dist = eif_gesture_dtw_distance(
            (const float (*)[EIF_GESTURE_FEATURE_DIM])det->buffer,
            det->buffer_count,
            (const float (*)[EIF_GESTURE_FEATURE_DIM])tmpl->samples,
            tmpl->length);

        // Normalize by length
        dist /= (det->buffer_count + tmpl->length);

        if (dist < det->last_distance && dist < tmpl->threshold) {
          det->last_distance = dist;
          det->last_gesture = tmpl->type;
        }
      }

      return det->last_gesture;
    }
  }

  return EIF_GESTURE_NONE;
}

// =============================================================================
// Simple Gesture Rules (No Templates Needed)
// =============================================================================

/**
 * @brief Simple rule-based gesture detection
 */
typedef struct {
  float ax_sum, ay_sum, az_sum;
  float ax_max, ay_max, az_max;
  int count;
  bool active;
  float threshold;
} eif_simple_gesture_t;

/**
 * @brief Initialize simple gesture detector
 */
static inline void eif_simple_gesture_init(eif_simple_gesture_t *sg,
                                           float threshold) {
  sg->ax_sum = sg->ay_sum = sg->az_sum = 0;
  sg->ax_max = sg->ay_max = sg->az_max = 0;
  sg->count = 0;
  sg->active = false;
  sg->threshold = threshold;
}

/**
 * @brief Update simple gesture detector
 */
static inline eif_gesture_type_t
eif_simple_gesture_update(eif_simple_gesture_t *sg, float ax, float ay,
                          float az) {
  float mag = sqrtf(ax * ax + ay * ay + az * az);

  // Detect motion start
  if (!sg->active && mag > sg->threshold) {
    sg->active = true;
    sg->ax_sum = sg->ay_sum = sg->az_sum = 0;
    sg->ax_max = sg->ay_max = sg->az_max = 0;
    sg->count = 0;
  }

  // Accumulate during motion
  if (sg->active) {
    sg->ax_sum += ax;
    sg->ay_sum += ay;
    sg->az_sum += az;

    if (fabsf(ax) > fabsf(sg->ax_max))
      sg->ax_max = ax;
    if (fabsf(ay) > fabsf(sg->ay_max))
      sg->ay_max = ay;
    if (fabsf(az) > fabsf(sg->az_max))
      sg->az_max = az;

    sg->count++;

    // Detect motion end
    if (mag < sg->threshold * 0.5f && sg->count > 5) {
      sg->active = false;

      // Classify based on dominant direction
      float abs_x = fabsf(sg->ax_sum);
      float abs_y = fabsf(sg->ay_sum);
      float abs_z = fabsf(sg->az_sum);

      if (abs_x > abs_y && abs_x > abs_z) {
        return sg->ax_sum > 0 ? EIF_GESTURE_SWIPE_RIGHT
                              : EIF_GESTURE_SWIPE_LEFT;
      } else if (abs_y > abs_x && abs_y > abs_z) {
        return sg->ay_sum > 0 ? EIF_GESTURE_SWIPE_UP : EIF_GESTURE_SWIPE_DOWN;
      } else if (sg->count < 10 && fabsf(sg->az_max) > sg->threshold * 2) {
        return EIF_GESTURE_TAP;
      } else if (sg->count > 30) {
        return EIF_GESTURE_SHAKE;
      }
    }
  }

  return EIF_GESTURE_NONE;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_GESTURE_H
