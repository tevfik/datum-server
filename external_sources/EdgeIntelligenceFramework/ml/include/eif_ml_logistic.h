/**
 * @file eif_ml_logistic.h
 * @brief Logistic Regression
 *
 * Binary and multi-class logistic regression:
 * - Binary logistic regression
 * - Softmax regression (multi-class)
 *
 * Simple, interpretable linear classifier.
 */

#ifndef EIF_ML_LOGISTIC_H
#define EIF_ML_LOGISTIC_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_LOGISTIC_MAX_FEATURES 64
#define EIF_LOGISTIC_MAX_CLASSES 10

/**
 * @brief Sigmoid function
 */
static inline float sigmoid_lr(float x) { return 1.0f / (1.0f + expf(-x)); }

/**
 * @brief Binary logistic regression
 */
typedef struct {
  int num_features;
  float *weights; // [num_features]
  float bias;
} eif_binary_logistic_t;

/**
 * @brief Initialize binary logistic regression
 */
static inline void eif_binary_logistic_init(eif_binary_logistic_t *lr,
                                            int num_features) {
  lr->num_features = num_features;
  lr->bias = 0.0f;
}

/**
 * @brief Predict probability of class 1
 */
static inline float eif_binary_logistic_proba(eif_binary_logistic_t *lr,
                                              const float *input) {
  float z = lr->bias;
  for (int i = 0; i < lr->num_features; i++) {
    z += lr->weights[i] * input[i];
  }
  return sigmoid_lr(z);
}

/**
 * @brief Predict class (0 or 1)
 */
static inline int eif_binary_logistic_predict(eif_binary_logistic_t *lr,
                                              const float *input) {
  return eif_binary_logistic_proba(lr, input) >= 0.5f ? 1 : 0;
}

/**
 * @brief Multi-class logistic regression (softmax)
 */
typedef struct {
  int num_features;
  int num_classes;

  // Weight matrix [num_classes x num_features]
  float *weights;
  // Bias vector [num_classes]
  float *bias;
} eif_softmax_regression_t;

/**
 * @brief Initialize softmax regression
 */
static inline void eif_softmax_regression_init(eif_softmax_regression_t *sr,
                                               int num_features,
                                               int num_classes) {
  sr->num_features = num_features;
  sr->num_classes = num_classes;
}

/**
 * @brief Predict class probabilities
 */
static inline void eif_softmax_regression_proba(eif_softmax_regression_t *sr,
                                                const float *input,
                                                float *probs) {
  float max_z = -1e30f;

  // Compute logits
  for (int c = 0; c < sr->num_classes; c++) {
    probs[c] = sr->bias[c];
    for (int i = 0; i < sr->num_features; i++) {
      probs[c] += sr->weights[c * sr->num_features + i] * input[i];
    }
    if (probs[c] > max_z)
      max_z = probs[c];
  }

  // Softmax
  float sum = 0.0f;
  for (int c = 0; c < sr->num_classes; c++) {
    probs[c] = expf(probs[c] - max_z);
    sum += probs[c];
  }
  for (int c = 0; c < sr->num_classes; c++) {
    probs[c] /= sum;
  }
}

/**
 * @brief Predict class
 */
static inline int eif_softmax_regression_predict(eif_softmax_regression_t *sr,
                                                 const float *input) {
  float probs[EIF_LOGISTIC_MAX_CLASSES];
  eif_softmax_regression_proba(sr, input, probs);

  int best_class = 0;
  float best_prob = probs[0];
  for (int c = 1; c < sr->num_classes; c++) {
    if (probs[c] > best_prob) {
      best_prob = probs[c];
      best_class = c;
    }
  }

  return best_class;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_LOGISTIC_H
