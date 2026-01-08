/**
 * @file eif_time_series_fixed.c
 * @brief Time Series Forecasting (Fixed Point) Implementation
 */

#include "eif_time_series_fixed.h"

// SES Implementation
void eif_ses_init_fixed(eif_ses_fixed_t *ses, q15_t alpha) {
    ses->alpha = alpha;
    ses->level = 0;
    ses->initialized = false;
}

q15_t eif_ses_update_fixed(eif_ses_fixed_t *ses, q15_t obs) {
    if (!ses->initialized) {
        ses->level = obs;
        ses->initialized = true;
        return obs;
    }

    // Formula: L_t = alpha * x_t + (1 - alpha) * L_{t-1}
    // Fixed point: 
    // Q15 alpha * Q15 obs -> Q30
    // Q15 (1-alpha) * Q15 level -> Q30
    // Sum >> 15 -> Q15

    // In Q15, 1.0 is 32768 (technically 32767 is max, so we use slightly approx or treat logic carefully)
    // To be precise: If alpha is 0.1 (3276), then 1.0 - alpha should be 32768 - 3276.
    // However, signal range logic:
    // Let's use 32767 as "almost 1.0". Or use 32768 conceptually in calculations.
    
    int32_t one_q15 = 32768;
    int32_t term1 = (int32_t)ses->alpha * (int32_t)obs;
    int32_t term2 = (int32_t)(one_q15 - ses->alpha) * (int32_t)ses->level;
    
    // Result is Q30
    int32_t result_q30 = term1 + term2;
    
    // Convert back to Q15 with rounding
    ses->level = (q15_t)((result_q30 + 16384) >> 15);
    
    return ses->level;
}

// SMA Implementation
void eif_sma_init_fixed(eif_sma_fixed_t *sma, q15_t *buffer, uint16_t size) {
    sma->buffer = buffer;
    sma->size = size;
    sma->head = 0;
    sma->sum = 0;
    sma->count = 0;
}

q15_t eif_sma_update_fixed(eif_sma_fixed_t *sma, q15_t val) {
    if (sma->count < sma->size) {
        // Filling phase
        sma->buffer[sma->head] = val;
        sma->sum += val;
        sma->count++;
    } else {
        // Sliding window
        // Remove old
        sma->sum -= sma->buffer[sma->head];
        // Add new
        sma->buffer[sma->head] = val;
        sma->sum += val;
    }
    
    // Advance head
    sma->head = (sma->head + 1) % sma->size;
    
    // Average
    // Division is slow. Optimized by reciprocal multiplication usually.
    // Here simple integer division.
    if (sma->count == 0) return 0;
    return (q15_t)(sma->sum / sma->count);
}
