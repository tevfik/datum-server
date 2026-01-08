#include "../framework/eif_test_runner.h"
#include "eif_dsp_fft_fixed.h"
#include <math.h>
#include <stdlib.h>

// Helper to generate a signal
// Q15 signal
void generate_cosine_q15(q15_t *buf, int len, float freq, float sample_rate, float amplitude) {
    for (int i = 0; i < len; i++) {
        float raw = amplitude * cosf(2.0f * 3.14159f * freq * (float)i / sample_rate);
        buf[2*i] = EIF_FLOAT_TO_Q15(raw); // Real
        buf[2*i + 1] = 0;                 // Imag
    }
}

// Test Initialization
bool test_fft_fixed_init(void) {
    eif_dsp_fft_fixed_instance_t S = {0};
    q15_t twiddles[512] = {0}; 
    
    // We test N=64
    uint16_t N = 64;
    S.pTwiddle = twiddles;
    S.pBitRevTable = malloc(N * sizeof(uint16_t));
    
    eif_status_t status = eif_dsp_fft_init_fixed(&S, N);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_NOT_NULL(S.pBitRevTable);
    
    // Check Twiddles
    // Twid[0] should be 1.0 (Real=32767, Im=0)
    // Tolerance for float conversion: 1-2 bits
    TEST_ASSERT_EQUAL_INT(32767, S.pTwiddle[0]);
    TEST_ASSERT_EQUAL_INT(0, S.pTwiddle[1]);
    
    // Twid[N/4] = -j1 => Real=0, Im=-32767
    // Index k=16 -> Array index 32,33
    q15_t im = S.pTwiddle[33];
    TEST_ASSERT_TRUE(abs(im - (-32767)) < 5);

    free(S.pBitRevTable);
    return true;
}

// Test FFT DC Caclulation
bool test_fft_fixed_dc(void) {
    uint16_t N = 64;
    eif_dsp_fft_fixed_instance_t S = {0};
    q15_t twiddles[64]; // N=64 -> 32 complex is usually enough, but init loop iterates N/2. 
                        // Size 64 q15 is exactly N/2 complex.
    uint16_t bitrev[64];
    S.pTwiddle = twiddles;
    S.pBitRevTable = bitrev;
    
    eif_dsp_fft_init_fixed(&S, N); 
    
    // DC Input: 0.5 everywhere
    q15_t signal[128]; // 64 complex
    for(int i=0; i<N; i++) {
        signal[2*i] = EIF_FLOAT_TO_Q15(0.5f);
        signal[2*i+1] = 0;
    }
    
    // FFT
    eif_dsp_fft_c15(&S, signal, signal, 0);
    
    // Output should have peak at bin 0.
    // Scaling by 1/N. Output = Input * N / N = Input.
    // So 0.5.
    
    q15_t bin0_r = signal[0];
    q15_t bin0_i = signal[1];
    
    // Tolerance
    TEST_ASSERT_TRUE(abs(bin0_r - EIF_FLOAT_TO_Q15(0.5f)) < 50);
    TEST_ASSERT_TRUE(abs(bin0_i - 0) < 50);
    
    return true;
}

// Test FFT Sine
bool test_fft_fixed_sine(void) {
    uint16_t N = 64;
    eif_dsp_fft_fixed_instance_t S = {0};
    q15_t twiddles[64];
    uint16_t bitrev[64];
    S.pTwiddle = twiddles;
    S.pBitRevTable = bitrev;
    eif_dsp_fft_init_fixed(&S, N); 
    
    // Generate Sine at 4th bin freq
    q15_t signal[128];
    for (int i=0; i<N; i++) {
        // cos(2*pi * 4 * i / 64)
        float val = 0.5f * cosf(2.0f * 3.14159f * 4.0f * i / 64.0f);
        signal[2*i] = EIF_FLOAT_TO_Q15(val);
        signal[2*i+1] = 0;
    }
    
    eif_dsp_fft_c15(&S, signal, signal, 0);
    
    // Peak should be at bin 4 (and bin 60 symm)
    // Scaling: Peak = A/2 = 0.25 (since Real Cosine splits energy)
    
    // We check Magnitude
    q15_t mags[64];
    eif_dsp_cmplx_mag_q15(signal, mags, N);
    
    // Bin 4
    q15_t expected = EIF_FLOAT_TO_Q15(0.25f);
    // Allow larger tolerance due to fixed point scaling attenuation (approx 10% loss)
    TEST_ASSERT_TRUE(abs(mags[4] - expected) < 800); 
    
    // Bin 10 should be near zero (Noise floor check)
    // 6-stage truncation noise can be significant.
    TEST_ASSERT_TRUE(abs(mags[10]) < 800);
    
    return true;
}

int run_fft_fixed_tests(void) {
    int failed = 0;
    // We manually invoke because header macros rely on RUN_TEST which expects function
    if (!test_fft_fixed_init()) { printf("test_fft_fixed_init FAILED\n"); failed++; }
    if (!test_fft_fixed_dc()) { printf("test_fft_fixed_dc FAILED\n"); failed++; }
    if (!test_fft_fixed_sine()) { printf("test_fft_fixed_sine FAILED\n"); failed++; }
    return failed;
}

