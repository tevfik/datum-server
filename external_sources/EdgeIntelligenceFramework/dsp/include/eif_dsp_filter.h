/**
 * @file eif_dsp_filter.h
 * @brief Digital Filters
 * 
 * FIR, IIR filters and filter design.
 */

#ifndef EIF_DSP_FILTER_H
#define EIF_DSP_FILTER_H

#include "eif_types.h"
#include "eif_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FIR and IIR Filters
// ============================================================================

eif_status_t eif_dsp_fir_f32(const float32_t* input, float32_t* output, size_t length, 
                              const float32_t* coeffs, size_t taps);
eif_status_t eif_dsp_iir_f32(const float32_t* input, float32_t* output, size_t length, 
                              const float32_t* coeffs, float32_t* state, size_t stages);

// ============================================================================
// Windowing Functions
// ============================================================================

void eif_dsp_window_hamming_f32(float32_t* window, size_t length);
void eif_dsp_window_hanning_f32(float32_t* window, size_t length);

// ============================================================================
// Filter Design
// ============================================================================

typedef enum {
    EIF_FILTER_LOWPASS,
    EIF_FILTER_HIGHPASS,
    EIF_FILTER_BANDPASS
} eif_filter_type_t;

eif_status_t eif_dsp_design_butterworth(eif_filter_type_t type, int order, float32_t cutoff, 
                                         float32_t sample_rate, float32_t* b, float32_t* a);

// ============================================================================
// Resampling
// ============================================================================

eif_status_t eif_dsp_resample_linear_f32(const float32_t* input, size_t input_len, 
                                          float32_t* output, size_t output_len);
eif_status_t eif_dsp_resample_cubic_f32(const float32_t* input, size_t input_len, 
                                         float32_t* output, size_t output_len);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_FILTER_H
