#ifndef EIF_DSP_PEAK_H
#define EIF_DSP_PEAK_H

#include "eif_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Robust Peak Detector
// ============================================================================
// Inspired by Pan-Tompkins for biomedical signals but generic

typedef struct {
    float32_t threshold;
    float32_t moving_avg; // Adaptive threshold state
    int refractory_period; // Min samples between peaks
    int refractory_counter;
    float32_t alpha; // Adaptation rate for threshold
} eif_robust_peak_t;

/**
 * @brief Initialize robust peak detector
 * @param ctx Context
 * @param fs Sample rate
 * @param min_peak_dist_ms Minimum distance between peaks in ms (refractory period)
 */
void eif_robust_peak_init(eif_robust_peak_t* ctx, float32_t fs, float32_t min_peak_dist_ms);

/**
 * @brief Process a single sample
 * @return true if peak detected
 */
bool eif_robust_peak_update(eif_robust_peak_t* ctx, float32_t sample);

/**
 * @brief Process buffer and return indices of detected peaks
 * @param output_indices Array to store indices
 * @param max_peaks Capacity of indices array
 * @return Number of peaks found
 */
int eif_robust_peak_process_buffer(eif_robust_peak_t* ctx, const float32_t* input, size_t length, int* output_indices, int max_peaks);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_PEAK_H
