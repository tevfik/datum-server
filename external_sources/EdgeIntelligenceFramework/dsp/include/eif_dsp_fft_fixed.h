/**
 * @file eif_dsp_fft_fixed.h
 * @brief Fixed-Point FFT Implementation (Q15/Q31)
 *
 * Provides Radix-2 / Radix-4 FFT implementation optimized for Cortex-M.
 * Uses Q15 fixed-point arithmetic with block floating point scaling usually.
 * or simply scaled fixed point.
 */

#ifndef EIF_DSP_FFT_FIXED_H
#define EIF_DSP_FFT_FIXED_H

#include "eif_fixedpoint.h"
#include "eif_status.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Twiddle factors will be pre-calculated or LUT based.

/**
 * @brief Fixed-point FFT Instance Structure
 */
typedef struct {
    uint16_t fftLen;        // FFT Length (must be power of 2)
    q15_t *pTwiddle;        // Points to Twiddle Factor Table (Q15) - populated by Init
    uint16_t *pBitRevTable; // Points to Bit Reverse Table - populated by Init
    uint16_t bitRevLength;  // Bit Reverse Table Length
} eif_dsp_fft_fixed_instance_t;

/**
 * @brief Initialize the Fixed Point FFT
 * @param[in,out] S      Points to an instance of the fixed point FFT structure.
 * @param[in]     fftLen Length of the FFT.
 * @return EIF_STATUS_OK if successful.
 */
eif_status_t eif_dsp_fft_init_fixed(eif_dsp_fft_fixed_instance_t *S, uint16_t fftLen);

/**
 * @brief Processing function for the Q15 Complex FFT.
 * @param[in]  S    Points to an instance of the fixed point FFT structure.
 * @param[in]  pSrc Points to input buffer (interleaved complex: real, imag, real, imag...).
 * @param[out] pDst Points to output buffer.
 * @param[in]  inverse If true, perform IFFT.
 */
void eif_dsp_fft_c15(const eif_dsp_fft_fixed_instance_t *S, 
                     q15_t *pSrc, 
                     q15_t *pDst, 
                     uint8_t inverse);

/**
 * @brief Calculate magnitude of complex Q15 vector
 * @param[in]  pSrc Points to input vector (complex interleaved)
 * @param[out] pDst Points to output vector (real)
 * @param[in]  numSamples Number of complex samples
 */
void eif_dsp_cmplx_mag_q15(const q15_t *pSrc, 
                           q15_t *pDst, 
                           uint32_t numSamples);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_FFT_FIXED_H
