/**
 * @file eif_learning.h
 * @brief On-Device Learning Utilities for EIF
 *
 * Provides lightweight learning capabilities for embedded systems.
 * Supports incremental updates and simple adaptation mechanisms.
 *
 * Features:
 * - Incremental mean/variance updates (online learning)
 * - Simple gradient-free optimization
 * - K-Nearest Neighbors with dynamic updates
 * - Prototype-based learning
 * - Weight adjustment for personalization
 *
 * Note: Full backpropagation training is typically done offline.
 * These utilities enable adaptation and fine-tuning on device.
 */

#ifndef EIF_LEARNING_H
#define EIF_LEARNING_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

#ifndef EIF_LEARN_MAX_CLASSES
#define EIF_LEARN_MAX_CLASSES 16
#endif

#ifndef EIF_LEARN_MAX_FEATURES
#define EIF_LEARN_MAX_FEATURES 64
#endif

#ifndef EIF_LEARN_MAX_PROTOTYPES
#define EIF_LEARN_MAX_PROTOTYPES 32
#endif

// =============================================================================
// Incremental Statistics
// =============================================================================

/**
 * @brief Running statistics tracker (Welford's algorithm)
 * Computes mean and variance in a single pass with minimal memory.
 */
typedef struct {
  int32_t count;                        ///< Number of samples
  int32_t mean[EIF_LEARN_MAX_FEATURES]; ///< Running mean (Q15)
  int32_t m2[EIF_LEARN_MAX_FEATURES];   ///< Sum of squared differences
  int dim;                              ///< Feature dimension
} eif_stats_t;

/**
 * @brief Initialize statistics tracker
 */
static inline void eif_stats_init(eif_stats_t *stats, int dim) {
  memset(stats, 0, sizeof(eif_stats_t));
  stats->dim = (dim > EIF_LEARN_MAX_FEATURES) ? EIF_LEARN_MAX_FEATURES : dim;
}

/**
 * @brief Update statistics with new sample (Welford's online algorithm)
 */
static inline void eif_stats_update(eif_stats_t *stats, const int16_t *x) {
  stats->count++;
  int32_t n = stats->count;

  for (int i = 0; i < stats->dim; i++) {
    int32_t delta = ((int32_t)x[i] << 15) - stats->mean[i];
    stats->mean[i] += delta / n;
    int32_t delta2 = ((int32_t)x[i] << 15) - stats->mean[i];
    stats->m2[i] += (delta * delta2) >> 15;
  }
}

/**
 * @brief Get current mean
 */
static inline void eif_stats_get_mean(const eif_stats_t *stats, int16_t *mean) {
  for (int i = 0; i < stats->dim; i++) {
    mean[i] = (int16_t)(stats->mean[i] >> 15);
  }
}

/**
 * @brief Get current variance
 */
static inline void eif_stats_get_variance(const eif_stats_t *stats,
                                          int16_t *var) {
  if (stats->count < 2) {
    memset(var, 0, stats->dim * sizeof(int16_t));
    return;
  }
  for (int i = 0; i < stats->dim; i++) {
    var[i] = (int16_t)(stats->m2[i] / (stats->count - 1));
  }
}

// =============================================================================
// Prototype-Based Learning
// =============================================================================

/**
 * @brief Prototype (class centroid) for nearest-prototype classification
 */
typedef struct {
  int16_t center[EIF_LEARN_MAX_FEATURES]; ///< Prototype center
  int32_t count;                          ///< Number of samples
  int label;                              ///< Class label
  bool active;                            ///< Is prototype in use
} eif_prototype_t;

/**
 * @brief Prototype-based classifier with online updates
 */
typedef struct {
  eif_prototype_t prototypes[EIF_LEARN_MAX_PROTOTYPES];
  int num_prototypes;
  int dim;
  int16_t learning_rate; ///< Q15 learning rate for updates
} eif_proto_classifier_t;

/**
 * @brief Initialize prototype classifier
 */
static inline void eif_proto_init(eif_proto_classifier_t *clf, int dim) {
  memset(clf, 0, sizeof(eif_proto_classifier_t));
  clf->dim = (dim > EIF_LEARN_MAX_FEATURES) ? EIF_LEARN_MAX_FEATURES : dim;
  clf->learning_rate = 3277; // 0.1 in Q15
}

/**
 * @brief Set learning rate (0.0 to 1.0)
 */
static inline void eif_proto_set_lr(eif_proto_classifier_t *clf, float lr) {
  clf->learning_rate = (int16_t)(lr * 32767.0f);
}

/**
 * @brief Compute squared L2 distance
 */
static inline int32_t eif_proto_distance(const int16_t *a, const int16_t *b,
                                         int dim) {
  int32_t dist = 0;
  for (int i = 0; i < dim; i++) {
    int32_t diff = (int32_t)a[i] - b[i];
    dist += (diff * diff) >> 10; // Scale to prevent overflow
  }
  return dist;
}

/**
 * @brief Find closest prototype
 * @return Index of closest prototype, -1 if none
 */
static inline int eif_proto_find_closest(const eif_proto_classifier_t *clf,
                                         const int16_t *x) {
  int32_t min_dist = 0x7FFFFFFF;
  int closest = -1;

  for (int i = 0; i < clf->num_prototypes; i++) {
    if (!clf->prototypes[i].active)
      continue;

    int32_t dist = eif_proto_distance(x, clf->prototypes[i].center, clf->dim);
    if (dist < min_dist) {
      min_dist = dist;
      closest = i;
    }
  }

  return closest;
}

/**
 * @brief Predict class label
 */
