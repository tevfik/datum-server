#include "../framework/eif_test_runner.h"
#include "eif_dsp.h"
#include "eif_generic.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool test_fft() {
    #define N 8
    float32_t data[2 * N]; // Real/Imag interleaved
    
    for (int i = 0; i < N; i++) {
        data[2*i] = sinf(2 * M_PI * 1.0f * i / 8.0f); // Real
        data[2*i+1] = 0.0f; // Imag
    }
    
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_fft_config_t config;
    eif_dsp_fft_init_f32(&config, N, &pool);
    
    eif_dsp_fft_f32(&config, data);
    
    float32_t mag_bin1 = sqrtf(data[2]*data[2] + data[3]*data[3]);
    // Expected 4.0
    TEST_ASSERT_EQUAL_FLOAT(4.0f, mag_bin1, 0.01f);
    
    eif_dsp_fft_deinit_f32(&config);
    return true;
}

bool test_ifft() {
    #define N_IFFT 8
    float32_t original[2 * N_IFFT];
    float32_t data[2 * N_IFFT];
    
    for (int i = 0; i < N_IFFT; i++) {
        original[2*i] = (float)i; // Real
        original[2*i+1] = 0.0f;   // Imag
        data[2*i] = original[2*i];
        data[2*i+1] = original[2*i+1];
    }
    
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_fft_config_t config;
    eif_dsp_fft_init_f32(&config, N_IFFT, &pool);
    
    // Forward FFT
    eif_dsp_fft_f32(&config, data);
    
    // Inverse FFT
    config.inverse = true;
    eif_dsp_fft_f32(&config, data);
    
    // Check reconstruction
    for (int i = 0; i < 2 * N_IFFT; i++) {
        TEST_ASSERT_EQUAL_FLOAT(original[i], data[i], 0.001f);
    }
    
    eif_dsp_fft_deinit_f32(&config);
    return true;
}

bool test_rfft() {
    #define N_RFFT 16
    float32_t input[N_RFFT];
    float32_t output[N_RFFT + 2]; // Complex output
    
    // 2 Hz signal in 16 Hz sample rate
    for (int i = 0; i < N_RFFT; i++) {
        input[i] = sinf(2 * M_PI * 2.0f * i / 16.0f);
    }
    
    uint8_t pool_buffer[2048];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_fft_config_t config;
    eif_dsp_fft_init_f32(&config, N_RFFT, &pool); // Init for N real points
    
    eif_dsp_rfft_f32(&config, input, output);
    
    // Bin 2 should have peak. Index 2 (Complex) -> output[4], output[5]
    float32_t mag_bin2 = sqrtf(output[4]*output[4] + output[5]*output[5]);
    // Expected 8.0
    TEST_ASSERT_EQUAL_FLOAT(8.0f, mag_bin2, 0.01f);
    
    eif_dsp_fft_deinit_f32(&config);
    return true;
}

bool test_mfcc() {
    eif_mfcc_config_t config = {
        .num_mfcc = 13,
        .num_filters = 26,
        .fft_length = 256,
        .sample_rate = 16000,
        .low_freq = 0.0f,
        .high_freq = 8000.0f
    };
    
    uint8_t pool_buffer[8192]; // Needs more space for MFCC tables
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_dsp_mfcc_init_f32(&config, &pool);
    
    float32_t fft_mag[129]; // 256/2 + 1
    for(int i=0; i<129; i++) fft_mag[i] = 1.0f; // Flat spectrum
    
    float32_t mfcc[13];
    eif_dsp_mfcc_compute_f32(&config, fft_mag, mfcc, &pool);
    
    // Check first coefficient (approx 33.89)
    TEST_ASSERT_EQUAL_FLOAT(33.89f, mfcc[0], 0.1f);
    return true;
}

