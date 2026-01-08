/**
 * @file eif_ml_knn.h
 * @brief k-Nearest Neighbors (k-NN) Classifier
 */

#ifndef EIF_ML_KNN_H
#define EIF_ML_KNN_H

#include "eif_status.h"
#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// k-NN Model (Lazy Learning)
// Stores pointer to training data
typedef struct {
  const float32_t *train_data; // [num_samples * num_features]
  const int32_t *train_labels; // [num_samples]
  int num_samples;
  int num_features;
  int k; // Number of neighbors to vote
} eif_ml_knn_t;

/**
 * @brief Initialize k-NN Model
 */
void eif_ml_knn_init(eif_ml_knn_t *knn, const float32_t *train_data,
                     const int32_t *train_labels, int num_samples,
                     int num_features, int k);

/**
 * @brief Predict class for a single input vector
 */
int32_t eif_ml_knn_predict(const eif_ml_knn_t *knn, const float32_t *input);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_KNN_H
