#include "../framework/eif_test_runner.h"
#include "eif_audio.h"
#include "eif_memory.h"

#define POOL_SIZE 32768
static uint8_t pool_buffer[POOL_SIZE];
static eif_memory_pool_t pool;

static void setup_audio(void) {
    eif_memory_pool_init(&pool, pool_buffer, POOL_SIZE);
}

bool test_audio_init_invalid(void) {
    setup_audio();
    eif_audio_preprocessor_t ctx;
    eif_audio_config_t config;
    
    TEST_ASSERT(eif_audio_init(NULL, &config, &pool) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_audio_init(&ctx, NULL, &pool) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_audio_init(&ctx, &config, NULL) == EIF_STATUS_INVALID_ARGUMENT);
    
    return true;
}

bool test_audio_init_oom(void) {
    setup_audio();
    eif_audio_preprocessor_t ctx;
    eif_audio_config_t config = {
        .sample_rate = 16000,
        .frame_length = 1024,
        .frame_stride = 512,
        .num_filters = 32,
        .num_mfcc = 13,
        .lower_freq = 20,
        .upper_freq = 4000,
        .output_frames = 50
    };
    
    uint8_t small_buffer[128];
    eif_memory_pool_t small_pool;
    eif_memory_pool_init(&small_pool, small_buffer, 128);
    
    // Should fail at MFCC init or STFT init or Buffer alloc
    TEST_ASSERT(eif_audio_init(&ctx, &config, &small_pool) != EIF_STATUS_OK);
    
    return true;
}

bool test_audio_push_invalid(void) {
    setup_audio();
    eif_audio_preprocessor_t ctx;
    float32_t samples[10];
    
    TEST_ASSERT(eif_audio_push(NULL, samples, 10) == EIF_STATUS_INVALID_ARGUMENT);
    TEST_ASSERT(eif_audio_push(&ctx, NULL, 10) == EIF_STATUS_INVALID_ARGUMENT);
    
    return true;
}

bool test_audio_reset(void) {
    setup_audio();
    eif_audio_preprocessor_t ctx;
    eif_audio_config_t config = {
        .sample_rate = 16000,
        .frame_length = 256,
        .frame_stride = 128,
        .num_filters = 20,
        .num_mfcc = 10,
        .lower_freq = 20,
        .upper_freq = 4000,
        .output_frames = 10
    };
    
    TEST_ASSERT(eif_audio_init(&ctx, &config, &pool) == EIF_STATUS_OK);
    
    // Push some data
    float32_t samples[256];
    for(int i=0; i<256; i++) samples[i] = (float)i;
    eif_audio_push(&ctx, samples, 256);
    
    TEST_ASSERT_EQUAL_INT(128, ctx.config.frame_stride);
    TEST_ASSERT_EQUAL_INT(256, ctx.config.frame_length);
    TEST_ASSERT_EQUAL_INT(128, ctx.samples_available);
    
    eif_audio_reset(&ctx);
    
    TEST_ASSERT(ctx.samples_available == 0);
    TEST_ASSERT(ctx.write_pos == 0);
    TEST_ASSERT(ctx.read_pos == 0);
    
    return true;
}

BEGIN_TEST_SUITE(run_audio_coverage_tests)
    RUN_TEST(test_audio_init_invalid);
    RUN_TEST(test_audio_init_oom);
    RUN_TEST(test_audio_push_invalid);
    RUN_TEST(test_audio_reset);
END_TEST_SUITE()
