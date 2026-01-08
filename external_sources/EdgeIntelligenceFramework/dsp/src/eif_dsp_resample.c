#include "eif_dsp_resample.h"
#include <math.h>

eif_status_t eif_resample_init(eif_resample_config_t* cfg, uint32_t in_rate, uint32_t out_rate) {
    if (!cfg || in_rate == 0 || out_rate == 0) return EIF_STATUS_INVALID_ARGUMENT;
    cfg->in_rate = in_rate;
    cfg->out_rate = out_rate;
    cfg->ratio = (float32_t)in_rate / (float32_t)out_rate;
    return EIF_STATUS_OK;
}

eif_status_t eif_resample_process_linear(eif_resample_config_t* cfg, const float32_t* input, size_t in_len, 
                                         float32_t* output, size_t* out_len, size_t max_out_len) {
    if (!cfg || !input || !output || !out_len) return EIF_STATUS_INVALID_ARGUMENT;
    
    float32_t ratio = cfg->ratio;
    size_t expected_out = (size_t)(in_len / ratio);
    
    if (expected_out > max_out_len) expected_out = max_out_len;
    
    // Optimization: Pre-calculate loop variables to avoid repeated casting/checks
    for (size_t i = 0; i < expected_out; i++) {
        float32_t src_idx_f = i * ratio;
        size_t idx0 = (size_t)src_idx_f;
        size_t idx1 = idx0 + 1;
        float32_t frac = src_idx_f - idx0;
        
        if (idx1 < in_len) {
            // Linear Interpolation: y = y0 + (y1 - y0) * frac
            // Optimized: y0 * (1-frac) + y1 * frac
            float32_t val0 = input[idx0];
            float32_t val1 = input[idx1];
            output[i] = val0 + frac * (val1 - val0);
        } else {
            // End of buffer handling
            output[i] = input[in_len - 1]; 
        }
    }
    
    *out_len = expected_out;
    return EIF_STATUS_OK;
}

eif_status_t eif_resample_decimate(const float32_t* input, size_t in_len, int factor, 
                                   float32_t* output, size_t* out_len) {
    if (!input || !output || !out_len || factor < 1) return EIF_STATUS_INVALID_ARGUMENT;
    
    size_t out_count = in_len / factor;
    *out_len = out_count;
    
    // Simple averaging (Boxcar filter) to prevent aliasing before downsampling
    float32_t scale = 1.0f / factor;
    
    #pragma omp parallel for
    for (size_t i = 0; i < out_count; i++) {
        float32_t sum = 0.0f;
        size_t start = i * factor;
        for (int j = 0; j < factor; j++) {
            sum += input[start + j];
        }
        output[i] = sum * scale;
    }
    
    return EIF_STATUS_OK;
}

// ==========================================
// Legacy/Core Resampling Implementation (eif_dsp_filter.h)
// ==========================================

eif_status_t eif_dsp_resample_linear_f32(const float32_t* input, size_t input_len, float32_t* output, size_t output_len) {
    if (!input || !output || output_len == 0 || input_len == 0) return EIF_STATUS_INVALID_ARGUMENT;

    if (output_len == 1) {
        output[0] = input[0];
        return EIF_STATUS_OK;
    }

    float32_t step = (float32_t)(input_len - 1) / (float32_t)(output_len - 1);

    for (size_t i = 0; i < output_len; i++) {
        float32_t exact_pos = i * step;
        size_t idx = (size_t)exact_pos;
        float32_t frac = exact_pos - idx;

        if (idx >= input_len - 1) {
            output[i] = input[input_len - 1];
        } else {
            // Linear Interp
            output[i] = input[idx] * (1.0f - frac) + input[idx + 1] * frac;
        }
    }
    return EIF_STATUS_OK;
}

static float32_t cubic_hermite(float32_t y0, float32_t y1, float32_t y2, float32_t y3, float32_t mu) {
    float32_t mu2 = mu * mu;
    float32_t a0 = -0.5f*y0 + 1.5f*y1 - 1.5f*y2 + 0.5f*y3;
    float32_t a1 = y0 - 2.5f*y1 + 2.0f*y2 - 0.5f*y3;
    float32_t a2 = -0.5f*y0 + 0.5f*y2;
    float32_t a3 = y1;
    return a0*mu*mu2 + a1*mu2 + a2*mu + a3;
}

eif_status_t eif_dsp_resample_cubic_f32(const float32_t* input, size_t input_len, float32_t* output, size_t output_len) {
    if (!input || !output || output_len == 0 || input_len == 0) return EIF_STATUS_INVALID_ARGUMENT;

    if (output_len == 1) {
        output[0] = input[0];
        return EIF_STATUS_OK;
    }

    float32_t step = (float32_t)(input_len - 1) / (float32_t)(output_len - 1);

    for (size_t i = 0; i < output_len; i++) {
        float32_t exact_pos = i * step;
        size_t idx = (size_t)exact_pos;
        float32_t frac = exact_pos - idx;

        // Boundary checks
        float32_t y0 = (idx > 0) ? input[idx-1] : input[0];
        float32_t y1 = input[idx];
        float32_t y2 = (idx < input_len - 1) ? input[idx+1] : input[input_len-1];
        float32_t y3 = (idx < input_len - 2) ? input[idx+2] : input[input_len-1];

        output[i] = cubic_hermite(y0, y1, y2, y3, frac);
    }
    return EIF_STATUS_OK;
}
