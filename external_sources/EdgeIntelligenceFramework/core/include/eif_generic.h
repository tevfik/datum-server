#ifndef EIF_GENERIC_H
#define EIF_GENERIC_H

#include "eif_types.h"
#include "eif_matrix.h"
#include "eif_matrix_fixed.h"
#include "eif_dsp.h"
#include "eif_dsp_fixed.h"

// --- Matrix Operations ---

// --- Matrix Operations ---

// Matrix Addition
#define eif_mat_add(A, B, C, rows, cols) _Generic((A), \
    float32_t*: eif_mat_add_f32, \
    const float32_t*: eif_mat_add_f32, \
    q15_t*: eif_mat_add_q15, \
    const q15_t*: eif_mat_add_q15 \
)(A, B, C, rows, cols)

// Matrix Subtraction
#define eif_mat_sub(A, B, C, rows, cols) _Generic((A), \
    float32_t*: eif_mat_sub_f32, \
    const float32_t*: eif_mat_sub_f32, \
    q15_t*: eif_mat_sub_q15, \
    const q15_t*: eif_mat_sub_q15 \
)(A, B, C, rows, cols)

// Matrix Multiplication
#define eif_mat_mul(A, B, C, rA, cA, cB) _Generic((A), \
    float32_t*: eif_mat_mul_f32, \
    const float32_t*: eif_mat_mul_f32, \
    q15_t*: eif_mat_mul_q15, \
    const q15_t*: eif_mat_mul_q15 \
)(A, B, C, rA, cA, cB)

// Matrix Transpose
#define eif_mat_transpose(A, AT, rows, cols) _Generic((A), \
    float32_t*: eif_mat_transpose_f32, \
    const float32_t*: eif_mat_transpose_f32, \
    q15_t*: eif_mat_transpose_q15, \
    const q15_t*: eif_mat_transpose_q15 \
)(A, AT, rows, cols)

// Matrix Inverse
#define eif_mat_inverse(A, Inv, n, pool) _Generic((A), \
    float32_t*: eif_mat_inverse_f32, \
    const float32_t*: eif_mat_inverse_f32, \
    q15_t*: eif_mat_inverse_q15, \
    const q15_t*: eif_mat_inverse_q15 \
)(A, Inv, n, pool)

// Matrix Cholesky
#define eif_mat_cholesky(A, L, n) _Generic((A), \
    float32_t*: eif_mat_cholesky_f32, \
    const float32_t*: eif_mat_cholesky_f32, \
    q15_t*: eif_mat_cholesky_q15, \
    const q15_t*: eif_mat_cholesky_q15 \
)(A, L, n)

// Matrix Scale
#define eif_mat_scale(A, scale, C, rows, cols) _Generic((A), \
    float32_t*: eif_mat_scale_f32, \
    const float32_t*: eif_mat_scale_f32, \
    q15_t*: eif_mat_scale_q15, \
    const q15_t*: eif_mat_scale_q15 \
)(A, scale, C, rows, cols)

// Matrix Identity
#define eif_mat_identity(I, n) _Generic((I), \
    float32_t*: eif_mat_identity_f32, \
    q15_t*: eif_mat_identity_q15 \
)(I, n)

// Matrix Copy
#define eif_mat_copy(Src, Dst, rows, cols) _Generic((Src), \
    float32_t*: eif_mat_copy_f32, \
    const float32_t*: eif_mat_copy_f32, \
    q15_t*: eif_mat_copy_q15, \
    const q15_t*: eif_mat_copy_q15 \
)(Src, Dst, rows, cols)

// --- DSP Operations ---

// FFT
#define eif_dsp_fft_init(config, len) _Generic((config), \
    eif_fft_config_t*: eif_dsp_fft_init_f32, \
    eif_fft_config_q15_t*: eif_dsp_fft_init_q15 \
)(config, len)

#define eif_dsp_fft_deinit(config) _Generic((config), \
    eif_fft_config_t*: eif_dsp_fft_deinit_f32, \
    eif_fft_config_q15_t*: eif_dsp_fft_deinit_q15 \
)(config)

#define eif_dsp_fft(config, data) _Generic((data), \
    float32_t*: eif_dsp_fft_f32, \
    q15_t*: eif_dsp_fft_q15 \
)((void*)config, data) 

// RFFT
#define eif_dsp_rfft(config, input, output) _Generic((input), \
    float32_t*: eif_dsp_rfft_f32, \
    q15_t*: eif_dsp_rfft_q15 \
)((void*)config, input, output)

// Magnitude
#define eif_dsp_magnitude(input, output, len) _Generic((input), \
    float32_t*: eif_dsp_magnitude_f32, \
    q15_t*: eif_dsp_magnitude_q15 \
)(input, output, len)

// Windowing (Hamming)
#define eif_dsp_window_hamming(window, len) _Generic((window), \
    float32_t*: eif_dsp_window_hamming_f32, \
    q15_t*: eif_dsp_window_hamming_q15 \
)(window, len)

// Windowing (Hanning)
#define eif_dsp_window_hanning(window, len) _Generic((window), \
    float32_t*: eif_dsp_window_hanning_f32, \
    q15_t*: eif_dsp_window_hanning_q15 \
)(window, len)

// Stats (RMS)
#define eif_dsp_rms(input, len) _Generic((input), \
    float32_t*: eif_dsp_rms_f32, \
    q15_t*: eif_dsp_rms_q15 \
)(input, len)

// Stats (ZCR)
#define eif_dsp_zcr(input, len) _Generic((input), \
    float32_t*: eif_dsp_zcr_f32, \
    q15_t*: eif_dsp_zcr_q15 \
)(input, len)

// Filters (FIR)
#define eif_dsp_fir(in, out, len, coeffs, taps) _Generic((in), \
    float32_t*: eif_dsp_fir_f32, \
    q15_t*: eif_dsp_fir_q15 \
)(in, out, len, coeffs, taps)

// Filters (IIR)
#define eif_dsp_iir(in, out, len, coeffs, state, stages) _Generic((in), \
    float32_t*: eif_dsp_iir_f32, \
    q15_t*: eif_dsp_iir_q15 \
)(in, out, len, coeffs, state, stages)

#endif // EIF_GENERIC_H
