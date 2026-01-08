#ifndef EIF_DSP_TRANSFORM_H
#define EIF_DSP_TRANSFORM_H

#include "eif_types.h"
#include "eif_status.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Short-Time Fourier Transform (STFT)
// ============================================================================

typedef struct {
    int fft_size;
    int hop_size;
    int window_size;
    float32_t* window_func;
    float32_t* internal_buffer; // Overlap buffer
} eif_stft_t;

eif_status_t eif_stft_init(eif_stft_t* ctx, int fft_size, int hop_size, int window_size);
void eif_stft_free(eif_stft_t* ctx); // Note: Project uses pool, this might need pool adjustment
// For this framework, we assume pool allocation usually, but here we define init/process.

// ============================================================================
// Goertzel Algorithm
// ============================================================================
// Efficient single-tone detection

typedef struct {
    float32_t coeff;
    float32_t q1;
    float32_t q2;
} eif_goertzel_t;

void eif_goertzel_init(eif_goertzel_t* ctx, float32_t target_freq, float32_t sample_rate);
void eif_goertzel_process_sample(eif_goertzel_t* ctx, float32_t sample);
float32_t eif_goertzel_compute_magnitude(eif_goertzel_t* ctx);
void eif_goertzel_reset(eif_goertzel_t* ctx);

// ============================================================================
// Wavelet Transform (DWT)
// ============================================================================
// Haar Wavelet - Single Level

void eif_dwt_haar_forward(const float32_t* input, float32_t* low_pass, float32_t* high_pass, size_t length);
void eif_dwt_haar_inverse(const float32_t* low_pass, const float32_t* high_pass, float32_t* output, size_t length);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_TRANSFORM_H
