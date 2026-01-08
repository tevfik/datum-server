/**
 * @file eif_online_learning.h
 * @brief Online Learning Utilities for Edge Intelligence
 *
 * Algorithms for learning and adapting on-device:
 * - Online gradient descent
 * - Incremental statistics
 * - Streaming PCA
 * - Adaptive model updates
 *
 * Enables edge devices to improve over time without cloud connectivity.
 */

#ifndef EIF_ONLINE_LEARNING_H
#define EIF_ONLINE_LEARNING_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_ONLINE_MAX_FEATURES 64
#define EIF_ONLINE_MAX_CLASSES 16

// =============================================================================
// Online Gradient Descent
// =============================================================================

/**
 * @brief Online linear model with SGD
 */
typedef struct {
  float weights[EIF_ONLINE_MAX_FEATURES];
  float bias;
  float learning_rate;
  float l2_reg; ///< L2 regularization
  int num_features;
  int64_t updates;
} eif_online_linear_t;

/**
 * @brief Initialize online linear model
 */
static inline void eif_online_linear_init(eif_online_linear_t *model,
                                          int num_features, float learning_rate,
                                          float l2_reg) {
  model->num_features = num_features < EIF_ONLINE_MAX_FEATURES
                            ? num_features
                            : EIF_ONLINE_MAX_FEATURES;
  model->learning_rate = learning_rate;
  model->l2_reg = l2_reg;
  model->bias = 0.0f;
  model->updates = 0;

  for (int i = 0; i < model->num_features; i++) {
    model->weights[i] = 0.0f;
  }
}

/**
 * @brief Predict with online linear model
 */
static inline float eif_online_linear_predict(eif_online_linear_t *model,
                                              const float *features) {
  float sum = model->bias;
  for (int i = 0; i < model->num_features; i++) {
    sum += model->weights[i] * features[i];
  }
  return sum;
}

/**
 * @brief Online update (regression)
 */
static inline void eif_online_linear_update(eif_online_linear_t *model,
                                            const float *features,
                                            float target) {
  float pred = eif_online_linear_predict(model, features);
  float error = target - pred;

  // Adaptive learning rate decay
  float lr = model->learning_rate / (1.0f + 0.0001f * model->updates);

  // Update weights with L2 regularization
  for (int i = 0; i < model->num_features; i++) {
    model->weights[i] +=
        lr * (error * features[i] - model->l2_reg * model->weights[i]);
  }
  model->bias += lr * error;

  model->updates++;
}

/**
 * @brief Online update (binary classification with logistic loss)
 */
static inline void eif_online_linear_update_logistic(eif_online_linear_t *model,
                                                     const float *features,
                                                     int label) {
  float logit = eif_online_linear_predict(model, features);
  float prob = 1.0f / (1.0f + expf(-logit));
  float error = (float)label - prob;

  float lr = model->learning_rate / (1.0f + 0.0001f * model->updates);

  for (int i = 0; i < model->num_features; i++) {
    model->weights[i] +=
        lr * (error * features[i] - model->l2_reg * model->weights[i]);
  }
  model->bias += lr * error;

  model->updates++;
}

// =============================================================================
// Online Centroid Classifier
// =============================================================================

/**
 * @brief Online centroid-based classifier (Nearest Class Mean)
 */
typedef struct {
  float centroids[EIF_ONLINE_MAX_CLASSES][EIF_ONLINE_MAX_FEATURES];
  int64_t class_counts[EIF_ONLINE_MAX_CLASSES];
  int num_classes;
  int num_features;
  float alpha; ///< Exponential update rate (0 = pure mean, >0 = EMA)
} eif_online_centroid_t;

/**
 * @brief Initialize online centroid classifier
 */
static inline void eif_online_centroid_init(eif_online_centroid_t *oc,
                                            int num_classes, int num_features,
                                            float alpha) {
  oc->num_classes = num_classes < EIF_ONLINE_MAX_CLASSES
                        ? num_classes
                        : EIF_ONLINE_MAX_CLASSES;
  oc->num_features = num_features < EIF_ONLINE_MAX_FEATURES
                         ? num_features
                         : EIF_ONLINE_MAX_FEATURES;
  oc->alpha = alpha;

  for (int c = 0; c < oc->num_classes; c++) {
    oc->class_counts[c] = 0;
    for (int f = 0; f < oc->num_features; f++) {
      oc->centroids[c][f] = 0.0f;
    }
  }
}

/**
 * @brief Update centroid with new sample
 */
