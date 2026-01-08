/**
 * @file main.c
 * @brief Matrix Profile Demo
 * 
 * Demonstrates Matrix Profile analysis for:
 * - Motif Discovery (repeating patterns)
 * - Discord Detection (anomalies)
 * - AB-Join (comparing two time series)
 * - Streaming analysis
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_matrix_profile.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Memory pool
static uint8_t pool_buffer[4 * 1024 * 1024];  // 4MB
static eif_memory_pool_t pool;

// ============================================================================
// Helper: Print time series segment
// ============================================================================
static void print_segment(const float32_t* ts, int start, int length) {
    printf("  [");
    for (int i = 0; i < length && i < 10; i++) {
        printf("%.2f%s", ts[start + i], i < length - 1 && i < 9 ? ", " : "");
    }
    if (length > 10) printf("...");
    printf("]\n");
}

// ============================================================================
// Demo 1: Motif Discovery
// ============================================================================
static void demo_motif_discovery(void) {
    printf("\n========================================\n");
    printf("Demo 1: MOTIF DISCOVERY\n");
    printf("========================================\n");
    printf("Finding repeating patterns in ECG-like signal\n\n");
    
    // Generate synthetic ECG-like signal with repeated heartbeat pattern
    const int TS_LEN = 200;
    const int WINDOW_SIZE = 20;
    float32_t ts[200];
    
    // Base: noisy signal
    for (int i = 0; i < TS_LEN; i++) {
        ts[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.3f;
    }
    
    // Insert heartbeat pattern at positions 30, 80, 130
    int pattern_positions[] = {30, 80, 130};
    for (int p = 0; p < 3; p++) {
        int pos = pattern_positions[p];
        for (int i = 0; i < WINDOW_SIZE; i++) {
            float32_t t = (float)i / WINDOW_SIZE;
            // Simulate QRS complex
            ts[pos + i] = sinf(2 * M_PI * t) * (1 - t) * 2.0f;
        }
    }
    
    printf("Time series length: %d, Window size: %d\n", TS_LEN, WINDOW_SIZE);
    printf("Pattern inserted at positions: 30, 80, 130\n\n");
    
    // Compute Matrix Profile
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    eif_matrix_profile_t mp;
    eif_mp_compute(ts, TS_LEN, WINDOW_SIZE, &mp, &pool);
    
    // Find top 3 motifs
    int motifs[3];
    float32_t distances[3];
    eif_mp_find_motifs(&mp, 3, motifs, distances, &pool);
    
    printf("Top 3 Motifs Found:\n");
    for (int i = 0; i < 3; i++) {
        printf("  Motif %d: position=%d, distance=%.4f\n", i+1, motifs[i], distances[i]);
        printf("  Pattern:");
        print_segment(ts, motifs[i], WINDOW_SIZE);
        printf("  Match at: position=%d\n\n", mp.profile_index[motifs[i]]);
    }
}

// ============================================================================
// Demo 2: Discord (Anomaly) Detection
// ============================================================================
static void demo_discord_detection(void) {
    printf("\n========================================\n");
    printf("Demo 2: DISCORD (ANOMALY) DETECTION\n");
    printf("========================================\n");
    printf("Finding anomalies in periodic sensor data\n\n");
    
    const int TS_LEN = 150;
    const int WINDOW_SIZE = 15;
    float32_t ts[150];
    
    // Normal periodic signal
    for (int i = 0; i < TS_LEN; i++) {
        ts[i] = sinf(2 * M_PI * i / 15.0f);
    }
    
    // Insert anomaly at position 60
    printf("Normal signal: sin wave with period 15\n");
    printf("Anomaly inserted at position 60: sharp spike\n\n");
    
    for (int i = 0; i < 10; i++) {
        ts[60 + i] = 3.0f * expf(-0.5f * i);  // Exponential spike
    }
    
    // Compute Matrix Profile
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    eif_matrix_profile_t mp;
    eif_mp_compute(ts, TS_LEN, WINDOW_SIZE, &mp, &pool);
    
    // Find top discord
    int discords[2];
    float32_t discord_dists[2];
    eif_mp_find_discords(&mp, 2, discords, discord_dists, &pool);
    
    printf("Discords (Anomalies) Found:\n");
    for (int i = 0; i < 2; i++) {
        printf("  Discord %d: position=%d, distance=%.4f\n", i+1, discords[i], discord_dists[i]);
        print_segment(ts, discords[i], WINDOW_SIZE);
    }
}

// ============================================================================
// Demo 3: AB-Join (Comparing Two Time Series)
// ============================================================================
static void demo_ab_join(void) {
    printf("\n========================================\n");
    printf("Demo 3: AB-JOIN (TIME SERIES COMPARISON)\n");
    printf("========================================\n");
    printf("Finding similar patterns between two sensors\n\n");
    
    const int LEN_A = 100;
    const int LEN_B = 120;
    const int WINDOW_SIZE = 12;
    float32_t ts_a[100], ts_b[120];
    
    // Sensor A: base signal
    for (int i = 0; i < LEN_A; i++) {
        ts_a[i] = sinf(2 * M_PI * i / 12.0f) + 0.1f * ((float)rand()/RAND_MAX - 0.5f);
    }
    
    // Sensor B: similar but with phase shift and different noise
    for (int i = 0; i < LEN_B; i++) {
        ts_b[i] = sinf(2 * M_PI * i / 12.0f + 0.5f) + 0.1f * ((float)rand()/RAND_MAX - 0.5f);
    }
    
    printf("Series A: sin wave, length=%d\n", LEN_A);
    printf("Series B: sin wave with phase shift, length=%d\n", LEN_B);
    printf("Window size: %d\n\n", WINDOW_SIZE);
    
    // Compute AB-Join
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    eif_matrix_profile_t mp;
    eif_mp_compute_ab(ts_a, LEN_A, ts_b, LEN_B, WINDOW_SIZE, &mp, &pool);
    
    // Find best matches
    printf("Best matches from A to B:\n");
    for (int i = 0; i < 5; i++) {
        int best_idx = -1;
        float32_t best_dist = 1e10f;
        for (int j = 0; j < mp.profile_length; j++) {
            if (mp.profile[j] < best_dist) {
                best_dist = mp.profile[j];
                best_idx = j;
            }
        }
        if (best_idx >= 0) {
            printf("  A[%d] matches B[%d] with distance %.4f\n", 
                   best_idx, mp.profile_index[best_idx], best_dist);
            mp.profile[best_idx] = 1e10f;  // Mark as used
        }
    }
}

// ============================================================================
// Demo 4: Streaming Analysis
// ============================================================================
static void demo_streaming(void) {
    printf("\n========================================\n");
    printf("Demo 4: STREAMING ANALYSIS\n");
    printf("========================================\n");
    printf("Real-time pattern monitoring\n\n");
    
    const int BUFFER_SIZE = 64;
    const int WINDOW_SIZE = 8;
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_mp_stream_t stream;
    eif_mp_stream_init(&stream, BUFFER_SIZE, WINDOW_SIZE, &pool);
    
    printf("Streaming data (simulating 50 samples)...\n");
    
    for (int i = 0; i < 50; i++) {
        float32_t value = sinf(2 * M_PI * i / 8.0f);
        
        // Add anomaly at sample 30
        if (i >= 30 && i < 35) {
            value = 5.0f;
        }
        
        eif_mp_stream_update(&stream, value, &pool);
        
        if (i == 25 || i == 40) {
            const eif_matrix_profile_t* mp = eif_mp_stream_get_profile(&stream);
            printf("\n  At sample %d:\n", i);
            printf("    Profile length: %d\n", mp->profile_length);
            
            // Check for discord
            int discord;
            float32_t dist;
            eif_mp_find_discords(mp, 1, &discord, &dist, &pool);
            printf("    Current discord at: %d (dist=%.2f)\n", discord, dist);
        }
    }
    
    printf("\nStreaming analysis complete.\n");
}

// ============================================================================
// Main
// ============================================================================
int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║     EIF Matrix Profile Demo              ║\n");
    printf("║     Time Series Pattern Analysis         ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    
    srand(42);  // Reproducible results
    
    demo_motif_discovery();
    demo_discord_detection();
    demo_ab_join();
    demo_streaming();
    
    printf("\n========================================\n");
    printf("Demo Complete!\n");
    printf("========================================\n");
    
    return 0;
}
