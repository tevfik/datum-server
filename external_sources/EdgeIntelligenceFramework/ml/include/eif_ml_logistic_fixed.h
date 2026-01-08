/**
 * @file eif_ml_logistic_fixed.h
 * @brief Logistic Regression (Inference Only) - Fixed Point Q15
 */

#ifndef EIF_ML_LOGISTIC_FIXED_H
#define EIF_ML_LOGISTIC_FIXED_H

#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Logistic Regression (Fixed Point)
// ============================================================================

/**
 * @brief Binary logistic regression (Fixed Point)
 */
typedef struct {
  int num_features;
  const q15_t *weights; // [num_features] (Q15)
  q31_t bias;           // (Q31) to match accumulator precision
} eif_binary_logistic_fixed_t;

/**
 * @brief Initialize binary logistic regression
 */
void eif_binary_logistic_init_fixed(eif_binary_logistic_fixed_t *lr,
                                   int num_features,
                                   const q15_t *weights,
                                   q31_t bias);

/**
 * @brief Predict probability of class 1
 * Returns Q15 value between 0 (0.0) and 32767 (1.0)
 */
q15_t eif_binary_logistic_proba_fixed(const eif_binary_logistic_fixed_t *lr,
                                     const q15_t *input);

/**
 * @brief Predict class (0 or 1) with threshold 0.5 (16384)
 */
int32_t eif_binary_logistic_predict_fixed(const eif_binary_logistic_fixed_t *lr,
                                         const q15_t *input);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_LOGISTIC_FIXED_H
