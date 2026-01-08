#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "eif_dsp_fixed.h"
#include "eif_memory.h"

#define FFT_LEN 32

void test_fft_q15_dc() {
    printf("Testing Q15 FFT (Len %d)...\n", FFT_LEN);
    
    eif_fft_config_q15_t config;
    // Memory pool for FFT tables
    uint8_t fft_buf[4096];
    eif_memory_pool_t fft_pool;
    eif_memory_pool_init(&fft_pool, fft_buf, sizeof(fft_buf));
    eif_dsp_fft_init_q15(&config, FFT_LEN, &fft_pool);
    
    // Input: DC signal (1.0 constant)
    // 1.0 in Q15 is approx 32767
    q15_t* data = (q15_t*)malloc(2 * FFT_LEN * sizeof(q15_t));
    for (int i = 0; i < FFT_LEN; i++) {
        data[2*i] = EIF_FLOAT_TO_Q15(0.5f); // 0.5 is safe (16384)
        data[2*i+1] = 0;
    }
    
    eif_dsp_fft_q15(&config, data);
    
    // Expected: DC bin (index 0) should be high, others 0.
    // Note: FFT scales down by 1/N total (1/2 per stage).
    // So output DC should be Input_DC * N / N = Input_DC?
    // Let's check scaling.
    // Stage 1: (x+y)/2.
    // ... Stage log2(N).
    // Total scaling 1/N.
    // Sum of 1.0s is N. Scaled by 1/N is 1.0.
    // So DC bin should be approx 0.5.
    
    printf("DC Bin: %d (%.4f)\n", data[0], EIF_Q15_TO_FLOAT(data[0]));
    printf("Nyquist Bin: %d (%.4f)\n", data[FFT_LEN], EIF_Q15_TO_FLOAT(data[FFT_LEN]));
    
    if (abs(data[0] - EIF_FLOAT_TO_Q15(0.5f)) < 100) {
        printf("SUCCESS: FFT Q15 DC Test\n");
    } else {
        printf("FAILURE: FFT Q15 DC Test\n");
    }
    
    free(data);
    eif_dsp_fft_deinit_q15(&config);
}

void test_windowing_q15() {
    printf("Testing Q15 Windowing...\n");
    q15_t window[32];
    eif_dsp_window_hamming_q15(window, 32);
    printf("Hamming[0]=%d, Hamming[16]=%d\n", window[0], window[16]);
    // Hamming[0] should be small (0.08 -> ~2621)
    // Hamming[16] (center) should be max (1.0 -> 32767)
}

void test_stats_q15() {
    printf("Testing Q15 Stats...\n");
    q15_t input[10] = {1000, -1000, 1000, -1000, 1000, -1000, 1000, -1000, 1000, -1000};
    
    q15_t rms = eif_dsp_rms_q15(input, 10);
    printf("RMS: %d (Expected ~1000)\n", rms);
    
    q15_t zcr = eif_dsp_zcr_q15(input, 10);
    printf("ZCR: %d (Expected ~1.0 -> 32767)\n", zcr);
}

void test_mfcc_q15() {
    printf("Testing Q15 MFCC...\n");
    
    // Config
    eif_mfcc_config_q15_t config = {
        .num_mfcc = 13,
        .num_filters = 26,
        .fft_length = 32,
        .sample_rate = 16000,
        .low_freq = 20,
        .high_freq = 4000
    };
    
    // Allocate buffers for config (User responsibility)
    config.filter_bank = (q15_t*)malloc(config.num_filters * (config.fft_length/2 + 1) * sizeof(q15_t));
    config.dct_matrix = (q15_t*)malloc(config.num_mfcc * config.num_filters * sizeof(q15_t));
    
    eif_dsp_mfcc_init_q15(&config);
    
    // Input: FFT Magnitude (Dummy)
    q15_t fft_mag[17]; // 32/2 + 1
    for(int i=0; i<17; i++) fft_mag[i] = 1000; // Constant energy
    
    q15_t mfcc_out[13];
    
    // Memory Pool for temp
    uint8_t buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    if (eif_dsp_mfcc_compute_q15(&config, fft_mag, mfcc_out, &pool) == EIF_STATUS_OK) {
        printf("MFCC[0]=%d\n", mfcc_out[0]);
        printf("SUCCESS: MFCC Q15 Run\n");
    } else {
        printf("FAILURE: MFCC Q15 Run\n");
    }
    
    free(config.filter_bank);
    free(config.dct_matrix);
}

void test_filters_q15() {
    printf("Testing Q15 Filters...\n");
    
    // FIR Test: Impulse Response
    q15_t input[10] = {32767, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Impulse
    q15_t coeffs[3] = {16384, 8192, 4096}; // 0.5, 0.25, 0.125
    q15_t output[10];
    
    eif_dsp_fir_q15(input, output, 10, coeffs, 3);
    
    printf("FIR Out: %d, %d, %d (Expected ~16384, ~8192, ~4096)\n", output[0], output[1], output[2]);
    
    // IIR Test: Simple decay
    // y[n] = 0.5*x[n] + 0.5*y[n-1]
    // b0=0.5, b1=0, b2=0, a1=-0.5, a2=0?
    // Formula: d1 = b1*x - a1*y + d2
    // If we want y[n] = 0.5*x + 0.5*y[n-1]
    // b0=0.5.
    // d1[n] = 0.5*y[n].
    // d1[n-1] = 0.5*y[n-1].
    // y[n] = b0*x + d1[n-1] = 0.5*x + 0.5*y[n-1]. Correct.
    // So b1=0, a1=-0.5?
    // d1[n] = b1*x - a1*y + d2
    // 0.5*y[n] = 0 - a1*y[n] -> a1 = -0.5.
    // Coeffs: [0.5, 0, 0, -0.5, 0] -> [16384, 0, 0, -16384, 0]
    
    q15_t iir_coeffs[5] = {16384, 0, 0, -16384, 0};
    q15_t state[2] = {0, 0};
    q15_t iir_out[10];
    
    eif_dsp_iir_q15(input, iir_out, 10, iir_coeffs, state, 1);
    
    printf("IIR Out: %d, %d, %d (Expected Decay)\n", iir_out[0], iir_out[1], iir_out[2]);
}

int main() {
    test_fft_q15_dc();
    test_windowing_q15();
    test_stats_q15();
    test_mfcc_q15();
    test_filters_q15();
    return 0;
}
