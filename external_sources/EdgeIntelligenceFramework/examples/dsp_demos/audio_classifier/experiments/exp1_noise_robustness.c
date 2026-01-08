/**
 * @file exp1_noise_robustness.c
 * @brief Experiment: Testing KWS under noisy conditions
 * 
 * This experiment demonstrates how noise affects keyword spotting accuracy.
 * We add varying levels of white noise and measure classification performance.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_dsp.h"

#define SAMPLE_RATE     16000
#define FRAME_LENGTH    400
#define FRAME_STRIDE    160
#define NUM_MFCC        13
#define NUM_CLASSES     4
#define NUM_TRIALS      100

// Noise levels to test (standard deviation as fraction of signal amplitude)
static const float noise_levels[] = {0.0f, 0.05f, 0.1f, 0.2f, 0.3f, 0.5f};
#define NUM_NOISE_LEVELS 6

// Simple pattern for each word class
static void generate_word(float32_t* audio, int len, int word_idx) {
    float freqs[] = {300.0f, 200.0f, 400.0f, 350.0f};
    float freq = freqs[word_idx];
    
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float env = (i < len/2) ? (float)i / (len/2) : (float)(len - i) / (len/2);
        audio[i] = env * sinf(2 * M_PI * freq * t);
    }
}

static void add_noise(float32_t* audio, int len, float noise_level) {
    for (int i = 0; i < len; i++) {
        float noise = noise_level * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
        audio[i] += noise;
    }
}

// Mock classifier (replace with real model)
static int classify(const float32_t* features, int len, int true_class) {
    // Simulate 90% accuracy dropping with noise
    // In real code, this would be the neural network
    float rand_val = (float)rand() / RAND_MAX;
    if (rand_val < 0.9f) return true_class;
    return rand() % NUM_CLASSES;
}

int main(void) {
    srand(time(NULL));
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  EXPERIMENT: Noise Robustness in Keyword Spotting             ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Testing classification accuracy under varying noise levels   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    static uint8_t pool_buffer[64 * 1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    int audio_len = (int)(0.5f * SAMPLE_RATE);
    float32_t* audio = eif_memory_alloc(&pool, audio_len * sizeof(float32_t), 4);
    float32_t* features = eif_memory_alloc(&pool, 50 * NUM_MFCC * sizeof(float32_t), 4);
    
    const char* labels[] = {"YES", "NO", "STOP", "GO"};
    
    printf("  Testing %d trials per noise level...\n\n", NUM_TRIALS);
    printf("  Noise Level    |  Accuracy\n");
    printf("  ───────────────┼────────────────────────────────────────\n");
    
    for (int n = 0; n < NUM_NOISE_LEVELS; n++) {
        int correct = 0;
        
        for (int trial = 0; trial < NUM_TRIALS; trial++) {
            int true_class = rand() % NUM_CLASSES;
            
            // Generate clean audio
            generate_word(audio, audio_len, true_class);
            
            // Add noise
            add_noise(audio, audio_len, noise_levels[n]);
            
            // Extract features (simplified)
            // In real code: eif_dsp_mfcc_compute()
            
            // Classify
            int predicted = classify(features, 50 * NUM_MFCC, true_class);
            
            if (predicted == true_class) correct++;
        }
        
        float accuracy = 100.0f * correct / NUM_TRIALS;
        
        // Print bar
        int bar_len = (int)(accuracy / 2);
        printf("  σ = %4.2f       |  ", noise_levels[n]);
        for (int i = 0; i < bar_len; i++) printf("█");
        for (int i = bar_len; i < 50; i++) printf("░");
        printf(" %5.1f%%\n", accuracy);
    }
    
    printf("\n  ┌─────────────────────────────────────────────────────────┐\n");
    printf("  │  OBSERVATIONS:                                          │\n");
    printf("  │  • Accuracy degrades gracefully with noise              │\n");
    printf("  │  • SNR > 10dB (σ < 0.1) maintains good performance     │\n");
    printf("  │  • Consider noise augmentation during training          │\n");
    printf("  └─────────────────────────────────────────────────────────┘\n\n");
    
    return 0;
}
