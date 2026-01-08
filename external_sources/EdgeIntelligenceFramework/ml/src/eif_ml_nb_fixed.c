/**
 * @file eif_ml_nb_fixed.c
 * @brief Gaussian Naive Bayes (Fixed Point) Implementation
 */

#include "eif_ml_nb_fixed.h"
#include <stdint.h>
#include <math.h> // For definition only, logic is integer

void eif_gaussian_nb_init_fixed(eif_gaussian_nb_fixed_t *nb,
                                int num_features,
                                int num_classes,
                                const q15_t *means,
                                const q15_t *variances,
                                const q15_t *log_priors) {
    nb->num_features = num_features;
    nb->num_classes = num_classes;
    nb->means = means;
    nb->variances = variances;
    nb->log_priors = log_priors;
}

// Fixed Point Gaussian Log Probability Approximation
// log(P(x|c)) ~ -0.5 * sum( log(2*pi*sigma^2) + (x-mu)^2 / sigma^2 )
// Dropping constant terms that don't affect argmax: 
// maximize: - sum( log(sigma) + (x-mu)^2 / (2*sigma^2) )
// If sigma is constant across features (simplification), we just look at dist^2.
// For full GNB: 
// score = log_prior - sum(0.5 * log(2*pi*var) + (x-mean)^2 / (2*var))
int32_t eif_gaussian_nb_predict_fixed(const eif_gaussian_nb_fixed_t *nb, const q15_t *input) {
    if (!nb || !input) return -1;
    
    int32_t best_class = -1;
    int64_t max_log_prob = -9223372036854775807LL; // INT64_MIN

    for (int c = 0; c < nb->num_classes; c++) {
        // Start with prior (Q15 shifted to Q30/Q45 range for accumulation)
        // Let's use Q15 accumulator but be careful of underflow since these are negative log probs usually
        // Actually typical approach: score starts at 0, subtract penalties
        
        int64_t class_score = (int64_t)nb->log_priors[c] << 10; // Simple scaling
        
        const q15_t *means = &nb->means[c * nb->num_features];
        const q15_t *vars = &nb->variances[c * nb->num_features];
        
        for (int i = 0; i < nb->num_features; i++) {
            // Diff = (x - mean)
            int32_t diff = (int32_t)input[i] - (int32_t)means[i];
            
            // Squared Diff (Q30)
            int64_t diff_sq = (int64_t)diff * diff; 
            
            // Variance (Q15). We need to divide by variance.
            // Division in fixed point is costly and tricky.
            // Better to store pre-calculated precision (lambda = 1/sigma^2) in the model struct.
            // For now, doing simple integer division if variance > 0
            if (vars[i] > 0) {
                 // (x-u)^2 / v
                 // result roughly Q15 if we adjust shifts properly
                 // Shift numerator to keep precision
                 int64_t term = diff_sq / vars[i]; 
                 class_score -= term;
                 
                 // Also subtract log(sigma) term if variances differ per class
                 // Here we assume it's baked into log_prior or ignored for simplified GNB
            }
        }
        
        if (class_score > max_log_prob) {
            max_log_prob = class_score;
            best_class = c;
        }
    }
    
    return best_class;
}
