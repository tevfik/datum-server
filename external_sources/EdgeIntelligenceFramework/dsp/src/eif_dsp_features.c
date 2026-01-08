#include "eif_dsp.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: Mel to Hz and back
static inline float32_t mel2hz(float32_t mel) { return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f); }
static inline float32_t hz2mel(float32_t hz) { return 2595.0f * log10f(1.0f + hz / 700.0f); }

eif_status_t eif_dsp_mfcc_init_f32(eif_mfcc_config_t* config, eif_memory_pool_t* pool) {
    if (!config || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Allocate Filter Bank (Unused in current compute, but good for future)
    int filter_size = (config->fft_length / 2 + 1) * config->num_filters;
    config->filter_bank = (float32_t*)eif_memory_alloc(pool, filter_size * sizeof(float32_t), 4);
    
    // Allocate Mel Energies Scratch Buffer
    config->mel_energies = (float32_t*)eif_memory_alloc(pool, config->num_filters * sizeof(float32_t), 4);
    
    if (!config->filter_bank || !config->mel_energies) return EIF_STATUS_OUT_OF_MEMORY;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_mfcc_compute_f32(const eif_mfcc_config_t* config, const float32_t* fft_mag, float32_t* mfcc_out, eif_memory_pool_t* pool) {
    (void)pool; // Unused
    if (!config || !fft_mag || !mfcc_out || !config->mel_energies) return EIF_STATUS_INVALID_ARGUMENT;
    
    float32_t min_mel = hz2mel(config->low_freq);
    float32_t max_mel = hz2mel(config->high_freq);
    float32_t mel_step = (max_mel - min_mel) / (config->num_filters + 1);
    
    // Use pre-allocated buffer
    float32_t* mel_energies = config->mel_energies;
    
    for (int m = 0; m < config->num_filters; m++) {
        mel_energies[m] = 0.0f;
        
        float32_t center_mel = min_mel + (m + 1) * mel_step;
        float32_t left_mel = center_mel - mel_step;
        float32_t right_mel = center_mel + mel_step;
        
        int fft_len_by_2 = config->fft_length / 2;
        for (int k = 0; k <= fft_len_by_2; k++) {
            float32_t freq = k * (float32_t)config->sample_rate / config->fft_length;
            float32_t mel = hz2mel(freq);
            
            float32_t weight = 0.0f;
            if (mel > left_mel && mel < center_mel) {
                weight = (mel - left_mel) / (center_mel - left_mel);
            } else if (mel >= center_mel && mel < right_mel) {
                weight = (right_mel - mel) / (right_mel - center_mel);
            }
            
            if (weight > 0.0f) {
                mel_energies[m] += weight * fft_mag[k]; 
            }
        }
    }
    
    // 2. Logarithm
    for (int m = 0; m < config->num_filters; m++) {
        if (mel_energies[m] < 1e-6f) mel_energies[m] = 1e-6f; // Avoid log(0)
        mel_energies[m] = logf(mel_energies[m]);
    }
    
    // 3. DCT-II
    for (int k = 0; k < config->num_mfcc; k++) {
        float32_t sum = 0.0f;
        for (int n = 0; n < config->num_filters; n++) {
            sum += mel_energies[n] * cosf(M_PI * k * (n + 0.5f) / config->num_filters);
        }
        mfcc_out[k] = sum;
    }
    
    return EIF_STATUS_OK;
}

float32_t eif_dsp_rms_f32(const float32_t* input, size_t length) {
    float32_t sum_sq = 0.0f;
    for (size_t i = 0; i < length; i++) {
        sum_sq += input[i] * input[i];
    }
    return sqrtf(sum_sq / length);
}

float32_t eif_dsp_zcr_f32(const float32_t* input, size_t length) {
    float32_t count = 0.0f;
    for (size_t i = 1; i < length; i++) {
        if ((input[i-1] > 0 && input[i] < 0) || (input[i-1] < 0 && input[i] > 0)) {
            count += 1.0f;
        }
    }
    return count / (length - 1);
}

bool eif_dsp_vad_energy_f32(const float32_t* input, size_t length, float32_t threshold) {
    if (!input || length == 0) return false;
    float32_t rms = eif_dsp_rms_f32(input, length);
    return rms > threshold;
}

bool eif_dsp_vad_zcr_f32(const float32_t* input, size_t length, float32_t threshold) {
    if (!input || length == 0) return false;
    float32_t zcr = eif_dsp_zcr_f32(input, length);
    return zcr > threshold;
}

float32_t eif_dsp_spectral_centroid_f32(const float32_t* input_mag, int fft_size, float32_t sample_rate) {
    if (!input_mag || fft_size <= 0) return 0.0f;
    
    int num_bins = fft_size / 2 + 1;
    float32_t sum_mag = 0.0f;
    float32_t sum_freq_mag = 0.0f;
    float32_t bin_width = sample_rate / fft_size;
    
    for (int i = 0; i < num_bins; i++) {
        float32_t freq = i * bin_width;
        sum_mag += input_mag[i];
        sum_freq_mag += freq * input_mag[i];
    }
    
    if (sum_mag < 1e-9f) return 0.0f;
    return sum_freq_mag / sum_mag;
}

float32_t eif_dsp_spectral_rolloff_f32(const float32_t* input_mag, int fft_size, float32_t sample_rate, float32_t threshold_percent) {
    if (!input_mag || fft_size <= 0) return 0.0f;
    
    int num_bins = fft_size / 2 + 1;
    float32_t total_energy = 0.0f;
    
    for (int i = 0; i < num_bins; i++) {
        total_energy += input_mag[i];
    }
    
    float32_t threshold_energy = total_energy * threshold_percent;
    float32_t cum_energy = 0.0f;
    float32_t bin_width = sample_rate / fft_size;
    
    for (int i = 0; i < num_bins; i++) {
        cum_energy += input_mag[i];
        if (cum_energy >= threshold_energy) {
            return i * bin_width;
        }
    }
    
    return (num_bins - 1) * bin_width; // Should not happen if threshold <= 1.0
}

float32_t eif_dsp_spectral_flux_f32(const float32_t* input_mag, const float32_t* prev_mag, int fft_size) {
    if (!input_mag || !prev_mag || fft_size <= 0) return 0.0f;
    
    int num_bins = fft_size / 2 + 1;
    float32_t flux = 0.0f;
    
    for (int i = 0; i < num_bins; i++) {
        float32_t diff = input_mag[i] - prev_mag[i];
        // Rectified flux: only positive changes (onset detection)
        // Or Euclidean distance? Usually rectified for onsets.
        // Let's use simple Euclidean distance for general flux.
        // Actually, standard spectral flux is usually sum((mag[i] - prev[i])^2) or sum(|mag[i] - prev[i]|)
        // Let's use L2 norm (Euclidean)
        flux += diff * diff;
    }
    
    return sqrtf(flux);
}

int eif_dsp_peak_detection_f32(const float32_t* input, int length, float32_t threshold, int min_distance, int* output_indices, int max_peaks) {
    if (!input || !output_indices || length <= 0 || max_peaks <= 0) return 0;
    
    int count = 0;
    int last_peak_idx = -min_distance - 1; // Ensure first peak can be at 0
    
    for (int i = 1; i < length - 1; i++) {
        if (input[i] > threshold && input[i] > input[i-1] && input[i] > input[i+1]) {
            // Found a local maximum above threshold
            if (i - last_peak_idx >= min_distance) {
                output_indices[count++] = i;
                last_peak_idx = i;
                if (count >= max_peaks) break;
            } else {
                // Conflict with previous peak. Keep the larger one?
                // Simple greedy approach: keep the first one found (standard for real-time).
                // Or: if current is larger than previous, replace previous.
                // Let's implement replacement if current is significantly larger?
                // For simplicity and standard behavior: ignore current if too close to previous.
            }
        }
    }
    
    return count;
}

void eif_dsp_envelope_f32(const float32_t* input, float32_t* output, int length, float32_t decay_factor) {
    if (!input || !output || length <= 0) return;
    
    // Simple Rectification + One-pole Low Pass Filter
    // y[n] = alpha * |x[n]| + (1 - alpha) * y[n-1]
    // Let's use decay_factor as (1 - alpha). So alpha = 1 - decay_factor.
    // If decay_factor is 0.9, y[n] = 0.1 * |x| + 0.9 * y[n-1]
    
    float32_t alpha = 1.0f - decay_factor;
    float32_t prev_y = 0.0f;
    
    for (int i = 0; i < length; i++) {
        float32_t abs_x = fabsf(input[i]);
        float32_t y = alpha * abs_x + decay_factor * prev_y;
        output[i] = y;
        prev_y = y;
    }
}
