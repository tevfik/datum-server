#include "eif_dsp_beamformer.h"
#include <stddef.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Speed of sound in air (m/s) at 20C
#define SPEED_OF_SOUND 343.0f

void eif_beamformer_init(eif_beamformer_t* ctx, float32_t mic_dist, uint32_t sample_rate, float32_t target_angle_deg) {
    if (!ctx) return;
    ctx->mic_dist_m = mic_dist;
    ctx->sample_rate = sample_rate;
    eif_beamformer_steer(ctx, target_angle_deg);
}

void eif_beamformer_steer(eif_beamformer_t* ctx, float32_t target_angle_deg) {
    if (!ctx) return;
    
    // TDOA = (d * sin(theta)) / c
    // Samples = TDOA * Fs
    float32_t angle_rad = target_angle_deg * (M_PI / 180.0f);
    float32_t tdoa = (ctx->mic_dist_m * sinf(angle_rad)) / SPEED_OF_SOUND;
    ctx->delay_samples = (int)(tdoa * ctx->sample_rate);
}

void eif_beamformer_process_stereo(eif_beamformer_t* ctx, const float32_t* input, float32_t* output, size_t num_frames) {
    if (!ctx || !input || !output) return;
    
    // Simple integer delay-and-sum. 
    // Fractional delay would be better (sinc interpolation) but costly.
    
    // If delay > 0: signal arrives at Mic1 *after* Mic2 (Mic2 is closer to source)
    // We need to delay Mic2 to match Mic1.
    // Stereo interleaved: L=Mic1, R=Mic2
    
    for (size_t i = 0; i < num_frames; i++) {
        float32_t mic1 = input[2*i];
        float32_t mic2 = input[2*i + 1];
        
        output[i] = 0.5f * (mic1 + mic2); // Placeholder for now without circular buffer
    }
}

void eif_beamformer_process_dual(eif_beamformer_t* ctx, const float32_t* mic1, const float32_t* mic2, 
                                 float32_t* output, size_t length) {
    int delay = ctx->delay_samples;
    
    // Simple bounded delay sum
    for (size_t i = 0; i < length; i++) {
        float32_t val1 = mic1[i];
        
        // Without ring buffer, we can only safely sum aligned
        if (delay == 0) {
            output[i] = 0.5f * (val1 + mic2[i]);
        } else {
             // Basic shift if valid
             int idx2 = (int)i - delay;
             if (idx2 >= 0 && idx2 < (int)length) {
                 output[i] = 0.5f * (val1 + mic2[idx2]);
             } else {
                 output[i] = 0.5f * val1; 
             }
        }
    }
}
