#ifndef EIF_DSP_GENERATOR_H
#define EIF_DSP_GENERATOR_H

#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Signal Generator
// ============================================================================

typedef enum {
    EIF_SIG_SINE,
    EIF_SIG_SQUARE,
    EIF_SIG_TRIANGLE,
    EIF_SIG_SAWTOOTH,
    EIF_SIG_WHITE_NOISE
} eif_signal_type_t;

typedef struct {
    eif_signal_type_t type;
    float32_t frequency;
    float32_t sample_rate;
    float32_t amplitude;
    float32_t phase; // Internal state
} eif_signal_gen_t;

void eif_signal_gen_init(eif_signal_gen_t* ctx, eif_signal_type_t type, float32_t freq, float32_t fs, float32_t amp);
float32_t eif_signal_gen_next(eif_signal_gen_t* ctx);
void eif_signal_gen_fill(eif_signal_gen_t* ctx, float32_t* buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_GENERATOR_H
