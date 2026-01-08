/**
 * @file eif_ml_pca_fixed.h
 * @brief Principal Component Analysis (Fixed Point Q15)
 */

#ifndef EIF_ML_PCA_FIXED_H
#define EIF_ML_PCA_FIXED_H

#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t n_features;
  uint16_t n_components;
  
  // Mean vector [n_features] (Q15)
  // Used to center the data: x_centered = x - mean
  const q15_t *mean; 
  
  // Principal Components [n_components x n_features] (Q15)
  // Usually orthonormal vectors
  const q15_t *components;
  
} eif_pca_fixed_t;

/**
 * @brief Initialize PCA (Fixed Point)
 */
void eif_pca_init_fixed(eif_pca_fixed_t *pca,
                        uint16_t n_features,
                        uint16_t n_components,
                        const q15_t *mean,
                        const q15_t *components);

/**
 * @brief Transform input vector to lower dimension (Projection)
 * 
 * @param pca Initialized PCA structure
 * @param input Input vector [n_features]
 * @param output Output vector [n_components]
 */
void eif_pca_transform_fixed(const eif_pca_fixed_t *pca,
                             const q15_t *input,
                             q15_t *output);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_PCA_FIXED_H
