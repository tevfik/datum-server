/**
 * @file eif_dsp_fft.h
 * @brief FFT and Spectral Transforms
 * 
 * Fast Fourier Transform, Real FFT, STFT, and related operations.
 */

#ifndef EIF_DSP_FFT_H
#define EIF_DSP_FFT_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FFT Configuration
// ============================================================================

typedef struct {
    uint16_t length;
    bool inverse;
    uint16_t* bit_reverse_indices;
    float32_t* twiddle_factors;
    float32_t* rfft_scratch;
} eif_fft_config_t;

eif_status_t eif_dsp_fft_init_f32(eif_fft_config_t* config, uint16_t length, eif_memory_pool_t* pool);
eif_status_t eif_dsp_fft_deinit_f32(eif_fft_config_t* config);
eif_status_t eif_dsp_fft_f32(const eif_fft_config_t* config, float32_t* data);
eif_status_t eif_dsp_rfft_f32(const eif_fft_config_t* config, const float32_t* input_real, float32_t* output_complex);
eif_status_t eif_dsp_magnitude_f32(const float32_t* input_complex, float32_t* output_mag, size_t length);

// ============================================================================
// STFT (Short-Time Fourier Transform)
// ============================================================================

typedef struct {
    uint16_t fft_length;
    uint16_t hop_length;
    uint16_t window_length;
    float32_t* window;
    float32_t* fft_buffer;
    eif_fft_config_t fft_config;
} eif_stft_config_t;

eif_status_t eif_dsp_stft_init_f32(eif_stft_config_t* config, uint16_t fft_length, 
                                   uint16_t hop_length, uint16_t window_length, eif_memory_pool_t* pool);
eif_status_t eif_dsp_stft_deinit_f32(eif_stft_config_t* config);
eif_status_t eif_dsp_stft_compute_f32(const eif_stft_config_t* config, const float32_t* input, 
                                       float32_t* output_mag, size_t num_frames);

// ============================================================================
// Wavelet Transform
// ============================================================================

eif_status_t eif_dsp_dwt_haar(const float32_t* input, float32_t* output, int size, eif_memory_pool_t* pool);
eif_status_t eif_dsp_idwt_haar(const float32_t* input, float32_t* output, int size, eif_memory_pool_t* pool);

// ============================================================================
// SIMD Helpers
// ============================================================================

void eif_fft_process_stage_simd(int n, int size, int half_size, int table_step, 
                                float32_t* data, const float32_t* twiddles, bool inverse);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_FFT_H