static inline void eif_online_centroid_update(eif_online_centroid_t *oc,
                                              const float *features,
                                              int label) {
  if (label < 0 || label >= oc->num_classes)
    return;

  oc->class_counts[label]++;

  if (oc->alpha > 0.0f) {
    // Exponential moving average
    for (int f = 0; f < oc->num_features; f++) {
      oc->centroids[label][f] = (1.0f - oc->alpha) * oc->centroids[label][f] +
                                oc->alpha * features[f];
    }
  } else {
    // True running mean
    float n = (float)oc->class_counts[label];
    for (int f = 0; f < oc->num_features; f++) {
      oc->centroids[label][f] += (features[f] - oc->centroids[label][f]) / n;
    }
  }
}

/**
 * @brief Predict class (nearest centroid)
 */
static inline int eif_online_centroid_predict(eif_online_centroid_t *oc,
                                              const float *features) {
  int best_class = 0;
  float best_dist = 1e30f;

  for (int c = 0; c < oc->num_classes; c++) {
    if (oc->class_counts[c] == 0)
      continue;

    float dist = 0.0f;
    for (int f = 0; f < oc->num_features; f++) {
      float diff = features[f] - oc->centroids[c][f];
      dist += diff * diff;
    }

    if (dist < best_dist) {
      best_dist = dist;
      best_class = c;
    }
  }

  return best_class;
}

// =============================================================================
// Incremental Normalization
// =============================================================================

/**
 * @brief Online feature normalization (z-score)
 */
typedef struct {
  float mean[EIF_ONLINE_MAX_FEATURES];
  float m2[EIF_ONLINE_MAX_FEATURES]; ///< Sum of squared deviations
  int num_features;
  int64_t count;
} eif_online_normalizer_t;

/**
 * @brief Initialize normalizer
 */
static inline void eif_online_normalizer_init(eif_online_normalizer_t *norm,
                                              int num_features) {
  norm->num_features = num_features < EIF_ONLINE_MAX_FEATURES
                           ? num_features
                           : EIF_ONLINE_MAX_FEATURES;
  norm->count = 0;

  for (int i = 0; i < norm->num_features; i++) {
    norm->mean[i] = 0.0f;
    norm->m2[i] = 0.0f;
  }
}

/**
 * @brief Update normalization statistics
 */
static inline void eif_online_normalizer_update(eif_online_normalizer_t *norm,
                                                const float *features) {
  norm->count++;

  for (int i = 0; i < norm->num_features; i++) {
    float delta = features[i] - norm->mean[i];
    norm->mean[i] += delta / norm->count;
    float delta2 = features[i] - norm->mean[i];
    norm->m2[i] += delta * delta2;
  }
}

/**
 * @brief Normalize features using current statistics
 */
static inline void
eif_online_normalizer_transform(eif_online_normalizer_t *norm,
                                const float *input, float *output) {
  for (int i = 0; i < norm->num_features; i++) {
    float std = sqrtf(norm->m2[i] / (norm->count > 1 ? norm->count - 1 : 1));
    if (std < 1e-6f)
      std = 1e-6f;
    output[i] = (input[i] - norm->mean[i]) / std;
  }
}

// =============================================================================
// Replay Buffer (Experience Replay for RL/Continual Learning)
// =============================================================================

#define EIF_REPLAY_BUFFER_SIZE 128

typedef struct {
  float features[EIF_REPLAY_BUFFER_SIZE][EIF_ONLINE_MAX_FEATURES];
  int labels[EIF_REPLAY_BUFFER_SIZE];
  int num_features;
  int size;
  int idx;
  int count;
} eif_replay_buffer_t;

/**
 * @brief Initialize replay buffer
 */
static inline void eif_replay_buffer_init(eif_replay_buffer_t *rb,
                                          int num_features) {
  rb->num_features = num_features < EIF_ONLINE_MAX_FEATURES
                         ? num_features
                         : EIF_ONLINE_MAX_FEATURES;
  rb->size = EIF_REPLAY_BUFFER_SIZE;
  rb->idx = 0;
  rb->count = 0;
}

/**
 * @brief Add experience to buffer
 */
static inline void eif_replay_buffer_add(eif_replay_buffer_t *rb,
                                         const float *features, int label) {
  for (int i = 0; i < rb->num_features; i++) {
    rb->features[rb->idx][i] = features[i];
  }
  rb->labels[rb->idx] = label;

  rb->idx = (rb->idx + 1) % rb->size;
  if (rb->count < rb->size)
    rb->count++;
}

/**
 * @brief Sample random experience (for replay)
 */
static inline bool eif_replay_buffer_sample(eif_replay_buffer_t *rb,
                                            float *features, int *label,
                                            uint32_t random_seed) {
  if (rb->count == 0)
    return false;

  // Simple LCG random
  uint32_t rnd = random_seed * 1103515245 + 12345;
  int idx = (rnd >> 16) % rb->count;

  for (int i = 0; i < rb->num_features; i++) {
    features[i] = rb->features[idx][i];
  }
  *label = rb->labels[idx];

  return true;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ONLINE_LEARNING_H
