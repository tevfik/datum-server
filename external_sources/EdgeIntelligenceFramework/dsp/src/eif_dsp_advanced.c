#include "eif_dsp.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// --- Filter Design (Butterworth) ---

// Simplified implementation for 1st and 2nd order sections (Biquads)
// For higher orders, we would cascade biquads.
// Here we implement a simple generic calculation for low order, or specific formulas.
// Let's implement generic recursive calculation or just standard formulas for biquad (order 2).
// Most edge apps use Biquads.

eif_status_t eif_dsp_design_butterworth(eif_filter_type_t type, int order, float32_t cutoff, float32_t sample_rate, float32_t* b, float32_t* a) {
    if (order != 1 && order != 2) return EIF_STATUS_INVALID_ARGUMENT; // Only support 1st/2nd order for now
    if (!b || !a || sample_rate <= 0 || cutoff <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    float32_t omega = 2.0f * M_PI * cutoff / sample_rate;
    float32_t sn = sinf(omega);
    float32_t cs = cosf(omega);
    float32_t alpha = sn / (2.0f * 0.70710678f); // Q=0.707 for Butterworth
    
    if (order == 1) {
        // 1st order
        // Lowpass: H(s) = 1 / (s + 1) -> Bilinear transform
        // Highpass: H(s) = s / (s + 1)
        
        float32_t tan_w2 = tanf(omega / 2.0f);
        
        if (type == EIF_FILTER_LOWPASS) {
             float32_t c = 1.0f / tan_w2;
             float32_t a0 = c + 1.0f;
             b[0] = 1.0f / a0;
             b[1] = 1.0f / a0;
             a[0] = 1.0f;
             a[1] = (1.0f - c) / a0;
        } else if (type == EIF_FILTER_HIGHPASS) {
             float32_t c = tan_w2;
             float32_t a0 = c + 1.0f;
             b[0] = 1.0f / a0;
             b[1] = -1.0f / a0;
             a[0] = 1.0f;
             a[1] = (c - 1.0f) / a0;
        } else {
            return EIF_STATUS_NOT_IMPLEMENTED;
        }
    } else {
        // 2nd order (Biquad)
        if (type == EIF_FILTER_LOWPASS) {
            float32_t a0 = 1.0f + alpha;
            b[0] = ((1.0f - cs) / 2.0f) / a0;
            b[1] = (1.0f - cs) / a0;
            b[2] = ((1.0f - cs) / 2.0f) / a0;
            a[0] = 1.0f;
            a[1] = (-2.0f * cs) / a0;
            a[2] = (1.0f - alpha) / a0;
        } else if (type == EIF_FILTER_HIGHPASS) {
            float32_t a0 = 1.0f + alpha;
            b[0] = ((1.0f + cs) / 2.0f) / a0;
            b[1] = -(1.0f + cs) / a0;
            b[2] = ((1.0f + cs) / 2.0f) / a0;
            a[0] = 1.0f;
            a[1] = (-2.0f * cs) / a0;
            a[2] = (1.0f - alpha) / a0;
        } else {
            return EIF_STATUS_NOT_IMPLEMENTED;
        }
    }
    
    return EIF_STATUS_OK;
}

// --- Wavelet Transform (Haar) ---

eif_status_t eif_dsp_dwt_haar(const float32_t* input, float32_t* output, int size, eif_memory_pool_t* pool) {
    if (!input || !output || size <= 0 || (size & (size - 1)) != 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Copy input to output as initial state
    memcpy(output, input, size * sizeof(float32_t));
    
    float32_t* temp = (float32_t*)eif_memory_alloc(pool, size * sizeof(float32_t), 4);
    if (!temp) return EIF_STATUS_OUT_OF_MEMORY;
    
    int current_size = size;
    while (current_size > 1) {
        int half_size = current_size / 2;
        
        for (int i = 0; i < half_size; i++) {
            float32_t a = output[2 * i];
            float32_t b = output[2 * i + 1];
            
            // Approximation (Low Pass)
            temp[i] = (a + b) / 1.41421356f; // sqrt(2)
            
            // Detail (High Pass)
            temp[half_size + i] = (a - b) / 1.41421356f;
        }
        
        // Copy back to output
        memcpy(output, temp, current_size * sizeof(float32_t));
        
        current_size = half_size;
    }
    
    // Note: We don't free temp because pool is linear. 
    // Ideally we should use a scratch allocator or scoped pool.
    
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_idwt_haar(const float32_t* input, float32_t* output, int size, eif_memory_pool_t* pool) {
    if (!input || !output || size <= 0 || (size & (size - 1)) != 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    memcpy(output, input, size * sizeof(float32_t));
    
    float32_t* temp = (float32_t*)eif_memory_alloc(pool, size * sizeof(float32_t), 4);
    if (!temp) return EIF_STATUS_OUT_OF_MEMORY;
    
    int current_size = 2;
    while (current_size <= size) {
        int half_size = current_size / 2;
        
        for (int i = 0; i < half_size; i++) {
            float32_t avg = output[i];
            float32_t diff = output[half_size + i];
            
            // Reconstruct
            // a = (avg + diff) / sqrt(2) * sqrt(2) ? No.
            // avg = (a+b)/sqrt(2), diff = (a-b)/sqrt(2)
            // avg + diff = 2a/sqrt(2) = a*sqrt(2) -> a = (avg+diff)/sqrt(2)
            // avg - diff = 2b/sqrt(2) = b*sqrt(2) -> b = (avg-diff)/sqrt(2)
            
            temp[2 * i] = (avg + diff) / 1.41421356f; // Wait, reconstruction factor?
            // Usually orthonormal Haar has factor 1/sqrt(2) for both fwd and inv?
            // Or sqrt(2) for one?
            // If fwd uses 1/sqrt(2), inv uses 1/sqrt(2) too?
            // (a+b)/s2 * 1/s2 + (a-b)/s2 * 1/s2 = (a+b+a-b)/2 = a. Correct.
            
            temp[2 * i] = (avg + diff) / 1.41421356f;
            temp[2 * i + 1] = (avg - diff) / 1.41421356f;
        }
        
        memcpy(output, temp, current_size * sizeof(float32_t));
        current_size *= 2;
    }
    
    return EIF_STATUS_OK;
}
