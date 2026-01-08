#include "eif_dsp.h"
#include <stddef.h>

eif_status_t eif_dsp_fir_f32(const float32_t* input, float32_t* output, size_t length, const float32_t* coeffs, size_t taps) {
    if (!input || !output || !coeffs) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (size_t i = 0; i < length; i++) {
        float32_t sum = 0.0f;
        for (size_t j = 0; j < taps; j++) {
            if (i >= j) {
                sum += input[i - j] * coeffs[j];
            }
        }
        output[i] = sum;
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_iir_f32(const float32_t* input, float32_t* output, size_t length, 
                         const float32_t* coeffs, float32_t* state, size_t stages) {
    if (!input || !output || !coeffs || !state) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Direct Form II Transposed
    // Coeffs: [b0, b1, b2, a1, a2]
    // State: [d1, d2]
    
    for (size_t n = 0; n < length; n++) {
        float32_t x = input[n];
        
        // Process each stage (biquad)
        for (size_t s = 0; s < stages; s++) {
            const float32_t* c = &coeffs[s * 5];
            float32_t* d = &state[s * 2];
            
            float32_t b0 = c[0];
            float32_t b1 = c[1];
            float32_t b2 = c[2];
            float32_t a1 = c[3];
            float32_t a2 = c[4];
            
            // y[n] = b0*x[n] + d1[n-1]
            float32_t stage_y = b0 * x + d[0];
            
            // d1[n] = b1*x[n] - a1*y[n] + d2[n-1]
            d[0] = b1 * x - a1 * stage_y + d[1];
            
            // d2[n] = b2*x[n] - a2*y[n]
            d[1] = b2 * x - a2 * stage_y;
            
            x = stage_y; // Output of this stage is input to next
        }
        output[n] = x;
    }
    return EIF_STATUS_OK;
}