static inline int eif_proto_predict(const eif_proto_classifier_t *clf,
                                    const int16_t *x) {
  int idx = eif_proto_find_closest(clf, x);
  return (idx >= 0) ? clf->prototypes[idx].label : -1;
}

/**
 * @brief Add or update prototype with new sample
 */
static inline void eif_proto_update(eif_proto_classifier_t *clf,
                                    const int16_t *x, int label) {
  // Find existing prototype with this label
  int existing = -1;
  for (int i = 0; i < clf->num_prototypes; i++) {
    if (clf->prototypes[i].active && clf->prototypes[i].label == label) {
      existing = i;
      break;
    }
  }

  if (existing >= 0) {
    // Update existing prototype with moving average
    eif_prototype_t *p = &clf->prototypes[existing];
    p->count++;

    for (int i = 0; i < clf->dim; i++) {
      // center = center + lr * (x - center)
      int32_t diff = (int32_t)x[i] - p->center[i];
      int32_t update = (diff * clf->learning_rate) >> 15;
      p->center[i] += (int16_t)update;
    }
  } else {
    // Create new prototype
    if (clf->num_prototypes < EIF_LEARN_MAX_PROTOTYPES) {
      eif_prototype_t *p = &clf->prototypes[clf->num_prototypes];
      memcpy(p->center, x, clf->dim * sizeof(int16_t));
      p->label = label;
      p->count = 1;
      p->active = true;
      clf->num_prototypes++;
    }
  }
}

/**
 * @brief Train on a batch of samples
 */
static inline void eif_proto_train(eif_proto_classifier_t *clf,
                                   const int16_t *X, const int *labels,
                                   int n_samples) {
  for (int i = 0; i < n_samples; i++) {
    eif_proto_update(clf, &X[i * clf->dim], labels[i]);
  }
}

// =============================================================================
// Simple Output Adjustment (Last-Layer Fine-Tuning)
// =============================================================================

/**
 * @brief Output calibration for personalization
 * Adjusts final layer weights based on user feedback.
 */
typedef struct {
  int16_t scale[EIF_LEARN_MAX_CLASSES]; ///< Per-class scaling (Q15)
  int16_t bias[EIF_LEARN_MAX_CLASSES];  ///< Per-class bias
  int num_classes;
  int16_t learning_rate;
} eif_output_adapter_t;

/**
 * @brief Initialize output adapter
 */
static inline void eif_adapter_init(eif_output_adapter_t *adapt,
                                    int num_classes) {
  adapt->num_classes = (num_classes > EIF_LEARN_MAX_CLASSES)
                           ? EIF_LEARN_MAX_CLASSES
                           : num_classes;
  adapt->learning_rate = 1638; // 0.05 in Q15

  for (int i = 0; i < adapt->num_classes; i++) {
    adapt->scale[i] = 32767; // 1.0 in Q15
    adapt->bias[i] = 0;
  }
}

/**
 * @brief Apply adaptation to model output
 */
static inline void eif_adapter_apply(const eif_output_adapter_t *adapt,
                                     const int16_t *input, int16_t *output) {
  for (int i = 0; i < adapt->num_classes; i++) {
    int32_t val = ((int32_t)input[i] * adapt->scale[i]) >> 15;
    val += adapt->bias[i];

    if (val > 32767)
      val = 32767;
    if (val < -32768)
      val = -32768;

    output[i] = (int16_t)val;
  }
}

/**
 * @brief Update adapter based on correct label
 * Simple heuristic: boost correct class, reduce others
 */
static inline void eif_adapter_update(eif_output_adapter_t *adapt,
                                      const int16_t *predicted,
                                      int correct_label) {
  int16_t lr = adapt->learning_rate;

  for (int i = 0; i < adapt->num_classes; i++) {
    if (i == correct_label) {
      // Increase scale/bias for correct class
      int32_t boost = (32767 - adapt->scale[i]) * lr >> 15;
      adapt->scale[i] += (int16_t)boost;
      adapt->bias[i] += lr >> 2;
    } else if (predicted[i] > predicted[correct_label]) {
      // Reduce scale for incorrectly-high predictions
      int32_t reduce = adapt->scale[i] * lr >> 16;
      adapt->scale[i] -= (int16_t)reduce;
      adapt->bias[i] -= lr >> 2;
    }
  }
}

// =============================================================================
// Exponential Moving Average for Weights
// =============================================================================

/**
 * @brief EMA weight smoother for stable adaptation
 */
typedef struct {
  int16_t *weights;     ///< Current weights
  int16_t *ema_weights; ///< Smoothed weights (EMA)
  int size;
  int16_t alpha; ///< EMA coefficient (Q15)
} eif_ema_t;

/**
 * @brief Initialize EMA tracker
 */
static inline void eif_ema_init(eif_ema_t *ema, int16_t *weights,
                                int16_t *ema_buf, int size) {
  ema->weights = weights;
  ema->ema_weights = ema_buf;
  ema->size = size;
  ema->alpha = 3277; // 0.1 in Q15

  // Initialize EMA to current weights
  memcpy(ema_buf, weights, size * sizeof(int16_t));
}

/**
 * @brief Update EMA: ema = alpha * weights + (1-alpha) * ema
 */
static inline void eif_ema_update(eif_ema_t *ema) {
  int16_t alpha = ema->alpha;
  int16_t one_minus_alpha = 32767 - alpha;

  for (int i = 0; i < ema->size; i++) {
    int32_t val = ((int32_t)ema->weights[i] * alpha) >> 15;
    val += ((int32_t)ema->ema_weights[i] * one_minus_alpha) >> 15;
    ema->ema_weights[i] = (int16_t)val;
  }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_LEARNING_H
