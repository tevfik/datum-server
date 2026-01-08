#include "eif_dsp_fft.h"
#include "eif_dsp_transform.h"
#include "eif_memory.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ===================================
// FFT Implementation (Restored)
// ===================================

static uint16_t reverse_bits(uint16_t x, uint16_t bits) {
    uint16_t result = 0;
    for (uint16_t i = 0; i < bits; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

eif_status_t eif_dsp_fft_init_f32(eif_fft_config_t* config, uint16_t length, eif_memory_pool_t* pool) {
    if (!config || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    if ((length & (length - 1)) != 0) return EIF_STATUS_INVALID_ARGUMENT;

    config->length = length;
    config->inverse = false;

    // Use memory pool for allocations
    config->twiddle_factors = (float32_t*)eif_memory_alloc(pool, sizeof(float32_t) * length, 4); 
    config->bit_reverse_indices = (uint16_t*)eif_memory_alloc(pool, sizeof(uint16_t) * length, 2);

    if (!config->twiddle_factors || !config->bit_reverse_indices) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }

    // Precompute Twiddles
    for (int i = 0; i < length / 2; i++) {
        float32_t angle = -2.0f * M_PI * i / length;
        config->twiddle_factors[2*i] = cosf(angle);
        config->twiddle_factors[2*i+1] = sinf(angle);
    }

    // Precompute Bit Reversal
    int bits = 0;
    while ((1 << bits) < length) bits++;
    
    for (int i = 0; i < length; i++) {
        config->bit_reverse_indices[i] = reverse_bits(i, bits);
    }
    
    config->rfft_scratch = (float32_t*)eif_memory_alloc(pool, sizeof(float32_t) * length * 2, 4);

    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_fft_deinit_f32(eif_fft_config_t* config) {
    if (!config) return EIF_STATUS_OK;
    // Memory pool manages deallocation, just clear pointers
    config->twiddle_factors = NULL;
    config->bit_reverse_indices = NULL;
    config->rfft_scratch = NULL;
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_fft_f32(const eif_fft_config_t* config, float32_t* data) {
    if (!config || !data) return EIF_STATUS_INVALID_ARGUMENT;
    
    uint16_t n = config->length;
    
    // Bit Reversal
    for (uint16_t i = 0; i < n; i++) {
        uint16_t j = config->bit_reverse_indices[i];
        if (j > i) {
            float32_t tr = data[2*i];
            float32_t ti = data[2*i+1];
            data[2*i] = data[2*j];
            data[2*i+1] = data[2*j+1];
            data[2*j] = tr;
            data[2*j+1] = ti;
        }
    }
    
    // Butterfly
    for (uint16_t len = 2; len <= n; len <<= 1) {
        float32_t ang = -2.0f * M_PI / len;
        if (config->inverse) ang = -ang; // Inverse uses positive angle
        
        float32_t wlen_r = cosf(ang);
        float32_t wlen_i = sinf(ang);
        
        for (uint16_t i = 0; i < n; i += len) {
            float32_t w_r = 1.0f;
            float32_t w_i = 0.0f;
            
            for (uint16_t j = 0; j < len / 2; j++) {
                float32_t u_r = data[2*(i+j)];
                float32_t u_i = data[2*(i+j)+1];
                
                float32_t v_r = data[2*(i+j+len/2)];
                float32_t v_i = data[2*(i+j+len/2)+1];
                
                float32_t tv_r = v_r * w_r - v_i * w_i;
                float32_t tv_i = v_r * w_i + v_i * w_r;
                
                data[2*(i+j)] = u_r + tv_r;
                data[2*(i+j)+1] = u_i + tv_i;
                
                data[2*(i+j+len/2)] = u_r - tv_r;
                data[2*(i+j+len/2)+1] = u_i - tv_i;
                
                float32_t tmp_r = w_r * wlen_r - w_i * wlen_i;
                w_i = w_r * wlen_i + w_i * wlen_r;
                w_r = tmp_r;
            }
        }
    }

    // Scaling for Inverse FFT
    if (config->inverse) {
        float32_t scale = 1.0f / n;
        for (uint16_t i = 0; i < n; i++) {
            data[2*i] *= scale;
            data[2*i+1] *= scale;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_rfft_f32(const eif_fft_config_t* config, const float32_t* input_real, float32_t* output_complex) {
    if (!config || !input_real || !output_complex) return EIF_STATUS_INVALID_ARGUMENT;
    if (!config->rfft_scratch) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Copy to complex buffer
    for (int i = 0; i < config->length; i++) {
        output_complex[2*i] = input_real[i];
        output_complex[2*i+1] = 0.0f;
    }
    
    return eif_dsp_fft_f32(config, output_complex);
}

eif_status_t eif_dsp_magnitude_f32(const float32_t* input_complex, float32_t* output_mag, size_t length) {
    for (size_t i=0; i<length; i++) {
        float32_t re = input_complex[2*i];
        float32_t im = input_complex[2*i+1];
        output_mag[i] = sqrtf(re*re + im*im);
    }
    return EIF_STATUS_OK;
}

// ===================================
// STFT Implementation (eif_dsp_fft.h variant)
// ===================================

eif_status_t eif_dsp_stft_init_f32(eif_stft_config_t* config, uint16_t fft_length, uint16_t hop_length, uint16_t window_length, eif_memory_pool_t* pool) {
    if (!config || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    if (window_length > fft_length) return EIF_STATUS_INVALID_ARGUMENT;

    config->fft_length = fft_length;
    config->hop_length = hop_length;
    config->window_length = window_length;
    
    eif_status_t status = eif_dsp_fft_init_f32(&config->fft_config, fft_length, pool);
    if (status != EIF_STATUS_OK) return status;

    // Use memory pool
    config->window = (float32_t*)eif_memory_alloc(pool, sizeof(float32_t) * window_length, 4);
    config->fft_buffer = (float32_t*)eif_memory_alloc(pool, sizeof(float32_t) * fft_length * 2, 4);
    
    if (!config->window || !config->fft_buffer) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }

    // Hamming window default
    for(int i=0; i<window_length; i++) {
        config->window[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (window_length - 1));
    }

    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_stft_deinit_f32(eif_stft_config_t* config) {
    if (!config) return EIF_STATUS_OK;
    eif_dsp_fft_deinit_f32(&config->fft_config);
    // Pointers managed by pool
    config->window = NULL;
    config->fft_buffer = NULL;
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_stft_compute_f32(const eif_stft_config_t* config, const float32_t* input, float32_t* output_magnitude, size_t num_frames) {
    if (!config || !input || !output_magnitude) return EIF_STATUS_INVALID_ARGUMENT;
    if (!config->window || !config->fft_buffer) return EIF_STATUS_INVALID_ARGUMENT;

    size_t in_idx = 0;
    size_t out_idx = 0;
    size_t fft_len = config->fft_length;
    size_t win_len = config->window_length;
    size_t hop = config->hop_length;
    size_t out_frame_size = fft_len / 2 + 1;
    float32_t* work_buffer = ((eif_stft_config_t*)config)->fft_buffer;

    for (size_t f = 0; f < num_frames; f++) {
        // Windowing
        for(size_t i=0; i<win_len; i++) {
            work_buffer[2*i] = input[in_idx + i] * config->window[i];
            work_buffer[2*i+1] = 0.0f;
        }
        // Zero padding
        for(size_t i=win_len; i<fft_len; i++) {
            work_buffer[2*i] = 0.0f;
            work_buffer[2*i+1] = 0.0f;
        }

        // FFT
        eif_dsp_fft_f32(&config->fft_config, work_buffer);

        // Magnitude
        eif_dsp_magnitude_f32(work_buffer, &output_magnitude[out_idx], out_frame_size);

        in_idx += hop;
        out_idx += out_frame_size;
    }

    return EIF_STATUS_OK;
}

// ===================================
// New Advanced DSP Features (eif_dsp_transform.h)
// ===================================

eif_status_t eif_stft_init(eif_stft_t* ctx, int fft_size, int hop_size, int window_size) {
    if (!ctx) return EIF_STATUS_INVALID_ARGUMENT;
    ctx->fft_size = fft_size;
    ctx->hop_size = hop_size;
    ctx->window_size = window_size;
    return EIF_STATUS_OK;
}

void eif_goertzel_init(eif_goertzel_t* ctx, float32_t target_freq, float32_t sample_rate) {
    if (!ctx) return;
    float32_t normalized_freq = target_freq / sample_rate;
    ctx->coeff = 2.0f * cosf(2.0f * M_PI * normalized_freq);
    ctx->q1 = 0.0f;
    ctx->q2 = 0.0f;
}

void eif_goertzel_process_sample(eif_goertzel_t* ctx, float32_t sample) {
    float32_t q0 = ctx->coeff * ctx->q1 - ctx->q2 + sample;
    ctx->q2 = ctx->q1;
    ctx->q1 = q0;
}

float32_t eif_goertzel_compute_magnitude(eif_goertzel_t* ctx) {
    float32_t mag_sq = (ctx->q1 * ctx->q1) + (ctx->q2 * ctx->q2) - (ctx->coeff * ctx->q1 * ctx->q2);
    if (mag_sq < 0) mag_sq = 0;
    return sqrtf(mag_sq);
}

void eif_goertzel_reset(eif_goertzel_t* ctx) {
    ctx->q1 = 0.0f;
    ctx->q2 = 0.0f;
}

void eif_dwt_haar_forward(const float32_t* input, float32_t* low_pass, float32_t* high_pass, size_t length) {
    if (length % 2 != 0) return;
    size_t half = length / 2;
    float32_t inv_sqrt2 = 0.70710678f;
    for (size_t i = 0; i < half; i++) {
        float32_t a = input[2*i];
        float32_t b = input[2*i + 1];
        low_pass[i] = (a + b) * inv_sqrt2;
        high_pass[i] = (a - b) * inv_sqrt2;
    }
}

void eif_dwt_haar_inverse(const float32_t* low_pass, const float32_t* high_pass, float32_t* output, size_t length) {
    if (length % 2 != 0) return;
    size_t half = length / 2;
    float32_t inv_sqrt2 = 0.70710678f;
    for (size_t i = 0; i < half; i++) {
        float32_t avg = low_pass[i];
        float32_t diff = high_pass[i];
        output[2*i] = (avg + diff) * inv_sqrt2;
        output[2*i + 1] = (avg - diff) * inv_sqrt2;
    }
}
