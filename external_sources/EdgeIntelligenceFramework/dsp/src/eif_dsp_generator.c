#include "eif_dsp_generator.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void eif_signal_gen_init(eif_signal_gen_t* ctx, eif_signal_type_t type, float32_t freq, float32_t fs, float32_t amp) {
    ctx->type = type;
    ctx->frequency = freq;
    ctx->sample_rate = fs;
    ctx->amplitude = amp;
    ctx->phase = 0.0f;
}

float32_t eif_signal_gen_next(eif_signal_gen_t* ctx) {
    float32_t val = 0.0f;
    float32_t phase = ctx->phase;
    
    switch (ctx->type) {
        case EIF_SIG_SINE:
            val = sinf(phase);
            break;
        case EIF_SIG_SQUARE:
            val = (phase < M_PI) ? 1.0f : -1.0f;
            break;
        case EIF_SIG_TRIANGLE:
            // Normalize phase to 0..1 range approx or use formula
            // -1 to 1 linear
            if (phase < M_PI) 
                val = -1.0f + (2.0f * phase / M_PI);
            else 
                val = 3.0f - (2.0f * phase / M_PI);
            break;
        case EIF_SIG_SAWTOOTH:
            val = -1.0f + (phase / M_PI); // Simple
            break;
        case EIF_SIG_WHITE_NOISE:
            val = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            break;
    }
    
    // Advance phase
    float32_t phase_inc = 2.0f * M_PI * ctx->frequency / ctx->sample_rate;
    ctx->phase += phase_inc;
    if (ctx->phase >= 2.0f * M_PI) ctx->phase -= 2.0f * M_PI;
    
    return val * ctx->amplitude;
}

void eif_signal_gen_fill(eif_signal_gen_t* ctx, float32_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = eif_signal_gen_next(ctx);
    }
}
