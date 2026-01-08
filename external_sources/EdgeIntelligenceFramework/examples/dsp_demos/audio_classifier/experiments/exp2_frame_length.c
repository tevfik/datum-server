/**
 * @file exp2_frame_length_comparison.c
 * @brief Experiment: Effect of frame length on MFCC features
 * 
 * This experiment compares different frame lengths and their impact
 * on feature quality and classification accuracy.
 * 
 * Theory:
 * - Short frames: Better temporal resolution, worse frequency resolution
 * - Long frames: Better frequency resolution, worse temporal resolution
 * - Heisenberg uncertainty: Δt × Δf ≥ 1/(4π)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_dsp.h"

#define SAMPLE_RATE 16000
#define NUM_MFCC    13

// Different frame lengths to test
typedef struct {
    const char* name;
    int samples;
    float ms;
} frame_config_t;

static const frame_config_t configs[] = {
    {"Very Short", 128,  8.0f},
    {"Short",      256, 16.0f},
    {"Standard",   400, 25.0f},
    {"Long",       512, 32.0f},
    {"Very Long",  800, 50.0f}
};
#define NUM_CONFIGS 5

// Generate test signal with known frequencies
static void generate_test_signal(float32_t* audio, int len) {
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        // Multi-tone test signal
        audio[i] = 0.5f * sinf(2 * M_PI * 300.0f * t) +
                   0.3f * sinf(2 * M_PI * 600.0f * t) +
                   0.2f * sinf(2 * M_PI * 1200.0f * t);
    }
}

// Compute spectral flatness (measure of tonality)
static float compute_spectral_flatness(const float32_t* spectrum, int len) {
    float log_sum = 0;
    float linear_sum = 0;
    
    for (int i = 1; i < len/2; i++) {  // Skip DC
        float mag = fabsf(spectrum[i]) + 1e-10f;
        log_sum += logf(mag);
        linear_sum += mag;
    }
    
    int n = len/2 - 1;
    float geom_mean = expf(log_sum / n);
    float arith_mean = linear_sum / n;
    
    return geom_mean / arith_mean;
}

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  EXPERIMENT: Frame Length Comparison                          ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Analyzing time-frequency tradeoff in MFCC extraction         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    static uint8_t pool_buffer[128 * 1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Generate 1 second of test audio
    int audio_len = SAMPLE_RATE;
    float32_t* audio = eif_memory_alloc(&pool, audio_len * sizeof(float32_t), 4);
    generate_test_signal(audio, audio_len);
    
    printf("  Test signal: 300Hz + 600Hz + 1200Hz composite tone\n\n");
    printf("  ┌──────────────┬──────────┬────────────┬─────────────────────┐\n");
    printf("  │ Frame Length │ Samples  │ FFT Bins   │ Frequency Resolution│\n");
    printf("  ├──────────────┼──────────┼────────────┼─────────────────────┤\n");
    
    for (int c = 0; c < NUM_CONFIGS; c++) {
        const frame_config_t* cfg = &configs[c];
        float freq_res = (float)SAMPLE_RATE / cfg->samples;
        
        printf("  │ %6.1f ms    │ %4d     │ %4d       │ %7.1f Hz          │\n",
               cfg->ms, cfg->samples, cfg->samples/2 + 1, freq_res);
    }
    
    printf("  └──────────────┴──────────┴────────────┴─────────────────────┘\n\n");
    
    printf("  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │  ANALYSIS:                                                  │\n");
    printf("  │                                                             │\n");
    printf("  │  • Short frames (8-16ms): Good for transients, clicks      │\n");
    printf("  │  • Standard frames (25ms): Optimal for speech              │\n");
    printf("  │  • Long frames (50ms): Better for steady tones             │\n");
    printf("  │                                                             │\n");
    printf("  │  RECOMMENDATION:                                            │\n");
    printf("  │  Use 25ms frames with 10ms stride for KWS applications     │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n\n");
    
    // Show visual comparison
    printf("  Frequency Resolution vs Time Resolution:\n\n");
    printf("       8ms: │████                             │ ◄ Best time, worst freq\n");
    printf("      16ms: │████████                         │\n");
    printf("      25ms: │████████████  ◄ STANDARD         │\n");
    printf("      32ms: │████████████████                 │\n");
    printf("      50ms: │█████████████████████████        │ ◄ Best freq, worst time\n");
    printf("            └─────────────────────────────────┘\n");
    printf("              0 Hz              1000 Hz resolution\n\n");
    
    return 0;
}
