#ifndef EIF_DSP_AGC_H
#define EIF_DSP_AGC_H

#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Automatic Gain Control (AGC)
// ============================================================================

typedef struct {
    float32_t target_level;   // Desired output level (e.g., 0.5 or -6dB)
    float32_t max_gain;       // Maximum amplification allowed (e.g., 30.0)
    float32_t current_gain;   // Internal state
    float32_t attack;         // Coefficient for gain increase (fast)
    float32_t decay;          // Coefficient for gain decrease (slow)
    float32_t noise_gate;     // Minimum level to amplify (avoid amplifying silence)
} eif_agc_t;

/**
 * @brief Initialize AGC
 * @param ctx Context
 * @param target Target peak level (0.0 - 1.0)
 * @param max_gain Max gain multiplier
 * @param kp Proportional constant (responsiveness)
 */
void eif_agc_init(eif_agc_t* ctx, float32_t target, float32_t max_gain);

/**
 * @brief Process audio block with AGC
 */
void eif_agc_process(eif_agc_t* ctx, const float32_t* input, float32_t* output, size_t length);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_AGC_H
