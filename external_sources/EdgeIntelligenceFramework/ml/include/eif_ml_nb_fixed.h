/**
 * @file eif_ml_nb_fixed.h
 * @brief Gaussian Naive Bayes Classifier (Fixed Point)
 */

#ifndef EIF_ML_NB_FIXED_H
#define EIF_ML_NB_FIXED_H

#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

// We use Q15 for inputs/means and likely Q31 for variances/probabilities
// to maintain precision during the Gaussian exponent calculation.

typedef struct {
  int num_features;
  int num_classes;

  // Means [num_classes x num_features] (Q15)
  const q15_t *means;

  // Variances [num_classes x num_features] (Q15 or Q31 depending on scale)
  // Inverse Variance usually pre-calculated to avoid division
  // Let's store sigma^2 in Q15 for simplicity first
  const q15_t *variances; 

  // Log Priors [num_classes] (Q15)
  // log(P(Y=c)) precalculated
  const q15_t *log_priors;
  
} eif_gaussian_nb_fixed_t;

/**
 * @brief Initialize NB Model (Fixed Point)
 */
void eif_gaussian_nb_init_fixed(eif_gaussian_nb_fixed_t *nb,
                                int num_features,
                                int num_classes,
                                const q15_t *means,
                                const q15_t *variances,
                                const q15_t *log_priors);

/**
 * @brief Predict class (Fixed Point)
 */
int32_t eif_gaussian_nb_predict_fixed(const eif_gaussian_nb_fixed_t *nb, const q15_t *input);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_NB_FIXED_H
