/**
 * @file test_matrix_profile.c
 * @brief Unit tests for Matrix Profile
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_matrix_profile.h"

static uint8_t pool_buffer[4 * 1024 * 1024];  // 4MB for FFT
static eif_memory_pool_t pool;
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("Running %s... ", #name); \
    tests_run++; \
    if (name()) { tests_passed++; printf("PASS\n"); } \
    else { printf("FAIL\n"); } \
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer)); \
} while(0)

// ============================================================================
// MASS Algorithm Tests
// ============================================================================

static bool test_mass_basic(void) {
    // Simple synthetic time series
    float32_t ts[32];
    for (int i = 0; i < 32; i++) {
        ts[i] = sinf(2.0f * 3.14159f * i / 8.0f);
    }
    
    float32_t query[8];
    for (int i = 0; i < 8; i++) {
        query[i] = ts[i];  // Query is exact match at position 0
    }
    
    float32_t distance_profile[25];  // 32 - 8 + 1
    
    eif_status_t status = eif_mass_compute(ts, 32, query, 8, distance_profile, &pool);
    if (status != EIF_STATUS_OK) return false;
    
    // Distance at position 0 should be ~0 (exact match)
    if (distance_profile[0] > 0.1f) return false;
    
    // Distance at position 8 should also be near 0 (same phase)
    if (distance_profile[8] > 0.5f) return false;
    
    return true;
}

// ============================================================================
// Matrix Profile Self-Join Tests
// ============================================================================

static bool test_mp_init(void) {
    eif_matrix_profile_t mp;
    eif_status_t status = eif_mp_init(&mp, 100, 10, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (mp.profile_length != 91) return false;  // 100 - 10 + 1
    if (mp.window_size != 10) return false;
    if (mp.profile == NULL) return false;
    
    return true;
}

static bool test_mp_self_join(void) {
    // Time series with repeating pattern
    float32_t ts[64];
    for (int i = 0; i < 64; i++) {
        ts[i] = sinf(2.0f * 3.14159f * i / 16.0f);
    }
    
    eif_matrix_profile_t mp;
    eif_status_t status = eif_mp_compute(ts, 64, 8, &mp, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (mp.profile_length != 57) return false;  // 64 - 8 + 1
    
    // All profile values should be finite
    for (int i = 0; i < mp.profile_length; i++) {
        if (mp.profile[i] < 0 || mp.profile[i] > 100) {
            if (mp.profile[i] != __FLT_MAX__) return false;
        }
    }
    
    return true;
}

// ============================================================================
// AB-Join Tests
// ============================================================================

static bool test_mp_ab_join(void) {
    // Two similar time series
    float32_t ts_a[32], ts_b[32];
    for (int i = 0; i < 32; i++) {
        ts_a[i] = sinf(2.0f * 3.14159f * i / 8.0f);
        ts_b[i] = sinf(2.0f * 3.14159f * i / 8.0f + 0.1f);  // Slightly shifted
    }
    
    eif_matrix_profile_t mp;
    eif_status_t status = eif_mp_compute_ab(ts_a, 32, ts_b, 32, 8, &mp, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (mp.profile_length != 25) return false;
    
    // Distances should be small (similar series)
    for (int i = 0; i < mp.profile_length; i++) {
        if (mp.profile[i] > 5.0f) return false;
    }
    
    return true;
}

// ============================================================================
// Motif and Discord Discovery Tests
// ============================================================================

static bool test_motif_discovery(void) {
    // Time series with clear motif (repeated pattern)
    float32_t ts[80];
    for (int i = 0; i < 80; i++) {
        ts[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;  // Noise
    }
    // Insert identical pattern at positions 10 and 50
    for (int i = 0; i < 10; i++) {
        float32_t pattern = sinf(2.0f * 3.14159f * i / 10.0f);
        ts[10 + i] = pattern;
        ts[50 + i] = pattern;
    }
    
    eif_matrix_profile_t mp;
    eif_mp_compute(ts, 80, 10, &mp, &pool);
    
    int motifs[2];
    float32_t distances[2];
    eif_status_t status = eif_mp_find_motifs(&mp, 2, motifs, distances, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    
    // Motif should be near position 10 or 50
    bool found = (abs(motifs[0] - 10) < 5 || abs(motifs[0] - 50) < 5);
    return found;
}

static bool test_discord_discovery(void) {
    // Time series with clear anomaly
    float32_t ts[64];
    for (int i = 0; i < 64; i++) {
        ts[i] = sinf(2.0f * 3.14159f * i / 8.0f);
    }
    // Insert anomaly at position 32
    for (int i = 0; i < 8; i++) {
        ts[32 + i] = 5.0f;  // Spike
    }
    
    eif_matrix_profile_t mp;
    eif_mp_compute(ts, 64, 8, &mp, &pool);
    
    int discords[1];
    eif_status_t status = eif_mp_find_discords(&mp, 1, discords, NULL, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    
    // Discord should be near the anomaly
    return abs(discords[0] - 32) < 5;
}

// ============================================================================
// Streaming Tests
// ============================================================================

static bool test_streaming_init(void) {
    eif_mp_stream_t stream;
    eif_status_t status = eif_mp_stream_init(&stream, 100, 10, &pool);
    
    if (status != EIF_STATUS_OK) return false;
    if (stream.window_size != 10) return false;
    if (stream.buffer_size != 100) return false;
    
    return true;
}

static bool test_streaming_update(void) {
    eif_mp_stream_t stream;
    eif_mp_stream_init(&stream, 64, 8, &pool);
    
    // Feed in data points
    for (int i = 0; i < 40; i++) {
        float32_t value = sinf(2.0f * 3.14159f * i / 8.0f);
        eif_status_t status = eif_mp_stream_update(&stream, value, &pool);
        if (status != EIF_STATUS_OK) return false;
    }
    
    const eif_matrix_profile_t* mp = eif_mp_stream_get_profile(&stream);
    if (mp == NULL) return false;
    
    return true;
}

// ============================================================================
// Main
// ============================================================================

int run_matrix_profile_tests(void) {
    printf("=== Matrix Profile Tests ===\n");
    
    srand(42);
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // MASS
    TEST(test_mass_basic);
    
    // Matrix Profile
    TEST(test_mp_init);
    TEST(test_mp_self_join);
    TEST(test_mp_ab_join);
    
    // Discovery
    TEST(test_motif_discovery);
    TEST(test_discord_discovery);
    
    // Streaming
    TEST(test_streaming_init);
    TEST(test_streaming_update);
    
    printf("Results: %d Run, %d Passed, %d Failed\n", 
           tests_run, tests_passed, tests_run - tests_passed);
    
    return tests_run - tests_passed;
}
