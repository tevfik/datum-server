/**
 * @file eif_dsp_fft_fixed.c
 * @brief Fixed-Point FFT Implementation
 */

#include "eif_dsp_fft_fixed.h"
#include <math.h> // Only for Init (sin/cos lookup generation)
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// =============================================================================
// Helper: Integer Square Root
// =============================================================================
static q15_t sqrt_q15_local(q31_t val) {
    if (val <= 0) return 0;
    int32_t res = 0;
    int32_t bit = 1 << 30; 
    while (bit > val) bit >>= 2;
    while (bit != 0) {
        if (val >= res + bit) {
            val -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (q15_t)res;
}

// =============================================================================
// Initialization
// =============================================================================
eif_status_t eif_dsp_fft_init_fixed(eif_dsp_fft_fixed_instance_t *S, uint16_t fftLen) {
    if (!S) return EIF_STATUS_ERROR;
    
    // Check power of 2
    if ((fftLen & (fftLen - 1)) != 0) return EIF_STATUS_ERROR;

    S->fftLen = fftLen;

    // 1. Generate Bit Reverse Table
    // We assume S->pBitRevTable points to a buffer of size fftLen (or user provided)
    // Actually, usually the library manages memory? 
    // Here we assume the USER allocated buffers in S->pTwiddle and S->pBitRevTable 
    // before calling Init. The struct has pointers, not arrays.
    // If they are NULL, returns error (unless we switch to malloc).
    
    // For this implementation, we require user to provide buffers or we fail.
    if (!S->pTwiddle || !S->pBitRevTable) {
        return EIF_STATUS_OUT_OF_MEMORY; 
    }

    uint16_t levels = 0;
    uint16_t temp = fftLen;
    while (temp >>= 1) levels++;

    for (uint16_t i = 0; i < fftLen; i++) {
        uint16_t rev = 0;
        uint16_t val = i;
        for (uint16_t j = 0; j < levels; j++) {
            rev = (rev << 1) | (val & 1);
            val >>= 1;
        }
        S->pBitRevTable[i] = rev;
    }
    S->bitRevLength = fftLen;

    // 2. Generate Twiddle Factors
    // W_N^k = e^(-j * 2*pi * k / N) = cos(theta) - j*sin(theta)
    // k goes 0 to N/2 - 1 ? 
    // Radix-2: We need N/2 complex factors.
    // Format: Real, Imag, Real, Imag...
    
    for (uint16_t k = 0; k < fftLen / 2; k++) {
        float theta = -2.0f * M_PI * (float)k / (float)fftLen;
        float c = cosf(theta);
        float s = sinf(theta);
        
        // Convert with clamping to avoid 1.0 -> -32768 overflow
        int32_t c_int = (int32_t)(c * 32768.0f);
        if (c_int > 32767) c_int = 32767;
        if (c_int < -32768) c_int = -32768;

        int32_t s_int = (int32_t)(s * 32768.0f);
        if (s_int > 32767) s_int = 32767;
        if (s_int < -32768) s_int = -32768;
        
        S->pTwiddle[2 * k]     = (q15_t)c_int;
        S->pTwiddle[2 * k + 1] = (q15_t)s_int;
    }

    return EIF_STATUS_OK;
}

// =============================================================================
// Core FFT Butterfly
// =============================================================================
void eif_dsp_fft_c15(const eif_dsp_fft_fixed_instance_t *S, 
                     q15_t *pSrc, 
                     q15_t *pDst, 
                     uint8_t inverse) {
    
    uint16_t n = S->fftLen;
    
    // 1. Bit Reversal Permutation
    // Copy src to dst with reordering
    for (uint16_t i = 0; i < n; i++) {
        uint16_t rev = S->pBitRevTable[i];
        pDst[2 * i]     = pSrc[2 * rev];
        pDst[2 * i + 1] = pSrc[2 * rev + 1];
    }
    
    // 2. Cooley-Tukey Butterfly Stages
    // For each stage
    for (uint16_t size = 2; size <= n; size <<= 1) {
        uint16_t half_size = size >> 1;
        uint16_t table_step = n / size; // Stride in Twiddle table
        
        // For each block of size 'size'
        for (uint16_t i = 0; i < n; i += size) {
            
            // For each butterfly in the block
            for (uint16_t j = 0; j < half_size; j++) {
                
                // Indices
                uint16_t k = j * table_step; // Twiddle index (0..N/2)
                
                // Twiddle Factor (W)
                // If inverse, Imag part of W is negated? 
                // W = cos - j*sin. Inverse W = cos + j*sin.
                q15_t wr = S->pTwiddle[2 * k];
                q15_t wi = S->pTwiddle[2 * k + 1];
                
                if (inverse) wi = -wi;

                // Indices in pDst
                uint16_t even_idx = 2 * (i + j);
                uint16_t odd_idx  = 2 * (i + j + half_size);
                
                // Data
                q15_t ar = pDst[even_idx];
                q15_t ai = pDst[even_idx + 1];
                q15_t br = pDst[odd_idx];
                q15_t bi = pDst[odd_idx + 1];
                
                // Multiply W * B (Complex Mul Q15)
                // (br + j bi) * (wr + j wi) = (br*wr - bi*wi) + j (br*wi + bi*wr)
                // Intermediate Q31
                q31_t t_r_31 = ((q31_t)br * wr) - ((q31_t)bi * wi);
                q31_t t_i_31 = ((q31_t)br * wi) + ((q31_t)bi * wr);
                
                // Shift back to Q15
                q15_t tr = (q15_t)(t_r_31 >> 15);
                q15_t ti = (q15_t)(t_i_31 >> 15);
                
                // Butterfly Update (with scaling by >> 1 to prevent overflow)
                // A' = (A + T) / 2
                // B' = (A - T) / 2
                
                pDst[even_idx]     = (ar + tr + 1) >> 1;
                pDst[even_idx + 1] = (ai + ti + 1) >> 1;
                
                pDst[odd_idx]      = (ar - tr + 1) >> 1;
                pDst[odd_idx + 1]  = (ai - ti + 1) >> 1;
            }
        }
    }
}

// =============================================================================
// Magnitude
// =============================================================================
void eif_dsp_cmplx_mag_q15(const q15_t *pSrc, 
                           q15_t *pDst, 
                           uint32_t numSamples) {
    for (uint32_t i = 0; i < numSamples; i++) {
        q15_t r = pSrc[2 * i];
        q15_t im = pSrc[2 * i + 1];
        // r^2 + im^2 can ideally overflow Q31 if q15 is full scale? 
        // max 1^2 + 1^2 = 2. Q30 2.0 overflows?
        // Yes. (INT16_MAX)^2 * 2 ~ 2 * 10^9 < INT32_MAX (2.14 * 10^9). 
        // So Q15^2 + Q15^2 fits in Q31 barely.
        
        q31_t sum = ((q31_t)r * r) + ((q31_t)im * im);
        
        // sqrt(sum) is in Q15 units?
        // sum is Q30. sqrt(Q30) -> Q15.
        // sqrt_q15_local expects integer input?
        // If we treat sum as raw integer: sqrt(r_raw^2 + im_raw^2) = result_raw
        // Yes, scaling matches.
        
        pDst[i] = sqrt_q15_local(sum);
    }
}
