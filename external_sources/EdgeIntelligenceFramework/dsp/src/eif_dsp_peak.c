#include "eif_dsp_peak.h"
#include <math.h>

void eif_robust_peak_init(eif_robust_peak_t* ctx, float32_t fs, float32_t min_peak_dist_ms) {
    ctx->threshold = 0.5f; // Default start
    ctx->moving_avg = 0.0f;
    ctx->refractory_period = (int)((min_peak_dist_ms / 1000.0f) * fs);
    ctx->refractory_counter = 0;
    ctx->alpha = 0.01f; // Slow adaptation
}

bool eif_robust_peak_update(eif_robust_peak_t* ctx, float32_t sample) {
    bool detected = false;
    float32_t abs_val = fabsf(sample);
    
    // Decrement refractory counter
    if (ctx->refractory_counter > 0) {
        ctx->refractory_counter--;
    }
    
    // Update threshold (adaptive)
    // Moving average of signal energy helps define noise floor
    ctx->moving_avg = (1.0f - ctx->alpha) * ctx->moving_avg + ctx->alpha * abs_val;
    
    // Dynamic threshold: e.g. 1.5x moving average + static absolute
    float32_t current_thresh = (1.5f * ctx->moving_avg);
    if (current_thresh < 0.1f) current_thresh = 0.1f; // Floor
    
    if (abs_val > current_thresh && ctx->refractory_counter == 0) {
        detected = true;
        ctx->refractory_counter = ctx->refractory_period;
    }
    
    return detected;
}

int eif_robust_peak_process_buffer(eif_robust_peak_t* ctx, const float32_t* input, size_t length, int* output_indices, int max_peaks) {
    int count = 0;
    
    // Basic local maxima check can be added here combined with robust update
    for (size_t i = 1; i < length - 1; i++) {
        // First check: is it a local max?
        if (input[i] > input[i-1] && input[i] > input[i+1]) {
            // Then check robust criteria
            if (eif_robust_peak_update(ctx, input[i])) {
                if (count < max_peaks) {
                    output_indices[count++] = (int)i;
                }
            }
        } else {
            // Still update background stats
             // Use smaller alpha for non-peaks?
        }
    }
    return count;
}
