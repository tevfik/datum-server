#include "../framework/eif_test_runner.h"
#include "eif_dsp.h"
#include "eif_memory.h"

#define POOL_SIZE 4096
static uint8_t pool_buffer[POOL_SIZE];
static eif_memory_pool_t pool;

static void setup_transform(void) {
    eif_memory_pool_init(&pool, pool_buffer, POOL_SIZE);
}

bool test_fft_init_invalid(void) {
    setup_transform();
    eif_fft_config_t config;
    TEST_ASSERT(eif_dsp_fft_init_f32(NULL, 64, &pool) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_fft_init_f32(&config, 64, NULL) == EIF_STATUS_INVALID_ARGUMENT);
    return true;
}

bool test_fft_init_oom(void) {
    setup_transform();
    eif_fft_config_t config;
    // Create a small pool that will fail allocation
    uint8_t small_buffer[16];
    eif_memory_pool_t small_pool;
    eif_memory_pool_init(&small_pool, small_buffer, 16);
    
    TEST_ASSERT(eif_dsp_fft_init_f32(&config, 1024, &small_pool) == EIF_STATUS_OUT_OF_MEMORY);
    return true;
}

bool test_fft_deinit(void) {
    setup_transform();
    eif_fft_config_t config;
    eif_dsp_fft_init_f32(&config, 64, &pool);
    
    TEST_ASSERT(eif_dsp_fft_deinit_f32(&config) == EIF_STATUS_OK);
    TEST_ASSERT(config.bit_reverse_indices == NULL);
    TEST_ASSERT(config.twiddle_factors == NULL);
    TEST_ASSERT(config.rfft_scratch == NULL);
    
    // Test NULL config
    TEST_ASSERT(eif_dsp_fft_deinit_f32(NULL) == EIF_STATUS_OK);
    return true;
}

bool test_fft_invalid_args(void) {
    setup_transform();
    eif_fft_config_t config;
    float32_t data[128];
    
    TEST_ASSERT(eif_dsp_fft_f32(NULL, data) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_fft_f32(&config, NULL) == EIF_STATUS_INVALID_ARGUMENT);
    return true;
}

bool test_rfft_invalid_args(void) {
    setup_transform();
    eif_fft_config_t config;
    float32_t input[64];
    float32_t output[128];
    
    eif_dsp_fft_init_f32(&config, 32, &pool); // N/2 = 32
    
    TEST_ASSERT(eif_dsp_rfft_f32(NULL, input, output) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_rfft_f32(&config, NULL, output) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_rfft_f32(&config, input, NULL) == EIF_STATUS_INVALID_ARGUMENT);
    
    // Corrupt scratch to test check? No, scratch is checked in init.
    // But if we manually set scratch to NULL
    config.rfft_scratch = NULL;
    TEST_ASSERT(eif_dsp_rfft_f32(&config, input, output) == EIF_STATUS_INVALID_ARGUMENT);
    
    return true;
}

bool test_stft_init_invalid(void) {
    setup_transform();
    eif_stft_config_t config;
    
    TEST_ASSERT(eif_dsp_stft_init_f32(NULL, 64, 32, 64, &pool) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_stft_init_f32(&config, 64, 32, 64, NULL) == EIF_STATUS_INVALID_ARGUMENT);
    
    // Window > FFT length
    TEST_ASSERT(eif_dsp_stft_init_f32(&config, 64, 32, 128, &pool) == EIF_STATUS_INVALID_ARGUMENT);
    
    return true;
}

bool test_stft_init_oom(void) {
    setup_transform();
    eif_stft_config_t config;
    uint8_t small_buffer[128]; // Enough for struct but not for arrays
    eif_memory_pool_t small_pool;
    eif_memory_pool_init(&small_pool, small_buffer, 128);
    
    // This should fail at FFT init or Window alloc
    TEST_ASSERT(eif_dsp_stft_init_f32(&config, 64, 32, 64, &small_pool) == EIF_STATUS_OUT_OF_MEMORY);
    
    return true;
}

bool test_stft_deinit(void) {
    setup_transform();
    eif_stft_config_t config;
    eif_dsp_stft_init_f32(&config, 64, 32, 64, &pool);
    
    TEST_ASSERT(eif_dsp_stft_deinit_f32(&config) == EIF_STATUS_OK);
    TEST_ASSERT(config.window == NULL);
    TEST_ASSERT(config.fft_buffer == NULL);
    
    TEST_ASSERT(eif_dsp_stft_deinit_f32(NULL) == EIF_STATUS_OK);
    return true;
}

bool test_stft_compute_invalid(void) {
    setup_transform();
    eif_stft_config_t config;
    float32_t input[100];
    float32_t output[100];
    
    eif_dsp_stft_init_f32(&config, 64, 32, 64, &pool);
    
    TEST_ASSERT(eif_dsp_stft_compute_f32(NULL, input, output, 1) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_stft_compute_f32(&config, NULL, output, 1) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_stft_compute_f32(&config, input, NULL, 1) == EIF_STATUS_INVALID_ARGUMENT);
    
    config.fft_buffer = NULL;
    TEST_ASSERT(eif_dsp_stft_compute_f32(&config, input, output, 1) == EIF_STATUS_INVALID_ARGUMENT);
    
    return true;
}

bool test_resample_linear_invalid(void) {
    setup_transform();
    float32_t input[10];
    float32_t output[10];
    
    TEST_ASSERT(eif_dsp_resample_linear_f32(NULL, 10, output, 10) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_resample_linear_f32(input, 10, NULL, 10) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_resample_linear_f32(input, 0, output, 10) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_resample_linear_f32(input, 10, output, 0) == EIF_STATUS_INVALID_ARGUMENT);
    
    return true;
}

bool test_resample_cubic(void) {
    setup_transform();
    float32_t input[4] = {0.0f, 1.0f, 2.0f, 3.0f};
    float32_t output[7]; // Upsample by 2x (approx)
    
    // 0, 0.5, 1, 1.5, 2, 2.5, 3
    
    TEST_ASSERT(eif_dsp_resample_cubic_f32(input, 4, output, 7) == EIF_STATUS_OK);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.5f, output[1]); // Cubic might not be exact linear
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.5f, output[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, output[4]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.5f, output[5]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, output[6]);
    
    return true;
}

bool test_resample_cubic_invalid(void) {
    setup_transform();
    float32_t input[10];
    float32_t output[10];
    
    TEST_ASSERT(eif_dsp_resample_cubic_f32(NULL, 10, output, 10) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_resample_cubic_f32(input, 10, NULL, 10) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_resample_cubic_f32(input, 0, output, 10) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_dsp_resample_cubic_f32(input, 10, output, 0) == EIF_STATUS_INVALID_ARGUMENT);
    
    return true;
}

BEGIN_TEST_SUITE(run_transform_coverage_tests)
    RUN_TEST(test_fft_init_invalid);
    RUN_TEST(test_fft_init_oom);
    RUN_TEST(test_fft_deinit);
    RUN_TEST(test_fft_invalid_args);
    RUN_TEST(test_rfft_invalid_args);
    RUN_TEST(test_stft_init_invalid);
    RUN_TEST(test_stft_init_oom);
    RUN_TEST(test_stft_deinit);
    RUN_TEST(test_stft_compute_invalid);
    RUN_TEST(test_resample_linear_invalid);
    RUN_TEST(test_resample_cubic);
    RUN_TEST(test_resample_cubic_invalid);
END_TEST_SUITE()
