#include <stdio.h>
#include <math.h>
#include "eif_dsp.h"
#include "eif_generic.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "eif_memory.h"

void test_fft() {
    printf("Testing Optimized FFT...\n");
    
    #define N 8
    float32_t data[2 * N]; // Real/Imag interleaved
    
    for (int i = 0; i < N; i++) {
        data[2*i] = sinf(2 * M_PI * 1.0f * i / 8.0f); // Real
        data[2*i+1] = 0.0f; // Imag
    }
    
    eif_fft_config_t config;
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_dsp_fft_init_f32(&config, N, &pool);
    
    eif_dsp_fft_f32(&config, data);
    
    float32_t mag_bin1 = sqrtf(data[2]*data[2] + data[3]*data[3]);
    printf("FFT Bin 1 Magnitude: %.4f (Expected 4.0)\n", mag_bin1);
    
    // eif_dsp_fft_deinit(&config); // No deinit needed with pool
}

void test_rfft() {
    printf("Testing Real FFT (RFFT)...\n");
    
    #define N_RFFT 16
    float32_t input[N_RFFT];
    float32_t output[N_RFFT + 2]; // Complex output
    
    // 2 Hz signal in 16 Hz sample rate
    for (int i = 0; i < N_RFFT; i++) {
        input[i] = sinf(2 * M_PI * 2.0f * i / 16.0f);
    }
    
    eif_fft_config_t config;
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_dsp_fft_init_f32(&config, N_RFFT / 2, &pool); // Init for N/2 complex
    
    eif_dsp_rfft_f32(&config, input, output);
    
    // Bin 2 should have peak. Index 2 (Complex) -> output[4], output[5]
    float32_t mag_bin2 = sqrtf(output[4]*output[4] + output[5]*output[5]);
    printf("RFFT Bin 2 Magnitude: %.4f (Expected 8.0)\n", mag_bin2);
}

void test_mfcc() {
    printf("Testing MFCC...\n");
    
    eif_mfcc_config_t config;
    config.num_mfcc = 13;
    config.num_filters = 26;
    config.fft_length = 256;
    config.sample_rate = 16000;
    config.low_freq = 0.0f;
    config.high_freq = 8000.0f;
    
    uint8_t pool_buffer[8192];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_dsp_mfcc_init_f32(&config, &pool);
    
    float32_t fft_mag[129]; // 256/2 + 1
    for(int i=0; i<129; i++) fft_mag[i] = 1.0f; // Flat spectrum
    
    float32_t mfcc[13];
    eif_dsp_mfcc_compute_f32(&config, fft_mag, mfcc, &pool);
    
    printf("MFCC[0]: %.4f\n", mfcc[0]);
}

void test_fir() {
    printf("Testing FIR Filter...\n");
    
    // Simple Moving Average (3-tap)
    float32_t coeffs[] = {0.333f, 0.333f, 0.333f};
    float32_t input[] = {1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f};
    float32_t output[6];
    
    eif_dsp_fir_f32(input, output, 6, coeffs, 3);
    
    printf("FIR Output: %.2f, %.2f, %.2f, %.2f, %.2f\n", 
           output[0], output[1], output[2], output[3], output[4]);
}

int main() {
    test_fft();
    test_rfft();
    test_mfcc();
    test_fir();
    return 0;
}
