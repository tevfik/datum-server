/**
 * @file eif_ml_pca_fixed.c
 * @brief PCA Fixed Point Implementation
 */

#include "eif_ml_pca_fixed.h"

void eif_pca_init_fixed(eif_pca_fixed_t *pca,
                        uint16_t n_features,
                        uint16_t n_components,
                        const q15_t *mean,
                        const q15_t *components) {
    pca->n_features = n_features;
    pca->n_components = n_components;
    pca->mean = mean;
    pca->components = components;
}

void eif_pca_transform_fixed(const eif_pca_fixed_t *pca,
                             const q15_t *input,
                             q15_t *output) {
    if (!pca || !input || !output) return;

    // Project: Y = (X - Mean) * Components^T
    // For each component j:
    // y[j] = sum_i( (input[i] - mean[i]) * component[j][i] )
    
    for (int j = 0; j < pca->n_components; j++) {
        const q15_t *comp_vec = &pca->components[j * pca->n_features];
        int64_t dot_prod = 0;
        
        for (int i = 0; i < pca->n_features; i++) {
            q15_t centered_val = input[i];
            
            // Subtract mean if provided
            if (pca->mean) {
                // Check overflow? Q15 subtraction is usually safe unless range violation
                centered_val -= pca->mean[i];
            }
            
            // Accumulate
            // Q15 * Q15 -> Q30
            dot_prod += (int64_t)centered_val * (int64_t)comp_vec[i];
        }
        
        // Convert Q30 result back to Q15 for output
        // We might need scaling factor if PC vectors are not unit length or if we want specific range
        // Standard PCA gives output in essentially same unit/scale as input if orthonormal.
        // Q30 >> 15 = Q15
        
        // Saturation check
        int64_t res = dot_prod >> 15;
        if (res > 32767) res = 32767;
        if (res < -32768) res = -32768;
        
        output[j] = (q15_t)res;
    }
}
