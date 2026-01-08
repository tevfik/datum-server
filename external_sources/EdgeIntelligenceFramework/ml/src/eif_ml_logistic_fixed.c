/**
 * @file eif_ml_logistic_fixed.c
 * @brief Logistic Regression (Fixed Point) Implementation
 */

#include "eif_ml_logistic_fixed.h"
#include <stdlib.h>

#define Q15_0_5 16384

// Simple linear interpolation lookup for sigmoid or approximation?
// Sigmoid(x) = 1 / (1 + exp(-x))
// In fixed point Q15, input range is [-1, 1] usually, but dot product can be large.
// The dot product result comes from weights (Q15) * input (Q15) = Q30.
// Typically we shift down to Q15 or Q3-Q4 integer range for sigmoid lookup.
// Let's use a standard approximation: 0.5 * (x / (1 + |x|) + 1)
// This is "fast sigmoid" or "hard sigmoid".
// Or just piece-wise linear.

// A standard Q15 Sigmoid approximation:
// Input x in Q12.4 format?
// Let's assume the dot product result is scaled to be roughly within [-8, 8] range for sigmoid.
// We'll treat the high bits of the accumulator as the integer part.

static q15_t sigmoid_q15(int32_t acc_q31) {
    // Sigmoid function is non-linear.
    // Using simple "Planer's approximation" or "Fast Sigmoid": 
    // f(x) = x / (1 + |x|) maps (-inf, inf) to (-1, 1).
    // sigmoid(x) = 0.5 * (x / (1 + |x|) + 1)
    
    // We need to handle scaling carefully.
    // Let's assume input acc_q31 represents a value where 1.0 (Q15) is 'unit'.
    // Actually, usually dot product can be quite large.
    // Let's shift Q31 down to Q15 (conceptually 16.15 format)
    // Then just clamp/linear approx.
    
    // Simplest approach: Hard Sigmoid
    // y = 0.2 * x + 0.5, clamped 0..1
    // 0.2 in Q15 is ~6553.
    // x in Q15.
    
    // Let's just scale down the accumulator
    q15_t x_q15 = (q15_t)(acc_q31 >> 1); // Rough scaling, heuristic
    
    // Hard sigmoid
    int32_t val = ((int32_t)x_q15 * 6554) >> 15; // x * 0.2
    val += Q15_0_5;
    
    if (val < 0) return 0;
    if (val > 32767) return 32767;
    return (q15_t)val;
}

void eif_binary_logistic_init_fixed(eif_binary_logistic_fixed_t *lr,
                                   int num_features,
                                   const q15_t *weights,
                                   q31_t bias) {
    lr->num_features = num_features;
    lr->weights = weights;
    lr->bias = bias;
}

q15_t eif_binary_logistic_proba_fixed(const eif_binary_logistic_fixed_t *lr,
                                     const q15_t *input) {
    if (!lr || !input || !lr->weights) return 0;

    // Linear part: z = w.x + b
    int64_t dot_prod = 0; // Use 64-bit to prevent overflow
    for (int i = 0; i < lr->num_features; i++) {
        dot_prod += (int64_t)lr->weights[i] * (int64_t)input[i];
    }
    
    // dot_prod is Q30. Bias is Q31.
    // Convert dot_prod to Q31 (<< 1)
    int64_t z_q31 = (dot_prod << 1) + lr->bias; // Q31
        
    // Sigmoid
    // Since we don't have a full math library for Q31 exp, we use approximation
    // We pass the upper bits to keep it in a manageable range
    // or we implement a proper lookup. 
    // Here we use the simplified helper above.
    
    // Reduce range for sigmoid function input:
    // Scale down from Q31 to fit roughly [-16, 16] or similar for approximation
    // Q31 has 31 fractional bits. 1.0 is 2^31.
    // Real values for logits are often in range -10 to 10.
    // 10.0 in Q31 is crazy large. 
    // Let's assume the weights/inputs were normalized such that output is reasonable.
    // However, 2^30 * 2^15 -> really big.
    
    // Let's normalize by shifting down significantly to get "integer-ish" range
    // Shift right by 16 to get Q15
    q15_t z_prox = (q15_t)(z_q31 >> 16); 
    
    return sigmoid_q15(z_prox);
}

int32_t eif_binary_logistic_predict_fixed(const eif_binary_logistic_fixed_t *lr,
                                         const q15_t *input) {
    q15_t prob = eif_binary_logistic_proba_fixed(lr, input);
    return (prob >= Q15_0_5) ? 1 : 0;
}
