#include "eif_dsp_agc.h"
#include <math.h>

void eif_agc_init(eif_agc_t* ctx, float32_t target, float32_t max_gain) {
    if (!ctx) return;
    ctx->target_level = target > 0 ? target : 0.5f;
    ctx->max_gain = max_gain > 1.0f ? max_gain : 1.0f;
    ctx->current_gain = 1.0f;
    ctx->attack = 0.01f; // Adjust as needed
    ctx->decay = 0.001f;
    ctx->noise_gate = 0.02f; // Below this, don't amplify
}

void eif_agc_process(eif_agc_t* ctx, const float32_t* input, float32_t* output, size_t length) {
    if (!ctx || !input || !output) return;
    
    float32_t target_energy = ctx->target_level;
    float32_t gain = ctx->current_gain;
    float32_t max_g = ctx->max_gain;
    float32_t noise_floor = ctx->noise_gate;
    
    for (size_t i = 0; i < length; i++) {
        float32_t in_val = input[i];
        float32_t abs_in = fabsf(in_val);
        
        // Apply gain
        float32_t out_val = in_val * gain;
        
        // Hard limiter to prevent clipping
        if (out_val > 1.0f) out_val = 1.0f;
        if (out_val < -1.0f) out_val = -1.0f;
        
        output[i] = out_val;
        
        // Update gain logic
        if (abs_in > noise_floor) {
            float32_t error = target_energy - fabsf(out_val); // Simple P-controller logic
            
            if (error < 0) {
                // Too loud, reduce gain fast (Attack)
                gain += error * ctx->attack;
            } else {
                // Too quiet, increase gain slow (Decay/Release)
                gain += error * ctx->decay;
            }
        }
        
        // Clamp gain
        if (gain < 1.0f) gain = 1.0f; 
        if (gain > max_g) gain = max_g;
    }
    
    ctx->current_gain = gain;
}
