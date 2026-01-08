/**
 * @file eif_ml_svm.h
 * @brief Support Vector Machine (Inference Only)
 *
 * Lightweight SVM for embedded classification:
 * - Linear SVM
 * - RBF kernel SVM (with support vectors)
 * - Multi-class via one-vs-all
 *
 * Designed for inference of pre-trained models.
 */

#ifndef EIF_ML_SVM_H
#define EIF_ML_SVM_H

#include <math.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum dimensions and support vectors
#define EIF_SVM_MAX_FEATURES 64
#define EIF_SVM_MAX_SUPPORT_VECTORS 128
#define EIF_SVM_MAX_CLASSES 10

/**
 * @brief Linear SVM classifier
 */
typedef struct {
  int num_features;
  int num_classes;

  // Weight vectors [num_classes x num_features]
  float *weights;
  // Bias terms [num_classes]
  float *bias;
} eif_linear_svm_t;

/**
 * @brief Initialize linear SVM
 */
static inline void eif_linear_svm_init(eif_linear_svm_t *svm, int num_features,
                                       int num_classes) {
  svm->num_features = num_features;
  svm->num_classes = num_classes;
}

/**
 * @brief Predict with linear SVM (returns class with highest score)
 */
static inline int eif_linear_svm_predict(eif_linear_svm_t *svm,
                                         const float *input) {
  int best_class = 0;
  float best_score = -1e30f;

  for (int c = 0; c < svm->num_classes; c++) {
    float score = svm->bias[c];
    for (int i = 0; i < svm->num_features; i++) {
      score += svm->weights[c * svm->num_features + i] * input[i];
    }

    if (score > best_score) {
      best_score = score;
      best_class = c;
    }
  }

  return best_class;
}

/**
 * @brief RBF kernel SVM (single binary classifier)
 */
typedef struct {
  int num_features;
  int num_sv;  ///< Number of support vectors
  float gamma; ///< RBF kernel parameter
  float rho;   ///< Bias term

  // Support vectors [num_sv x num_features]
  float *support_vectors;
  // Alpha * y values [num_sv]
  float *alpha_y;
} eif_rbf_svm_t;

/**
 * @brief RBF kernel function
 */
static inline float eif_rbf_kernel(const float *x1, const float *x2, int dim,
                                   float gamma) {
  float dist_sq = 0.0f;
  for (int i = 0; i < dim; i++) {
    float diff = x1[i] - x2[i];
    dist_sq += diff * diff;
  }
  return expf(-gamma * dist_sq);
}

/**
 * @brief Predict with RBF SVM (returns decision value)
 */
static inline float eif_rbf_svm_decision(eif_rbf_svm_t *svm,
                                         const float *input) {
  float decision = -svm->rho;

  for (int i = 0; i < svm->num_sv; i++) {
    float k =
        eif_rbf_kernel(input, &svm->support_vectors[i * svm->num_features],
                       svm->num_features, svm->gamma);
    decision += svm->alpha_y[i] * k;
  }

  return decision;
}

/**
 * @brief Binary prediction
 */
static inline int eif_rbf_svm_predict(eif_rbf_svm_t *svm, const float *input) {
  return eif_rbf_svm_decision(svm, input) > 0 ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_SVM_H
