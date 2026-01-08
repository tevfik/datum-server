#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../framework/eif_test_runner.h"
#include "../../dsp/include/eif_ringbuffer.h"
#include "../../dsp/include/eif_adpcm.h"
#include "../../dsp/include/eif_vad_advanced.h"

// Define macros if not present in runner
#ifndef TEST_ASSERT_FALSE
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))
#endif

#ifndef TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL(expected, actual) TEST_ASSERT_EQUAL_INT(expected, actual)
#endif

#ifndef TEST_ASSERT_INT_WITHIN
#define TEST_ASSERT_INT_WITHIN(delta, expected, actual) \
    do { \
        int diff = abs((expected) - (actual)); \
        if (diff > (delta)) return false; \
    } while(0)
#endif

// =============================================================================
// Ring Buffer Tests
// =============================================================================
bool test_ringbuffer_basic(void) {
    uint8_t buffer[10];
    eif_ringbuffer_t rb;
    eif_ringbuffer_init(&rb, buffer, 10, false);

    TEST_ASSERT_EQUAL(0, eif_ringbuffer_available_read(&rb));
    TEST_ASSERT_EQUAL(10, eif_ringbuffer_available_write(&rb));

    uint8_t data_in[] = {1, 2, 3, 4, 5};
    size_t written = eif_ringbuffer_write(&rb, data_in, 5);
    TEST_ASSERT_EQUAL(5, written);
    TEST_ASSERT_EQUAL(5, eif_ringbuffer_available_read(&rb));
    TEST_ASSERT_EQUAL(5, eif_ringbuffer_available_write(&rb));

    uint8_t data_out[5];
    size_t read = eif_ringbuffer_read(&rb, data_out, 5);
    TEST_ASSERT_EQUAL(5, read);
    TEST_ASSERT_EQUAL(1, data_out[0]);
    TEST_ASSERT_EQUAL(5, data_out[4]);
    TEST_ASSERT_EQUAL(0, eif_ringbuffer_available_read(&rb));
    return true;
}

bool test_ringbuffer_overwrite(void) {
    uint8_t buffer[5];
    eif_ringbuffer_t rb;
    eif_ringbuffer_init(&rb, buffer, 5, true); // Overwrite enabled

    uint8_t data1[] = {1, 2, 3};
    eif_ringbuffer_write(&rb, data1, 3);
    
    uint8_t data2[] = {4, 5, 6}; // Should push out 1
    // Buffer should be 2, 3, 4, 5, 6
    eif_ringbuffer_write(&rb, data2, 3);

    TEST_ASSERT_EQUAL(5, eif_ringbuffer_available_read(&rb));
    
    uint8_t out[5];
    eif_ringbuffer_read(&rb, out, 5);
    
    // Expected: 2, 3, 4, 5, 6
    TEST_ASSERT_EQUAL(2, out[0]);
    TEST_ASSERT_EQUAL(6, out[4]);
    return true;
}

// =============================================================================
// ADPCM Tests
// =============================================================================
bool test_adpcm_codec(void) {
    eif_adpcm_state_t enc_state;
    eif_adpcm_state_t dec_state;

    eif_adpcm_init(&enc_state);
    eif_adpcm_init(&dec_state);

    // Generate sine wave
    int16_t input_pcm[128];
    for (int i=0; i<128; i++) {
        input_pcm[i] = (int16_t)(10000.0f * sinf(2.0f * 3.14159f * i / 32.0f));
    }

    uint8_t compressed[64];
    int16_t output_pcm[128];

    // Encode
    size_t comp_len = eif_adpcm_encode(&enc_state, input_pcm, compressed, 128);
    TEST_ASSERT_EQUAL(64, comp_len);

    // Decode
    size_t dec_len = eif_adpcm_decode(&dec_state, compressed, output_pcm, 64);
    TEST_ASSERT_EQUAL(128, dec_len);

    // Verify basic signal integrity
    float energy = 0.0f;
    for(int i=0; i<128; i++) {
        energy += fabsf((float)output_pcm[i]);
    }
    TEST_ASSERT_TRUE(energy > 1000.0f); // Make sure we decoded something signal-like
    
    TEST_ASSERT_INT_WITHIN(1000, 0, output_pcm[0]); 
    return true;
}

// =============================================================================
// Advanced VAD Tests
// =============================================================================
bool test_vad_advanced(void) {
    eif_vad_adv_config_t config;
    config.energy_threshold = 0.01f;
    config.zcr_threshold = 0.3f;
    config.frame_size = 256;
    config.sample_rate = 16000;
    config.hangvoer_frames = 5;

    eif_vad_adv_t vad;
    eif_vad_adv_init(&vad, &config);

    int16_t silence[256] = {0};
    int16_t voice[256];
    int16_t noise_zcr[256];

    // Generate voice (sine wave, low ZCR, high energy)
    for(int i=0; i<256; i++) {
        voice[i] = (int16_t)(10000.0f * sinf(2.0f * 3.14159f * i / 32.0f));
    }

    // Generate noise (high ZCR)
    for(int i=0; i<256; i++) {
        noise_zcr[i] = (int16_t)((rand() % 2000) * ((i%2==0)?1:-1));
    }

    // 1. Test Silence
    bool res = eif_vad_adv_process(&vad, silence, 256);
    TEST_ASSERT_FALSE(res);

    // 2. Test Voice
    res = eif_vad_adv_process(&vad, voice, 256);
    TEST_ASSERT_TRUE(res);

    // 3. Test Hangover (Feed silence after voice)
    res = eif_vad_adv_process(&vad, silence, 256);
    TEST_ASSERT_TRUE(res); // Should still be true due to hangover
    return true;
}

int run_audio_features_tests(void) { 
    int failed = 0;
    
    if (!test_ringbuffer_basic()) { printf("test_ringbuffer_basic FAILED\n"); failed++; }
    if (!test_ringbuffer_overwrite()) { printf("test_ringbuffer_overwrite FAILED\n"); failed++; }
    if (!test_adpcm_codec()) { printf("test_adpcm_codec FAILED\n"); failed++; }
    if (!test_vad_advanced()) { printf("test_vad_advanced FAILED\n"); failed++; }

    return failed;
}
