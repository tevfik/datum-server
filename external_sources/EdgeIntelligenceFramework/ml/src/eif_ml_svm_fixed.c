/**
 * @file eif_ml_svm_fixed.c
 * @brief Support Vector Machine (Fixed Point) Implementation
 */

#include "eif_ml_svm_fixed.h"
#include <stdint.h>

void eif_linear_svm_init_fixed(eif_linear_svm_fixed_t *svm, 
                              int num_features,
                              int num_classes,
                              const q15_t *weights,
                              const q31_t *bias) {
    svm->num_features = num_features;
    svm->num_classes = num_classes;
    svm->weights = weights;
    svm->bias = bias;
}

int32_t eif_linear_svm_predict_fixed(const eif_linear_svm_fixed_t *svm, 
                                    const q15_t *input) {
    if (!svm || !input || !svm->weights) return -1;

    int32_t best_class = -1;
    int64_t max_score = -9223372036854775807LL; // INT64_MIN

    for (int c = 0; c < svm->num_classes; c++) {
        // Point to the weight vector for class c
        // Weights are flattened: [class 0 w... | class 1 w... | ...]
        const q15_t *class_weights = &svm->weights[c * svm->num_features];
        
        int64_t dot_prod = 0;
        
        // Calculate dot product
        // TODO: Optimize with SIMD (eif_hal_simd) later
        for (int i = 0; i < svm->num_features; i++) {
            dot_prod += (int32_t)input[i] * (int32_t)class_weights[i];
        }
        
        // Convert Q30 dot product to Q31 domain (approximate by << 1)
        // Check for potential overflow if needed, but 64-bit is safe
        int64_t score_q31 = dot_prod << 1;
        
        // Add bias (Q31)
        if (svm->bias) {
            score_q31 += svm->bias[c];
        }
        
        if (score_q31 > max_score) {
            max_score = score_q31;
            best_class = c;
        }
    }

    return best_class;
}
