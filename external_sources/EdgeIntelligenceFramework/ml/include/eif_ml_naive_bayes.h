/**
 * @file eif_ml_naive_bayes.h
 * @brief Naive Bayes Classifier
 *
 * Lightweight Naive Bayes for embedded systems:
 * - Gaussian Naive Bayes
 * - Multinomial Naive Bayes
 *
 * Fast, memory-efficient probabilistic classifier.
 */

#ifndef EIF_ML_NAIVE_BAYES_H
#define EIF_ML_NAIVE_BAYES_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EIF_NB_MAX_FEATURES 64
#define EIF_NB_MAX_CLASSES 10

/**
 * @brief Gaussian Naive Bayes classifier
 */
typedef struct {
  int num_features;
  int num_classes;

  // Per-class statistics [num_classes x num_features]
  float *means;
  float *variances;

  // Class priors [num_classes] (log probabilities)
  float *log_priors;
} eif_gaussian_nb_t;

/**
 * @brief Initialize Gaussian NB
 */
static inline void eif_gaussian_nb_init(eif_gaussian_nb_t *nb, int num_features,
                                        int num_classes) {
  nb->num_features = num_features;
  nb->num_classes = num_classes;
}

/**
 * @brief Gaussian PDF (log)
 */
static inline float gaussian_log_pdf(float x, float mean, float var) {
  float diff = x - mean;
  return -0.5f * (logf(2.0f * M_PI * var) + (diff * diff) / var);
}

/**
 * @brief Predict with Gaussian NB
 * @return Predicted class label
 */
static inline int eif_gaussian_nb_predict(eif_gaussian_nb_t *nb,
                                          const float *input) {
  int best_class = 0;
  float best_log_prob = -1e30f;

  for (int c = 0; c < nb->num_classes; c++) {
    float log_prob = nb->log_priors[c];

    for (int i = 0; i < nb->num_features; i++) {
      int idx = c * nb->num_features + i;
      log_prob +=
          gaussian_log_pdf(input[i], nb->means[idx], nb->variances[idx]);
    }

    if (log_prob > best_log_prob) {
      best_log_prob = log_prob;
      best_class = c;
    }
  }

  return best_class;
}

/**
 * @brief Get class probabilities (normalized)
 */
static inline void eif_gaussian_nb_predict_proba(eif_gaussian_nb_t *nb,
                                                 const float *input,
                                                 float *probs) {
  float log_probs[EIF_NB_MAX_CLASSES];
  float max_log = -1e30f;

  // Compute log probabilities
  for (int c = 0; c < nb->num_classes; c++) {
    log_probs[c] = nb->log_priors[c];

    for (int i = 0; i < nb->num_features; i++) {
      int idx = c * nb->num_features + i;
      log_probs[c] +=
          gaussian_log_pdf(input[i], nb->means[idx], nb->variances[idx]);
    }

    if (log_probs[c] > max_log)
      max_log = log_probs[c];
  }

  // Convert to probabilities (log-sum-exp trick)
  float sum = 0.0f;
  for (int c = 0; c < nb->num_classes; c++) {
    probs[c] = expf(log_probs[c] - max_log);
    sum += probs[c];
  }

  for (int c = 0; c < nb->num_classes; c++) {
    probs[c] /= sum;
  }
}

/**
 * @brief Multinomial Naive Bayes (for text/count data)
 */
typedef struct {
  int num_features; ///< Vocabulary size
  int num_classes;

  // Log probabilities [num_classes x num_features]
  float *log_feature_probs;
  // Class priors [num_classes]
  float *log_priors;
} eif_multinomial_nb_t;

/**
 * @brief Predict with Multinomial NB
 */
static inline int eif_multinomial_nb_predict(eif_multinomial_nb_t *nb,
                                             const float *counts) {
  int best_class = 0;
  float best_log_prob = -1e30f;

  for (int c = 0; c < nb->num_classes; c++) {
    float log_prob = nb->log_priors[c];

    for (int i = 0; i < nb->num_features; i++) {
      if (counts[i] > 0) {
        log_prob += counts[i] * nb->log_feature_probs[c * nb->num_features + i];
      }
    }

    if (log_prob > best_log_prob) {
      best_log_prob = log_prob;
      best_class = c;
    }
  }

  return best_class;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_NAIVE_BAYES_H
