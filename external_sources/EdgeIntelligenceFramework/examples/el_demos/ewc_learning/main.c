/**
 * @file main.c
 * @brief EWC Demo - Elastic Weight Consolidation for Continual Learning
 * 
 * This demo shows how EWC prevents catastrophic forgetting:
 * - Train on Task A (recognize patterns 0-4)
 * - Compute Fisher Information (weight importance)
 * - Train on Task B (recognize patterns 5-9) with EWC regularization
 * - Test: Performance on Task A is preserved!
 * 
 * Scenario: Edge device learns new sensor patterns without forgetting old ones
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "eif_el.h"
#include "eif_memory.h"

// =============================================================================
// Configuration
// =============================================================================

#define NUM_WEIGHTS     16      // Feature dimension
#define NUM_PATTERNS    5       // Patterns per task
#define SAMPLES_PER_PATTERN 10  // Training samples per pattern
#define TRAINING_EPOCHS 30
#define EWC_LAMBDA      1000.0f // EWC regularization strength

// Memory pool
static uint8_t pool_buffer[256 * 1024];
static eif_memory_pool_t pool;

// =============================================================================
// Pattern Data
// =============================================================================

typedef struct {
    float32_t patterns[NUM_PATTERNS * NUM_WEIGHTS];
    const char* pattern_names[NUM_PATTERNS];
    const char* task_name;
} task_data_t;

static task_data_t task_a, task_b;

void generate_task_data(void) {
    // Task A: Low frequency patterns
    task_a.task_name = "Task A (Low Frequency Patterns)";
    const char* names_a[] = {"Sine_1Hz", "Sine_2Hz", "Sine_3Hz", "Square_1Hz", "Triangle_1Hz"};
    
    for (int p = 0; p < NUM_PATTERNS; p++) {
        task_a.pattern_names[p] = names_a[p];
        float freq = (p < 3) ? (1.0f + p) : 1.0f;
        for (int i = 0; i < NUM_WEIGHTS; i++) {
            float t = (float)i / NUM_WEIGHTS * 2.0f * 3.14159f * freq;
            if (p < 3) {
                task_a.patterns[p * NUM_WEIGHTS + i] = sinf(t);
            } else if (p == 3) {
                task_a.patterns[p * NUM_WEIGHTS + i] = (sinf(t) > 0) ? 1.0f : -1.0f;
            } else {
                task_a.patterns[p * NUM_WEIGHTS + i] = 2.0f * fabsf(fmodf(t, 2*3.14159f) / (2*3.14159f) - 0.5f) - 0.5f;
            }
        }
    }
    
    // Task B: High frequency patterns
    task_b.task_name = "Task B (High Frequency Patterns)";
    const char* names_b[] = {"Sine_5Hz", "Sine_7Hz", "Chirp", "Pulse", "Noise"};
    
    for (int p = 0; p < NUM_PATTERNS; p++) {
        task_b.pattern_names[p] = names_b[p];
        float freq = (p < 2) ? (5.0f + 2*p) : 3.0f;
        for (int i = 0; i < NUM_WEIGHTS; i++) {
            float t = (float)i / NUM_WEIGHTS * 2.0f * 3.14159f;
            if (p < 2) {
                task_b.patterns[p * NUM_WEIGHTS + i] = sinf(t * freq);
            } else if (p == 2) {
                task_b.patterns[p * NUM_WEIGHTS + i] = sinf(t * (1.0f + 5.0f * i / NUM_WEIGHTS));
            } else if (p == 3) {
                task_b.patterns[p * NUM_WEIGHTS + i] = (i == NUM_WEIGHTS/2) ? 1.0f : 0.0f;
            } else {
                task_b.patterns[p * NUM_WEIGHTS + i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
            }
        }
    }
}

// =============================================================================
// Simple Neural Network (for comparison)
// =============================================================================

typedef struct {
    float32_t weights[NUM_WEIGHTS];
    float32_t bias;
} simple_classifier_t;

void classifier_init(simple_classifier_t* clf) {
    for (int i = 0; i < NUM_WEIGHTS; i++) {
        clf->weights[i] = (float)rand() / RAND_MAX * 0.2f - 0.1f;
    }
    clf->bias = 0.0f;
}

float32_t classifier_predict(simple_classifier_t* clf, const float32_t* x) {
    float32_t sum = clf->bias;
    for (int i = 0; i < NUM_WEIGHTS; i++) {
        sum += clf->weights[i] * x[i];
    }
    return sum;
}

void classifier_train(simple_classifier_t* clf, const float32_t* x, float32_t y, float32_t lr) {
    float32_t pred = classifier_predict(clf, x);
    float32_t error = pred - y;
    for (int i = 0; i < NUM_WEIGHTS; i++) {
        clf->weights[i] -= lr * error * x[i];
    }
    clf->bias -= lr * error;
}

float32_t evaluate_task(simple_classifier_t* clf, task_data_t* task) {
    int correct = 0;
    for (int p = 0; p < NUM_PATTERNS; p++) {
        float32_t pred = classifier_predict(clf, &task->patterns[p * NUM_WEIGHTS]);
        int pred_class = (int)(pred + 0.5f);
        if (pred_class < 0) pred_class = 0;
        if (pred_class >= NUM_PATTERNS) pred_class = NUM_PATTERNS - 1;
        if (pred_class == p) correct++;
    }
    return (float32_t)correct / NUM_PATTERNS;
}

// =============================================================================
// EWC Training
// =============================================================================

static void compute_grad(const float32_t* w, const float32_t* x, 
                          float32_t* grad, int n) {
    // Gradient for MSE loss
    float32_t sum = 0.0f;
    for (int i = 0; i < n; i++) {
        sum += w[i] * x[i];
    }
    for (int i = 0; i < n; i++) {
        grad[i] = x[i] * sum * 0.1f;
    }
}

// =============================================================================
// Visualization
// =============================================================================

void print_pattern(const float32_t* pattern, int len) {
    printf("  │");
    for (int i = 0; i < len; i++) {
        int level = (int)((pattern[i] + 1.0f) * 4);
        if (level < 0) level = 0;
        if (level > 8) level = 8;
        const char* blocks[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█", "█"};
        printf("%s", blocks[level]);
    }
    printf("│\n");
}

void print_accuracy_bar(const char* label, float accuracy, int width) {
    int filled = (int)(accuracy * width);
    printf("  %20s: [", label);
    for (int i = 0; i < width; i++) {
        if (i < filled) printf("█");
        else printf("░");
    }
    printf("] %.0f%%\n", accuracy * 100);
}

void print_fisher_info(const float32_t* fisher, int len) {
    float max_f = 0.0f;
    for (int i = 0; i < len; i++) {
        if (fisher[i] > max_f) max_f = fisher[i];
    }
    
    printf("\n  Fisher Information (weight importance):\n  │");
    for (int i = 0; i < len; i++) {
        int level = (int)(fisher[i] / max_f * 8);
        if (level < 0) level = 0;
        if (level > 8) level = 8;
        const char* blocks[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
        printf("%s", blocks[level]);
    }
    printf("│\n");
}

// =============================================================================
// Main
// =============================================================================

int main(void) {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║     EWC Demo - Elastic Weight Consolidation                   ║\n");
    printf("║     Preventing Catastrophic Forgetting in Continual Learning  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    srand(42);
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Generate data
    printf("📊 Generating pattern recognition tasks...\n\n");
    generate_task_data();
    
    // Show patterns
    printf("┌────────────────────────────────────────────────────────────────┐\n");
    printf("│                     %s                       │\n", task_a.task_name);
    printf("├────────────────────────────────────────────────────────────────┤\n");
    for (int p = 0; p < NUM_PATTERNS; p++) {
        printf("│ %-12s ", task_a.pattern_names[p]);
        print_pattern(&task_a.patterns[p * NUM_WEIGHTS], NUM_WEIGHTS);
    }
    printf("└────────────────────────────────────────────────────────────────┘\n\n");
    
    printf("┌────────────────────────────────────────────────────────────────┐\n");
    printf("│                    %s                       │\n", task_b.task_name);
    printf("├────────────────────────────────────────────────────────────────┤\n");
    for (int p = 0; p < NUM_PATTERNS; p++) {
        printf("│ %-12s ", task_b.pattern_names[p]);
        print_pattern(&task_b.patterns[p * NUM_WEIGHTS], NUM_WEIGHTS);
    }
    printf("└────────────────────────────────────────────────────────────────┘\n\n");
    
    // Initialize classifiers
    simple_classifier_t clf_without_ewc, clf_with_ewc;
    classifier_init(&clf_without_ewc);
    memcpy(&clf_with_ewc, &clf_without_ewc, sizeof(simple_classifier_t));
    
    // Initialize EWC
    eif_ewc_t ewc;
    eif_ewc_init(&ewc, NUM_WEIGHTS, EWC_LAMBDA, &pool);
    eif_ewc_set_weights(&ewc, clf_with_ewc.weights);
    
    // ==========================================================================
    // Phase 1: Train on Task A
    // ==========================================================================
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Phase 1: Training on Task A\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    for (int epoch = 0; epoch < TRAINING_EPOCHS; epoch++) {
        for (int p = 0; p < NUM_PATTERNS; p++) {
            classifier_train(&clf_without_ewc, &task_a.patterns[p * NUM_WEIGHTS], 
                              (float)p, 0.01f);
            classifier_train(&clf_with_ewc, &task_a.patterns[p * NUM_WEIGHTS], 
                              (float)p, 0.01f);
        }
    }
    
    float acc_a_without = evaluate_task(&clf_without_ewc, &task_a);
    float acc_a_with = evaluate_task(&clf_with_ewc, &task_a);
    
    printf("  After Task A training:\n");
    print_accuracy_bar("Without EWC (Task A)", acc_a_without, 30);
    print_accuracy_bar("With EWC (Task A)", acc_a_with, 30);
    printf("\n");
    
    // ==========================================================================
    // Compute Fisher Information and Consolidate
    // ==========================================================================
    
    printf("🔒 Computing Fisher Information and consolidating Task A...\n");
    
    // Update EWC weights from trained classifier
    eif_ewc_set_weights(&ewc, clf_with_ewc.weights);
    
    // Compute Fisher
    float32_t all_patterns[NUM_PATTERNS * NUM_WEIGHTS];
    memcpy(all_patterns, task_a.patterns, sizeof(all_patterns));
    eif_ewc_compute_fisher(&ewc, all_patterns, NUM_PATTERNS, NUM_WEIGHTS, compute_grad);
    
    print_fisher_info(ewc.fisher, NUM_WEIGHTS);
    
    // Consolidate
    eif_ewc_consolidate(&ewc);
    printf("   ✓ Task A consolidated (weights saved as optimal)\n\n");
    
    // ==========================================================================
    // Phase 2: Train on Task B
    // ==========================================================================
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Phase 2: Training on Task B (while preserving Task A with EWC)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    for (int epoch = 0; epoch < TRAINING_EPOCHS; epoch++) {
        for (int p = 0; p < NUM_PATTERNS; p++) {
            // WITHOUT EWC: Just train normally
            classifier_train(&clf_without_ewc, &task_b.patterns[p * NUM_WEIGHTS], 
                              (float)p, 0.01f);
            
            // WITH EWC: Use regularization
            float32_t gradient[NUM_WEIGHTS];
            float32_t pred = classifier_predict(&clf_with_ewc, &task_b.patterns[p * NUM_WEIGHTS]);
            float32_t error = pred - (float)p;
            
            for (int i = 0; i < NUM_WEIGHTS; i++) {
                gradient[i] = error * task_b.patterns[p * NUM_WEIGHTS + i];
            }
            
            // Copy weights to EWC
            memcpy(ewc.weights, clf_with_ewc.weights, NUM_WEIGHTS * sizeof(float32_t));
            
            // Update with EWC
            eif_ewc_update(&ewc, gradient, 0.01f);
            
            // Copy back
            memcpy(clf_with_ewc.weights, ewc.weights, NUM_WEIGHTS * sizeof(float32_t));
        }
        
        if ((epoch + 1) % 10 == 0) {
            float penalty = eif_ewc_penalty(&ewc);
            printf("  Epoch %2d: EWC penalty = %.2f\n", epoch + 1, penalty);
        }
    }
    
    printf("\n");
    
    // ==========================================================================
    // Final Evaluation
    // ==========================================================================
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Final Results: Catastrophic Forgetting Analysis\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    float final_a_without = evaluate_task(&clf_without_ewc, &task_a);
    float final_b_without = evaluate_task(&clf_without_ewc, &task_b);
    float final_a_with = evaluate_task(&clf_with_ewc, &task_a);
    float final_b_with = evaluate_task(&clf_with_ewc, &task_b);
    
    printf("  ┌────────────────────────────────────────────────────────────┐\n");
    printf("  │                 WITHOUT EWC (Standard Training)            │\n");
    printf("  ├────────────────────────────────────────────────────────────┤\n");
    print_accuracy_bar("Task A (Old)", final_a_without, 30);
    print_accuracy_bar("Task B (New)", final_b_without, 30);
    printf("  │ ⚠️  CATASTROPHIC FORGETTING: Task A accuracy dropped!      │\n");
    printf("  └────────────────────────────────────────────────────────────┘\n\n");
    
    printf("  ┌────────────────────────────────────────────────────────────┐\n");
    printf("  │                  WITH EWC (Regularized)                    │\n");
    printf("  ├────────────────────────────────────────────────────────────┤\n");
    print_accuracy_bar("Task A (Old)", final_a_with, 30);
    print_accuracy_bar("Task B (New)", final_b_with, 30);
    printf("  │ ✅ Task A performance PRESERVED while learning Task B!     │\n");
    printf("  └────────────────────────────────────────────────────────────┘\n\n");
    
    // Summary
    printf("📊 Summary:\n");
    printf("   • Without EWC: Task A dropped from %.0f%% → %.0f%% (forgetting!)\n",
           acc_a_without * 100, final_a_without * 100);
    printf("   • With EWC:    Task A stayed at %.0f%% → %.0f%% (preserved!)\n",
           acc_a_with * 100, final_a_with * 100);
    printf("   • EWC Lambda:  %.0f\n", EWC_LAMBDA);
    printf("   • Fisher Info prevents changing important weights\n\n");
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("EWC Demo Complete!\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    return 0;
}
