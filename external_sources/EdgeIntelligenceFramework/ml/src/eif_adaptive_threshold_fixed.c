/**
 * @file eif_adaptive_threshold_fixed.c
 * @brief Adaptive Threshold (Fixed Point) Implementation
 */

#include "eif_adaptive_threshold_fixed.h"
#include <stdlib.h>

// Approximate square root for Q15
static q15_t sqrt_q15(q15_t val) {
    if (val <= 0) return 0;
    // Simple Babylonian method or lookup
    // For small range, we can just do 10 iters
    int32_t x = val;
    int32_t y = 1 << 14; // Initial guess 0.5? or val/2
    // Actually val is < 32768. 
    // sqrt(32767) = 181. 
    // But Q15 math: sqrt(x/32768) = sqrt(x)/181. 
    // This is distinct from integer sqrt. 
    // Fixed point sqrt: sqrt(X_fx) = sqrt(X_float) * sqrt(Scale)
    // No, standard approach: y_fx = sqrt(x_fx * Scale)
    
    // Simpler: use standard integer sqrt and adjust
    // sqrt(val * 32768) gives the result in Q15
    // But val * 32768 overflows 32-bit if val > 1 is possible.
    // Assuming val is Q15 (0..1.0), then product ok in 32-bit (max 32767*32768 ~ 1e9 < 2e9)
    
    // Simplified integer sqrt for raw variance values
    int32_t res = 0;
    int32_t bit = 1 << 14; // Optimize start bit for 16-bit input range (max 32767)
    int32_t num = (int32_t)val;
 
    while (bit > num)
        bit >>= 2;
 
    while (bit != 0) {
        if (num >= res + bit) {
            num -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (q15_t)res;
}

void eif_adaptive_thresh_init_fixed(eif_adaptive_threshold_fixed_t *at, q15_t alpha) {
    at->alpha = alpha;
    at->mean = 0;
    at->variance = 0;
    at->initialized = false;
}

void eif_adaptive_thresh_update_fixed(eif_adaptive_threshold_fixed_t *at, q15_t val) {
    if (!at->initialized) {
        at->mean = val;
        at->variance = 0;
        at->initialized = true;
        return;
    }

    // Welford's approx / EMA
    
    int32_t one_q15 = 32768;
    int32_t diff = (int32_t)val - (int32_t)at->mean; // Q15 unit
    
    // Update Mean (EMA)
    // mean += alpha * diff
    int32_t incr = ((int32_t)at->alpha * diff) >> 15;
    at->mean += (q15_t)incr;
    
    // Update Variance (EMA)
    
    // Diff squared (Raw^2)
    int32_t diff_sq = diff * diff; 
    
    // We want Var to be in Raw units.
    // Var = (1-alpha)*Var + alpha*diff^2
    
    // Term 1: alpha * diff_sq
    // Result is Scaled by 2^15 because alpha is Q15.
    // We do NOT shift it down yet, to match Term 2 scale.
    int32_t term1 = ((int32_t)at->alpha * diff_sq); 
    
    // Term 2: (1-alpha) * Var
    // Result is Scaled by 2^15.
    int32_t term2 = ((int32_t)(one_q15 - at->alpha) * (int32_t)at->variance);
    
    // Sum and then normalize
    int32_t new_var_scaled = term1 + term2;
    at->variance = (q15_t)(new_var_scaled >> 15);
}

bool eif_adaptive_thresh_check_fixed(const eif_adaptive_threshold_fixed_t *at, 
                                     q15_t val, 
                                     q15_t std_dev_factor) {
    if (!at->initialized) return false;
    
    q15_t std_dev = sqrt_q15(at->variance);
    
    // Threshold = Mean + factor * std
    // factor is Q11.4? Or Q15 representing e.g. 3.0?
    // Let's assume factor is Q12.3 (8192 = 1.0) or Q13.2
    // If user passes factor=3.0 as Q15 (e.g. 3 * 32768/8? No Q15 supports [-1,1]).
    // Factor > 1 needs different scaling.
    // Let's assume factor is standard Q15 but interpreted as integer part if needed?
    // Safer: factor is integer scaled.
    // Let's assume std_dev_factor is Q12.3 format (where 8 = 1.0, range up to 4096.0)
    // Or just say factor is simple Q15 fraction if < 1.0.
    // Usually we want 2-sigma or 3-sigma.
    // Let's assume input is Q11.4 (1.0 = 16)
    
    int32_t threshold_delta = ((int32_t)std_dev * std_dev_factor) >> 4; // adjust shift for Q4
    
    int32_t upper = at->mean + threshold_delta;
    int32_t lower = at->mean - threshold_delta;
    
    return (val > upper || val < lower);
}
