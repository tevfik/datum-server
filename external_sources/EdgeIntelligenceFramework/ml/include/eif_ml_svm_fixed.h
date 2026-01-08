/**
 * @file eif_ml_svm_fixed.h
 * @brief Support Vector Machine (Inference Only) - Fixed Point
 */

#ifndef EIF_ML_SVM_FIXED_H
#define EIF_ML_SVM_FIXED_H

#include "eif_fixedpoint.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Linear SVM classifier (Fixed Point)
 */
typedef struct {
  int num_features;
  int num_classes;

  // Weight vectors [num_classes x num_features] in Q15
  const q15_t *weights;
  
  // Bias terms [num_classes] in Q31 (higher precision for accumulator)
  const q31_t *bias;
  
} eif_linear_svm_fixed_t;

/**
 * @brief Initialize linear SVM (Fixed Point)
 */
void eif_linear_svm_init_fixed(eif_linear_svm_fixed_t *svm, 
                              int num_features,
                              int num_classes,
                              const q15_t *weights,
                              const q31_t *bias);

/**
 * @brief Predict class for linear SVM (Fixed Point)
 * Perform One-vs-All classification
 */
int32_t eif_linear_svm_predict_fixed(const eif_linear_svm_fixed_t *svm, 
                                    const q15_t *input);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_SVM_FIXED_H
