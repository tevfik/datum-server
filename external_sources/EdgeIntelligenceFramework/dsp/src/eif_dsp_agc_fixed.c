/**
 * @file eif_dsp_agc_fixed.c
 * @brief Fixed-Point AGC Implementation
 */

#include "eif_dsp_agc_fixed.h"

// We use Q4.11 for Gain to allow up to 16x amplification (which is plenty for
// AGC). 1.0 in Q4.11 is 1 << 11 = 2048.
#define GAIN_UNITY 2048
#define GAIN_SHIFT 11

static inline q15_t eif_abs_q15(q15_t x) {
  if (x == -32768)
    return 32767;
  return (x < 0) ? -x : x;
}

void eif_agc_q15_init(eif_agc_q15_t *ctx, q15_t target, q15_t max_gain) {
  if (!ctx)
    return;
  ctx->target_level = target > 0 ? target : 16384; // Default 0.5
  ctx->max_gain =
      max_gain; // Q4.11 format expected? No, usually user passes Q15
                // equivalent. Let's assume user passes max_gain in same format
                // as Gain (Q4.11). Or simply Q15 (1.0 = 32768) and we limit it?
                // If max_gain > 1.0 is needed, user must understand scale.
                // Let's assume default max gain is 4.0 -> 8192 in Q4.11.
  if (max_gain == 0)
    ctx->max_gain = 2048; // Unity default
  else
    ctx->max_gain = max_gain;

  ctx->current_gain = GAIN_UNITY;
  ctx->attack = 327;     // ~0.01 in Q15
  ctx->decay = 32;       // ~0.001 in Q15
  ctx->noise_gate = 655; // ~0.02 in Q15
}

void eif_agc_q15_process(eif_agc_q15_t *ctx, const q15_t *input, q15_t *output,
                         size_t length) {
  if (!ctx || !input || !output)
    return;

  q15_t target = ctx->target_level;
  q15_t gain = ctx->current_gain;
  q15_t max_g = ctx->max_gain;
  q15_t noise_floor = ctx->noise_gate;
  q15_t attack = ctx->attack;
  q15_t decay = ctx->decay;

  for (size_t i = 0; i < length; i++) {
    q15_t in_val = input[i];
    q15_t abs_in = eif_abs_q15(in_val);

    // Apply Gain: Input(Q15) * Gain(Q4.11) = Q19.11
    // Shift right by 11 to get Q15? No.
    // Q0.15 * Q4.11 -> Q4.26.
    // Desired Q0.15. Shift by 11: Q4.15. Then saturate.

    q31_t product = (q31_t)in_val * gain;
    // product is Q4.26 (conceptually).
    // e.g. 1.0 * 1.0 = 32768 * 2048 = 67,108,864
    // To get 1.0 (32768) we shift by 11: 32768. Correct.

    q31_t out_32 = product >> GAIN_SHIFT;

    // Saturation
    if (out_32 > 32767)
      out_32 = 32767;
    if (out_32 < -32768)
      out_32 = -32768;
    q15_t out_val = (q15_t)out_32;
    q15_t abs_out = eif_abs_q15(out_val);

    output[i] = out_val;

    // Update Gain
    if (abs_in > noise_floor) {
      // Error = Target - |Output|
      q15_t error = target - abs_out;

      // Logic:
      // if too loud (error negative), reduce gain (Attack)
      // if too quiet (error positive), increase gain (Decay)

      q31_t delta = 0;
      if (error < 0) {
        // negative error * attack (Q15) -> Q30 negative
        delta = ((q31_t)error * attack) >> 15;
      } else {
        delta = ((q31_t)error * decay) >> 15;
      }
      // gain is Q4.11. delta is Q15 scale of error.
      // Error was Q15. Attack is Q15. Result Q15.
      // Gain is Q4.11. We need to match scales?
      // Gain is amplitude scale.
      // If delta is added directly to gain (Q4.11), we are mixing units.
      // But P-control works by proportional addition.
      // Let's assume simple addition works for adaptation.
      gain += (q15_t)delta;
    }

    // Clamp gain
    if (gain < GAIN_UNITY)
      gain = GAIN_UNITY; // Don't attenuate below unity for AGC?
                         // Often AGC can attenuate. But ctx default min is 1.0
                         // in float code? "if (gain < 1.0f) gain = 1.0f;" ->
                         // Yes, float code clamps min 1.0.
    if (gain > max_g)
      gain = max_g;
  }

  ctx->current_gain = gain;
}
