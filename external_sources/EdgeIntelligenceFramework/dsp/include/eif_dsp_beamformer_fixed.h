/**
 * @file eif_dsp_beamformer_fixed.h
 * @brief Fixed-Point Beamformer (Delay-and-Sum)
 */

#ifndef EIF_DSP_BEAMFORMER_FIXED_H
#define EIF_DSP_BEAMFORMER_FIXED_H

#include "eif_fixedpoint.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  q15_t mic_dist_mm; // Millimeters to fit in Q15? No, Q15 range is small.
                     // Dist is usually < 1m. Q15 1.0 = 1 meter?
                     // Or just keep dist/rate in standard int/float for setup?
                     // Setup is usually infrequent.
                     // Let's use standard types for config to avoid headache,
                     // only process path needs to be Q15.
  uint32_t sample_rate;
  int delay_samples;
} eif_beamformer_q15_t;

/**
 * @brief Initialize Beamformer
 * @param mic_dist_m Microphone distance in meters (float for config
 * convenience)
 * @param target_angle_deg Target angle (float for config convenience)
 */
void eif_beamformer_q15_init(eif_beamformer_q15_t *ctx, float mic_dist_m,
                             uint32_t sample_rate, float target_angle_deg);

/**
 * @brief Process Stereo Interleaved (L=Mic1, R=Mic2)
 * @param input Interleaved Q15
 * @param output Mono Q15
 */
void eif_beamformer_q15_process_stereo(eif_beamformer_q15_t *ctx,
                                       const q15_t *input, q15_t *output,
                                       size_t num_frames);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_BEAMFORMER_FIXED_H
