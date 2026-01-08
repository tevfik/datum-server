#include "eif_test_runner.h"
#include "eif_audio.h"
#include <math.h>
#include <string.h>

static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;

bool test_audio_pipeline(void) {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_audio_config_t config = {
        .sample_rate = 16000,
        .frame_length = 512,
        .frame_stride = 256,
        .num_mfcc = 13,
        .num_filters = 26,
        .lower_freq = 20.0f,
        .upper_freq = 4000.0f,
        .output_frames = 10
    };
    
    eif_audio_preprocessor_t audio_ctx;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_audio_init(&audio_ctx, &config, &pool));
    
    // Generate Sine Wave (1kHz)
    float32_t sine_wave[16000];
    for(int i=0; i<16000; i++) {
        sine_wave[i] = sinf(2.0f * M_PI * 1000.0f * i / 16000.0f);
    }
    
    // Push 1 second of audio
    // This should generate ~62 frames (16000 / 256)
    // Our output buffer holds 10 frames.
    // So it should shift and keep the last 10 frames.
    
    // Push in chunks
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_audio_push(&audio_ctx, sine_wave, 8000));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_audio_push(&audio_ctx, sine_wave + 8000, 8000));
    
    // Check features
    const float32_t* features = eif_audio_get_features(&audio_ctx);
    
    // Just verify they are not all zero
    float32_t sum = 0.0f;
    for(int i=0; i<10 * 13; i++) {
        sum += fabsf(features[i]);
    }
    TEST_ASSERT_TRUE(sum > 1.0f);
    
    return true;
}

BEGIN_TEST_SUITE(run_audio_tests)
    RUN_TEST(test_audio_pipeline);
END_TEST_SUITE()
