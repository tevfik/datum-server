/**
 * @file eif_ml_knn_fixed.h
 * @brief k-Nearest Neighbors (k-NN) Classifier (Fixed-Point Q15)
 */

#ifndef EIF_ML_KNN_FIXED_H
#define EIF_ML_KNN_FIXED_H

#include "eif_status.h"
#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

// k-NN Model (Lazy Learning) - Fixed Point
// Stores pointer to training data
typedef struct {
  const q15_t *train_data; // [num_samples * num_features]
  const int32_t *train_labels; // [num_samples]
  int num_samples;
  int num_features;
  int k; // Number of neighbors to vote
} eif_ml_knn_fixed_t;

/**
 * @brief Initialize k-NN Model (Fixed Point)
 */
void eif_ml_knn_init_fixed(eif_ml_knn_fixed_t *knn, const q15_t *train_data,
                           const int32_t *train_labels, int num_samples,
                           int num_features, int k);

/**
 * @brief Predict class for a single input vector (Fixed Point)
 */
int32_t eif_ml_knn_predict_fixed(const eif_ml_knn_fixed_t *knn, const q15_t *input);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_KNN_FIXED_H
