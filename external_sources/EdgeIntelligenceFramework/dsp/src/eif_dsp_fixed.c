#include "eif_dsp_fixed.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: Bit Reversal
static uint32_t reverse_bits(uint32_t x, int bits) {
    uint32_t y = 0;
    for (int i = 0; i < bits; i++) {
        y = (y << 1) | (x & 1);
        x >>= 1;
    }
    return y;
}

eif_status_t eif_dsp_fft_init_q15(eif_fft_config_q15_t* config, uint16_t length, eif_memory_pool_t* pool) {
    if (!config || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    config->length = length;
    config->inverse = false;
    
    config->bit_reverse_indices = (uint16_t*)eif_memory_alloc(pool, length * sizeof(uint16_t), sizeof(uint16_t));
    config->twiddle_factors = (q15_t*)eif_memory_alloc(pool, length * sizeof(q15_t), sizeof(q15_t));
    
    if (!config->bit_reverse_indices || !config->twiddle_factors) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Compute Bit Reverse Indices
    int levels = 0;
    while ((1 << levels) < length) levels++;
    for (int i = 0; i < length; i++) {
        config->bit_reverse_indices[i] = reverse_bits(i, levels);
    }
    
    // Compute Twiddle Factors
    for (int k = 0; k < length / 2; k++) {
        float angle = -2 * M_PI * k / length;
        float c = cosf(angle);
        float s = sinf(angle);
        
        // Saturate to Q15 range
        if (c >= 1.0f) config->twiddle_factors[2*k] = EIF_Q15_MAX;
        else if (c <= -1.0f) config->twiddle_factors[2*k] = EIF_Q15_MIN;
        else config->twiddle_factors[2*k] = EIF_FLOAT_TO_Q15(c);
        
        if (s >= 1.0f) config->twiddle_factors[2*k+1] = EIF_Q15_MAX;
        else if (s <= -1.0f) config->twiddle_factors[2*k+1] = EIF_Q15_MIN;
        else config->twiddle_factors[2*k+1] = EIF_FLOAT_TO_Q15(s);
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_fft_deinit_q15(eif_fft_config_q15_t* config) {
    // Memory is managed by pool, no explicit deallocation needed
    (void)config;
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_fft_q15(const eif_fft_config_q15_t* config, q15_t* data) {
    if (!config || !data) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = config->length;
    
    // Bit Reversal
    for (int i = 0; i < n; i++) {
        int rev = config->bit_reverse_indices[i];
        if (rev > i) {
            q15_t temp_r = data[2*i];
            q15_t temp_i = data[2*i+1];
            data[2*i] = data[2*rev];
            data[2*i+1] = data[2*rev+1];
            data[2*rev] = temp_r;
            data[2*rev+1] = temp_i;
        }
    }
    
    // Butterfly Operations
    for (int size = 2; size <= n; size *= 2) {
        int half_size = size / 2;
        int table_step = n / size;
        
        for (int i = 0; i < n; i += size) {
            for (int j = 0; j < half_size; j++) {
                int k = j * table_step;
                q15_t w_r = config->twiddle_factors[2*k];
                q15_t w_i = config->twiddle_factors[2*k+1];
                
                if (config->inverse) w_i = -w_i;
                
                int even_idx = 2 * (i + j);
                int odd_idx = 2 * (i + j + half_size);
                
                q15_t odd_r = data[odd_idx];
                q15_t odd_i = data[odd_idx+1];
                
                // Complex Mul: (w_r + j*w_i) * (odd_r + j*odd_i)
                // = (w_r*odd_r - w_i*odd_i) + j*(w_r*odd_i + w_i*odd_r)
                // Use Q15 mul (shifts right by 15)
                q15_t twiddled_r = eif_q15_sub(eif_q15_mul(w_r, odd_r), eif_q15_mul(w_i, odd_i));
                q15_t twiddled_i = eif_q15_add(eif_q15_mul(w_r, odd_i), eif_q15_mul(w_i, odd_r));
                
                q15_t even_r = data[even_idx];
                q15_t even_i = data[even_idx+1];
                
                // Butterfly with Scaling (>>1) to prevent overflow
                data[even_idx] = (even_r >> 1) + (twiddled_r >> 1);
                data[even_idx+1] = (even_i >> 1) + (twiddled_i >> 1);
                data[odd_idx] = (even_r >> 1) - (twiddled_r >> 1);
                data[odd_idx+1] = (even_i >> 1) - (twiddled_i >> 1);
            }
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_rfft_q15(const eif_fft_config_q15_t* config, const q15_t* input_real, q15_t* output_complex, eif_memory_pool_t* pool) {
    if (!config || !input_real || !output_complex || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int n = config->length;
    // n is N/2
    
    // Copy input to output buffer (as complex)
    memcpy(output_complex, input_real, 2 * n * sizeof(q15_t));
    
    // Perform N/2 FFT
    eif_dsp_fft_q15(config, output_complex);
    
    // Split / Unpack
    q15_t* Z = (q15_t*)eif_memory_alloc(pool, 2 * n * sizeof(q15_t), sizeof(q15_t));
    if (!Z) return EIF_STATUS_OUT_OF_MEMORY;
    memcpy(Z, output_complex, 2 * n * sizeof(q15_t));
    
    output_complex[0] = eif_q15_add(Z[0], Z[1]); // Real DC
    output_complex[1] = 0;
    
    output_complex[2*n] = eif_q15_sub(Z[0], Z[1]); // Real Nyquist
    output_complex[2*n+1] = 0;
    
    for (int k = 1; k < n; k++) {
        q15_t z_k_r = Z[2*k];
        q15_t z_k_i = Z[2*k+1];
        
        q15_t z_nk_r = Z[2*(n - k)];
        q15_t z_nk_i = Z[2*(n - k)+1];
        
        q15_t v_r = (z_k_r + z_nk_r) >> 1;
        q15_t v_i = (z_k_i - z_nk_i) >> 1;
        
        q15_t w_r = (z_k_i + z_nk_i) >> 1;
        q15_t w_i = (z_nk_r - z_k_r) >> 1;
        
        float angle = -M_PI * k / n;
        q15_t tw_r = EIF_FLOAT_TO_Q15(cosf(angle));
        q15_t tw_i = EIF_FLOAT_TO_Q15(sinf(angle));
        
        q15_t term_r = eif_q15_sub(eif_q15_mul(tw_r, w_r), eif_q15_mul(tw_i, w_i));
        q15_t term_i = eif_q15_add(eif_q15_mul(tw_r, w_i), eif_q15_mul(tw_i, w_r));
        
        output_complex[2*k] = eif_q15_add(v_r, term_r);
        output_complex[2*k+1] = eif_q15_add(v_i, term_i);
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_magnitude_q15(const q15_t* input_complex, q15_t* output_mag, size_t length) {
    for (size_t i = 0; i < length; i++) {
        q15_t r = input_complex[2*i];
        q15_t im = input_complex[2*i+1];
        // mag = sqrt(r^2 + im^2)
        // r^2 can be Q30.
        // We need sqrt(Q30) -> Q15.
        // eif_q15_mul returns Q15 (shifted).
        // r*r in Q15 domain = (r*r)>>15.
        q15_t sq_r = eif_q15_mul(r, r);
        q15_t sq_im = eif_q15_mul(im, im);
        q15_t sum = eif_q15_add(sq_r, sq_im);
        output_mag[i] = eif_q15_sqrt(sum);
    }
    return EIF_STATUS_OK;
}

// --- Windowing ---
eif_status_t eif_dsp_window_hamming_q15(q15_t* window, size_t length) {
    if (!window || length == 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Hamming: w[n] = 0.54 - 0.46 * cos(2*pi*n / (N-1))
    // Q15: 0.54 -> 17694, 0.46 -> 15073
    
    for (size_t i = 0; i < length; i++) {
        uint32_t angle_norm = (i * 65535) / (length - 1);
        q15_t cos_val = eif_q15_cos((q15_t)angle_norm);
        
        // 0.54 - 0.46 * cos
        int32_t val = 17694 - ((15073 * cos_val) >> 15);
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        window[i] = (q15_t)val;
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_window_hanning_q15(q15_t* window, size_t length) {
    if (!window || length == 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Hanning: w[n] = 0.5 * (1 - cos(2*pi*n / (N-1)))
    // Q15: 0.5 -> 16384
    
    for (size_t i = 0; i < length; i++) {
        uint32_t angle_norm = (i * 65535) / (length - 1);
        q15_t cos_val = eif_q15_cos((q15_t)angle_norm);
        
        // 0.5 * (1 - cos)
        int32_t val = 16384 - ((16384 * cos_val) >> 15);
        if (val > 32767) val = 32767;
        window[i] = (q15_t)val;
    }
    return EIF_STATUS_OK;
}



// --- Filters ---
eif_status_t eif_dsp_fir_q15(const q15_t* input, q15_t* output, size_t length, const q15_t* coeffs, size_t taps) {
    if (!input || !output || !coeffs) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (size_t i = 0; i < length; i++) {
        q63_t sum = 0; // 64-bit accumulator to prevent overflow during sum of products
        for (size_t j = 0; j < taps; j++) {
            if (i >= j) {
                // input * coeff -> Q15 * Q15 = Q30
                sum += (q31_t)input[i - j] * coeffs[j];
            }
        }
        // Result is Q30. Convert to Q15.
        // Rounding: + (1<<14) before shift?
        // sum >> 15
        q31_t res = (q31_t)(sum >> 15);
        
        // Saturation
        if (res > 32767) res = 32767;
        else if (res < -32768) res = -32768;
        
        output[i] = (q15_t)res;
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_iir_q15(const q15_t* input, q15_t* output, size_t length, 
                             const q15_t* coeffs, q15_t* state, size_t stages) {
    if (!input || !output || !coeffs || !state) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Direct Form II Transposed or Direct Form I?
    // Float impl used Direct Form II Transposed.
    // Coeffs: [b0, b1, b2, a1, a2]
    // State: [d1, d2]
    
    for (size_t n = 0; n < length; n++) {
        q15_t x = input[n];
        
        // Process each stage
        for (size_t s = 0; s < stages; s++) {
            const q15_t* c = &coeffs[s * 5];
            q15_t* d = &state[s * 2];
            
            q15_t b0 = c[0];
            q15_t b1 = c[1];
            q15_t b2 = c[2];
            q15_t a1 = c[3];
            q15_t a2 = c[4];
            
            // Calculations in Q30/Q31
            // y[n] = b0*x + d1
            // d1 is Q15. b0*x is Q30.
            // We need to maintain state in higher precision or match Q formats.
            // Usually state is kept in Q15 for memory, but this degrades precision.
            // Better to keep state in Q31 if possible, but struct says q15_t* state.
            // So we assume state is Q15.
            
            // b0*x >> 15 -> Q15
            q31_t term_b0 = ((q31_t)b0 * x) >> 15;
            q31_t stage_y_32 = term_b0 + d[0];
            
            // Saturation for y
            q15_t stage_y;
            if (stage_y_32 > 32767) stage_y = 32767;
            else if (stage_y_32 < -32768) stage_y = -32768;
            else stage_y = (q15_t)stage_y_32;
            
            // Update d1
            // d1 = b1*x - a1*y + d2
            q31_t term_b1 = ((q31_t)b1 * x) >> 15;
            q31_t term_a1 = ((q31_t)a1 * stage_y) >> 15;
            q31_t d1_32 = term_b1 - term_a1 + d[1];
            
            // Saturate d1
            if (d1_32 > 32767) d[0] = 32767;
            else if (d1_32 < -32768) d[0] = -32768;
            else d[0] = (q15_t)d1_32;
            
            // Update d2
            // d2 = b2*x - a2*y
            q31_t term_b2 = ((q31_t)b2 * x) >> 15;
            q31_t term_a2 = ((q31_t)a2 * stage_y) >> 15;
            q31_t d2_32 = term_b2 - term_a2;
            
            // Saturate d2
            if (d2_32 > 32767) d[1] = 32767;
            else if (d2_32 < -32768) d[1] = -32768;
            else d[1] = (q15_t)d2_32;
            
            x = stage_y;
        }
        output[n] = x;
    }
    return EIF_STATUS_OK;
}

// --- Statistics ---
q15_t eif_dsp_rms_q15(const q15_t* input, size_t length) {
    if (!input || length == 0) return 0;
    
    q63_t sum_sq = 0; 
    for (size_t i = 0; i < length; i++) {
        sum_sq += (q31_t)input[i] * input[i];
    }
    
    q31_t mean_sq = (q31_t)(sum_sq / length);
    return eif_q15_sqrt((q15_t)(mean_sq >> 15));
}

q15_t eif_dsp_zcr_q15(const q15_t* input, size_t length) {
    if (!input || length < 2) return 0;
    
    size_t crossings = 0;
    for (size_t i = 1; i < length; i++) {
        if ((input[i-1] >= 0 && input[i] < 0) || (input[i-1] < 0 && input[i] >= 0)) {
            crossings++;
        }
    }
    
    int32_t rate = (crossings * 32768) / (length - 1);
    if (rate > 32767) return 32767;
    return (q15_t)rate;
}

// --- MFCC ---
eif_status_t eif_dsp_mfcc_init_q15(eif_mfcc_config_q15_t* config) {
    if (!config || !config->filter_bank || !config->dct_matrix) return EIF_STATUS_INVALID_ARGUMENT;
    
    int fft_len = config->fft_length;
    int num_filters = config->num_filters;
    int num_mfcc = config->num_mfcc;
    
    // Dummy Filterbank: Identity-like (just for testing flow)
    for (int i = 0; i < num_filters * (fft_len/2 + 1); i++) {
        config->filter_bank[i] = 100; 
    }
    
    // Dummy DCT Matrix
    for (int i = 0; i < num_mfcc * num_filters; i++) {
        config->dct_matrix[i] = 100;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_dsp_mfcc_compute_q15(const eif_mfcc_config_q15_t* config, const q15_t* fft_mag, q15_t* mfcc_out, eif_memory_pool_t* pool) {
    if (!config || !fft_mag || !mfcc_out || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int num_filters = config->num_filters;
    int num_mfcc = config->num_mfcc;
    int bin_count = config->fft_length / 2 + 1;
    
    q15_t* mel_energies = (q15_t*)eif_memory_alloc(pool, num_filters * sizeof(q15_t), 4);
    if (!mel_energies) return EIF_STATUS_OUT_OF_MEMORY;
    
    // 1. Compute Mel Energies
    for (int i = 0; i < num_filters; i++) {
        q31_t sum = 0;
        for (int j = 0; j < bin_count; j++) {
            sum += (q31_t)config->filter_bank[i * bin_count + j] * fft_mag[j];
        }
        mel_energies[i] = (q15_t)(sum >> 15); 
    }
    
    // 2. Logarithm
    for (int i = 0; i < num_filters; i++) {
        mel_energies[i] = eif_q15_log(mel_energies[i]);
    }
    
    // 3. DCT
    for (int i = 0; i < num_mfcc; i++) {
        q31_t sum = 0;
        for (int j = 0; j < num_filters; j++) {
            sum += (q31_t)config->dct_matrix[i * num_filters + j] * mel_energies[j];
        }
        mfcc_out[i] = (q15_t)(sum >> 15);
    }
    
    return EIF_STATUS_OK;
}
