/**
 * @file main.c
 * @brief Online Learning Demo - Streaming Data with Concept Drift Detection
 * 
 * This demo shows online learning adapting to streaming sensor data:
 * - Phase 1: Process stable sensor readings (low error)
 * - Phase 2: Detect concept drift when data distribution changes
 * - Phase 3: React and adapt model to new distribution
 * - Phase 4: Return to stability with updated model
 * 
 * Scenario: Industrial sensor monitoring with sudden environment changes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "eif_el.h"
#include "eif_memory.h"

// =============================================================================
// Configuration
// =============================================================================

#define NUM_WEIGHTS     5       // Feature dimension
#define STREAM_LENGTH   100     // Total streaming samples
#define DRIFT_WINDOW    15      // Window for drift detection
#define DRIFT_THRESHOLD 0.4f    // Error threshold for drift

// Memory pool
static uint8_t pool_buffer[64 * 1024];
static eif_memory_pool_t pool;

// =============================================================================
// Data Stream Simulation
// =============================================================================

typedef enum {
    PHASE_STABLE_1,     // Stable initial distribution
    PHASE_DRIFT,        // Distribution shift (concept drift)
    PHASE_STABLE_2      // New stable distribution
} stream_phase_t;

typedef struct {
    float32_t input[NUM_WEIGHTS];
    float32_t target;
    stream_phase_t phase;
} stream_sample_t;

static stream_sample_t data_stream[STREAM_LENGTH];

// True model parameters (hidden from learner)
static float32_t true_weights_1[NUM_WEIGHTS] = {0.5f, 0.3f, 0.2f, 0.1f, 0.4f};
static float32_t true_weights_2[NUM_WEIGHTS] = {-0.3f, 0.6f, -0.1f, 0.5f, -0.2f};

void generate_stream_data(void) {
    for (int i = 0; i < STREAM_LENGTH; i++) {
        // Generate random input
        for (int f = 0; f < NUM_WEIGHTS; f++) {
            data_stream[i].input[f] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        }
        
        // Phase-dependent target generation
        float32_t* weights;
        float noise = (float)rand() / RAND_MAX * 0.2f - 0.1f;
        
        if (i < 35) {
            data_stream[i].phase = PHASE_STABLE_1;
            weights = true_weights_1;
        } else if (i < 65) {
            data_stream[i].phase = PHASE_DRIFT;
            // Gradual transition
            float alpha = (float)(i - 35) / 30.0f;
            float32_t target = 0.0f;
            for (int f = 0; f < NUM_WEIGHTS; f++) {
                float w = (1.0f - alpha) * true_weights_1[f] + alpha * true_weights_2[f];
                target += w * data_stream[i].input[f];
            }
            data_stream[i].target = target + noise;
            continue;
        } else {
            data_stream[i].phase = PHASE_STABLE_2;
            weights = true_weights_2;
        }
        
        // Compute target
        data_stream[i].target = noise;
        for (int f = 0; f < NUM_WEIGHTS; f++) {
            data_stream[i].target += weights[f] * data_stream[i].input[f];
        }
    }
}

// =============================================================================
// Visualization
// =============================================================================

void print_error_graph(const float32_t* errors, int len, 
                        const stream_phase_t* phases, float threshold) {
    printf("\n  Error Rate │\n");
    
    // Find max for scaling
    float max_err = threshold;
    for (int i = 0; i < len; i++) {
        if (errors[i] > max_err) max_err = errors[i];
    }
    
    // Print graph rows
    int rows = 8;
    for (int r = rows - 1; r >= 0; r--) {
        float level = max_err * r / (rows - 1);
        printf("      %5.2f │", level);
        
        for (int i = 0; i < len; i += 2) {
            if (errors[i] >= level) {
                switch (phases[i]) {
                    case PHASE_STABLE_1: printf("\033[32m█\033[0m"); break;  // Green
                    case PHASE_DRIFT:    printf("\033[31m█\033[0m"); break;  // Red
                    case PHASE_STABLE_2: printf("\033[34m█\033[0m"); break;  // Blue
                }
            } else {
                printf(" ");
            }
        }
        
        // Draw threshold line
        if (fabsf(level - threshold) < max_err / rows) {
            printf(" ← Drift Threshold");
        }
        printf("\n");
    }
    
    // X axis
    printf("            └");
    for (int i = 0; i < len / 2; i++) printf("─");
    printf("→ Time\n");
    
    // Legend
    printf("            Legend: \033[32m█\033[0m Stable-1  \033[31m█\033[0m Drift  \033[34m█\033[0m Stable-2\n");
}

void print_weights_comparison(const float32_t* learned, const float32_t* true_w, int len) {
    printf("\n  Weight Comparison:\n");
    printf("  ┌────────┬──────────┬──────────┬────────┐\n");
    printf("  │ Weight │  Learned │   True   │ Error  │\n");
    printf("  ├────────┼──────────┼──────────┼────────┤\n");
    
    for (int i = 0; i < len; i++) {
        float err = fabsf(learned[i] - true_w[i]);
        printf("  │  w[%d]  │  %+6.3f  │  %+6.3f  │ %5.3f  │\n",
               i, learned[i], true_w[i], err);
    }
    printf("  └────────┴──────────┴──────────┴────────┘\n");
}

// =============================================================================
// Main
// =============================================================================

int main(void) {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║      Online Learning Demo - Concept Drift Detection           ║\n");
    printf("║      Adaptive Learning for Streaming Sensor Data              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    srand(42);
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Generate streaming data
    printf("📊 Generating sensor data stream with concept drift...\n\n");
    generate_stream_data();
    
    // Stream summary
    printf("  Stream Configuration:\n");
    printf("  • Samples 0-34:   Stable distribution (weights: [+0.5, +0.3, ...])\n");
    printf("  • Samples 35-64:  Gradual drift (distribution changing)\n");
    printf("  • Samples 65-99:  New stable distribution (weights: [-0.3, +0.6, ...])\n\n");
    
    // Initialize online learner
    eif_online_learner_t learner;
    eif_online_init(&learner, NUM_WEIGHTS, 0.5f, DRIFT_WINDOW, &pool);
    learner.error_threshold = DRIFT_THRESHOLD;
    
    // Initial weights (zeros)
    float32_t initial[NUM_WEIGHTS] = {0};
    eif_online_set_weights(&learner, initial);
    
    // State tracking
    float32_t error_history[STREAM_LENGTH] = {0};
    int drift_detected_at = -1;
    int num_resets = 0;
    
    printf("🚀 Starting Online Learning Stream\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    // Process stream
    for (int t = 0; t < STREAM_LENGTH; t++) {
        stream_sample_t* sample = &data_stream[t];
        
        // Predict
        float32_t pred = eif_online_predict(&learner, sample->input, NUM_WEIGHTS);
        
        // Compute error
        float32_t error = sample->target - pred;
        float32_t loss = error * error;
        
        // Compute gradient
        float32_t gradient[NUM_WEIGHTS];
        for (int f = 0; f < NUM_WEIGHTS; f++) {
            gradient[f] = -2.0f * error * sample->input[f];
        }
        
        // Update model
        eif_online_update(&learner, gradient, loss);
        
        // Track error
        error_history[t] = eif_online_error_rate(&learner);
        
        // Check for drift
        if (eif_online_drift_detected(&learner) && drift_detected_at < 0) {
            drift_detected_at = t;
            printf("  ⚠️  Drift detected at sample %d! Error rate: %.2f\n", 
                   t, error_history[t]);
            printf("      → Triggering model adaptation...\n\n");
            eif_online_reset_drift(&learner);
            num_resets++;
        }
        
        // Progress
        if ((t + 1) % 25 == 0) {
            const char* phase_str = (sample->phase == PHASE_STABLE_1) ? "Stable-1" :
                                    (sample->phase == PHASE_DRIFT) ? "Drift" : "Stable-2";
            printf("  Sample %3d: Phase=%-8s Error=%.3f Drift=%s\n",
                   t + 1, phase_str, error_history[t],
                   eif_online_drift_detected(&learner) ? "YES" : "NO");
        }
    }
    
    printf("\n");
    
    // Visualize error history
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Error Rate Over Time (Concept Drift Visualization)\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    stream_phase_t phases[STREAM_LENGTH];
    for (int i = 0; i < STREAM_LENGTH; i++) {
        phases[i] = data_stream[i].phase;
    }
    print_error_graph(error_history, STREAM_LENGTH, phases, DRIFT_THRESHOLD);
    
    printf("\n");
    
    // Final weights analysis
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Learned vs True Weights (After Adaptation)\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    print_weights_comparison(learner.weights, true_weights_2, NUM_WEIGHTS);
    
    // Summary
    printf("\n📊 Summary:\n");
    printf("   • Total samples processed: %d\n", STREAM_LENGTH);
    printf("   • Drift detected at sample: %d\n", drift_detected_at);
    printf("   • Model resets triggered: %d\n", num_resets);
    printf("   • Final error rate: %.3f\n", error_history[STREAM_LENGTH - 1]);
    printf("   • Adaptation successful: %s\n",
           error_history[STREAM_LENGTH - 1] < DRIFT_THRESHOLD ? "YES ✓" : "Ongoing");
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("Online Learning Demo Complete!\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    return 0;
}
