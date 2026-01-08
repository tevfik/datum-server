/**
 * @file eif_dsp_window.h
 * @brief Window Functions for DSP
 */

#ifndef EIF_DSP_WINDOW_H
#define EIF_DSP_WINDOW_H

#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate Hamming Window
 */
void eif_dsp_window_hamming_f32(float32_t *window, size_t length);

/**
 * @brief Generate Hanning Window
 */
void eif_dsp_window_hanning_f32(float32_t *window, size_t length);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_WINDOW_H
