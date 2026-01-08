#ifndef EIF_DSP_BEAMFORMER_H
#define EIF_DSP_BEAMFORMER_H

#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Delay-and-Sum Beamformer
// ============================================================================

typedef struct {
    float32_t mic_dist_m;     // Distance between microphones in meters
    uint32_t sample_rate;
    int delay_samples;        // Calculated static delay for target angle
} eif_beamformer_t;

/**
 * @brief Initialize 2-mic beamformer
 * Target angle is relative to broadside (0 = perpendicular to array)
 */
void eif_beamformer_init(eif_beamformer_t* ctx, float32_t mic_dist, uint32_t sample_rate, float32_t target_angle_deg);

/**
 * @brief Separate delay calculation for dynamic steering
 */
void eif_beamformer_steer(eif_beamformer_t* ctx, float32_t target_angle_deg);

/**
 * @brief Process stereo input
 * Assumes input is interleaved: L, R, L, R...
 * Output is mono.
 */
void eif_beamformer_process_stereo(eif_beamformer_t* ctx, const float32_t* input, float32_t* output, size_t num_frames);

/**
 * @brief Process separate channel buffers
 */
void eif_beamformer_process_dual(eif_beamformer_t* ctx, const float32_t* mic1, const float32_t* mic2, 
                                 float32_t* output, size_t length);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_BEAMFORMER_H
