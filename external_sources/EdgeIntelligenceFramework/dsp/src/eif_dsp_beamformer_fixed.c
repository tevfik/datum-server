/**
 * @file eif_dsp_beamformer_fixed.c
 * @brief Fixed-Point Beamformer Implementation
 */

#include "eif_dsp_beamformer_fixed.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define SPEED_OF_SOUND 343.0f

void eif_beamformer_q15_init(eif_beamformer_q15_t *ctx, float mic_dist_m,
                             uint32_t sample_rate, float target_angle_deg) {
  if (!ctx)
    return;

  // Config logic calculation (can be float, it's init time)
  float angle_rad = target_angle_deg * (M_PI / 180.0f);
  float tdoa = (mic_dist_m * sinf(angle_rad)) / SPEED_OF_SOUND;

  ctx->sample_rate = sample_rate;
  ctx->delay_samples = (int)(tdoa * sample_rate);
}

void eif_beamformer_q15_process_stereo(eif_beamformer_q15_t *ctx,
                                       const q15_t *input, q15_t *output,
                                       size_t num_frames) {
  if (!ctx || !input || !output)
    return;

  int delay = ctx->delay_samples;

  // Assume L = Mic1 (Reference), R = Mic2 (Delayed or Advanced)
  // If delay > 0: Signal hits Mic2 BEFORE Mic1? No.
  // TDOA = (d * sin(theta)) / c.
  // If theta > 0 (Source right), hits Mic2 first?
  // Standard broadside: 0 deg.
  // Let's implement standard Delay-And-Sum.
  // y[n] = 0.5 * (x1[n] + x2[n - delay])

  // NOTE: Without a circular buffer state, we can't implement TRUE delay across
  // blocks. The float implementation also had this limitation (simple
  // placeholder). We will mimic that behavioral placeholder but strictly in
  // Q15.

  for (size_t i = 0; i < num_frames; i++) {
    q15_t mic1 = input[2 * i];
    q15_t mic2 = input[2 * i + 1];

    q15_t delayed_mic2;

    if (delay == 0) {
      delayed_mic2 = mic2;
    } else {
      // "Look back" or "Look forward" in the current buffer
      // Since we don't have history, we clamp to edges or pad with 0
      int idx = (int)i - delay;
      if (idx >= 0 && idx < (int)num_frames) {
        delayed_mic2 = input[2 * idx + 1]; // R channel at delayed index
      } else {
        delayed_mic2 = 0; // Padding
      }
    }

    // Sum and Average (>> 1)
    // (mic1 + delayed_mic2) / 2
    // Use Q31 intermediate to avoid overflow before shift?
    // Or (a>>1) + (b>>1). Latter loses 1 bit precision.
    // Q15+Q15 can fit in int32.
    int32_t sum = (int32_t)mic1 + delayed_mic2;
    output[i] = (q15_t)(sum / 2); // Divide by 2
  }
}