bool test_fir() {
    // Simple Moving Average (3-tap)
    float32_t coeffs[] = {0.333f, 0.333f, 0.333f};
    float32_t input[] = {1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f};
    float32_t output[6];
    
    eif_dsp_fir_f32(input, output, 6, coeffs, 3);
    
    TEST_ASSERT_EQUAL_FLOAT(0.333f, output[0], 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(0.666f, output[1], 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(0.999f, output[2], 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(0.999f, output[3], 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(0.666f, output[4], 0.01f);
    
    return true;
}

bool test_stft() {
    #define STFT_FFT_LEN 16
    #define STFT_HOP_LEN 8
    #define STFT_WIN_LEN 16
    #define STFT_FRAMES 2
    
    // 2 Hz signal
    float32_t input[32];
    for (int i = 0; i < 32; i++) {
        input[i] = sinf(2 * M_PI * 2.0f * i / 16.0f);
    }
    
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_stft_config_t config;
    eif_dsp_stft_init_f32(&config, STFT_FFT_LEN, STFT_HOP_LEN, STFT_WIN_LEN, &pool);
    
    float32_t output_mag[STFT_FRAMES * (STFT_FFT_LEN / 2 + 1)];
    eif_dsp_stft_compute_f32(&config, input, output_mag, STFT_FRAMES);
    
    // Bin 2 (index 2) should be high in both frames
    float32_t mag_frame0_bin2 = output_mag[0 * 9 + 2];
    float32_t mag_frame1_bin2 = output_mag[1 * 9 + 2];
    
    // Expected value depends on window loss, but should be significant (> 1.0)
    TEST_ASSERT(mag_frame0_bin2 > 1.0f);
    TEST_ASSERT(mag_frame1_bin2 > 1.0f);
    
    eif_dsp_stft_deinit_f32(&config);
    return true;
}

bool test_resample() {
    // Input: 0, 10, 20, 30 (Length 4)
    float32_t input[] = {0.0f, 10.0f, 20.0f, 30.0f};
    // Output: Length 7 (Upsample by ~2x)
    float32_t output[7];
    
    eif_dsp_resample_linear_f32(input, 4, output, 7);
    
    // Expected: 0, 5, 10, 15, 20, 25, 30
    TEST_ASSERT_EQUAL_FLOAT(0.0f, output[0], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, output[1], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, output[2], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, output[3], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, output[4], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(25.0f, output[5], 0.1f);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, output[6], 0.1f);
    
    return true;
}

bool test_vad() {
    // Silence (low energy)
    float32_t silence[] = {0.01f, -0.01f, 0.00f, 0.01f, -0.01f};
    TEST_ASSERT(eif_dsp_vad_energy_f32(silence, 5, 0.1f) == false);
    
    // Speech (high energy)
    float32_t speech[] = {0.5f, -0.5f, 0.8f, -0.8f, 0.5f};
    TEST_ASSERT(eif_dsp_vad_energy_f32(speech, 5, 0.1f) == true);
    
    // High ZCR (Noise/Fricative)
    float32_t noise[] = {0.1f, -0.1f, 0.1f, -0.1f, 0.1f};
    TEST_ASSERT(eif_dsp_vad_zcr_f32(noise, 5, 0.5f) == true);
    
    return true;
}

bool test_spectral_features() {
    // 4-point FFT -> 3 bins (0, Fs/4, Fs/2)
    // Fs = 100 Hz
    // Bins: 0 Hz, 25 Hz, 50 Hz
    int fft_size = 4;
    float32_t sample_rate = 100.0f;
    
    // Spectrum: Peak at 25 Hz
    float32_t mag[] = {0.0f, 10.0f, 0.0f}; 
    
    // Centroid: (0*0 + 25*10 + 50*0) / 10 = 250 / 10 = 25 Hz
    float32_t centroid = eif_dsp_spectral_centroid_f32(mag, fft_size, sample_rate);
    TEST_ASSERT_EQUAL_FLOAT(25.0f, centroid, 0.001f);
    
    // Rolloff (85%): Total = 10. Threshold = 8.5.
    // Bin 0: 0 < 8.5
    // Bin 1: 0+10 = 10 >= 8.5 -> Return 25 Hz
    float32_t rolloff = eif_dsp_spectral_rolloff_f32(mag, fft_size, sample_rate, 0.85f);
    TEST_ASSERT_EQUAL_FLOAT(25.0f, rolloff, 0.001f);
    
    // Flux
    float32_t prev_mag[] = {0.0f, 0.0f, 0.0f};
    // Diff: 0, 10, 0. Sq: 0, 100, 0. Sum: 100. Sqrt: 10.
    float32_t flux = eif_dsp_spectral_flux_f32(mag, prev_mag, fft_size);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, flux, 0.001f);
    
    return true;
}

bool test_peak_detection() {
    // 0, 1, 5, 1, 0, 1, 4, 1, 0
    // Peaks at index 2 (val 5) and index 6 (val 4)
    float32_t input[] = {0.0f, 1.0f, 5.0f, 1.0f, 0.0f, 1.0f, 4.0f, 1.0f, 0.0f};
    int indices[5];
    
    // Threshold 2.0, Min Dist 2
    int count = eif_dsp_peak_detection_f32(input, 9, 2.0f, 2, indices, 5);
    
    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_INT(2, indices[0]);
    TEST_ASSERT_EQUAL_INT(6, indices[1]);
    
    // Min Dist 5 (should skip second peak)
    count = eif_dsp_peak_detection_f32(input, 9, 2.0f, 5, indices, 5);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_INT(2, indices[0]);
    
    return true;
}

bool test_envelope() {
    // AM Signal: Carrier 10Hz, Envelope 1Hz
    // But for simple test, just a few points
    // 10, -10, 10, -10 -> Envelope should be around 10
    float32_t input[] = {10.0f, -10.0f, 10.0f, -10.0f, 10.0f, -10.0f};
    float32_t output[6];
    
    // Decay 0.5 -> Alpha 0.5
    // y0 = 0.5*10 + 0 = 5
    // y1 = 0.5*10 + 0.5*5 = 7.5
    // y2 = 0.5*10 + 0.5*7.5 = 8.75
    // ... approaches 10
    
    eif_dsp_envelope_f32(input, output, 6, 0.5f);
    
    TEST_ASSERT_EQUAL_FLOAT(5.0f, output[0], 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(7.5f, output[1], 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(8.75f, output[2], 0.01f);
    
    return true;
}

BEGIN_TEST_SUITE(run_dsp_tests)
    RUN_TEST(test_fft);
    RUN_TEST(test_ifft);
    RUN_TEST(test_rfft);
    RUN_TEST(test_mfcc);
    RUN_TEST(test_fir);
    RUN_TEST(test_stft);
    RUN_TEST(test_resample);
    RUN_TEST(test_vad);
    RUN_TEST(test_spectral_features);
    RUN_TEST(test_peak_detection);
    RUN_TEST(test_envelope);
END_TEST_SUITE()
