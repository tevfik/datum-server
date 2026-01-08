/**
 * @file main.c
 * @brief Few-Shot Learning Demo - Prototypical Networks for Quick Personalization
 * 
 * This demo shows N-way K-shot classification:
 * - 5-way (5 gesture classes)
 * - 3-shot (3 examples per class)
 * - Prototype-based classification using Euclidean distance
 * 
 * Scenario: Wearable device learns to recognize new user gestures from just 3 examples
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

#define NUM_CLASSES     5       // N-way classification
#define SHOTS_PER_CLASS 3       // K-shot learning
#define EMBED_DIM       8       // Embedding dimension
#define NUM_QUERIES     10      // Test queries

// Memory pool
static uint8_t pool_buffer[128 * 1024];
static eif_memory_pool_t pool;

// =============================================================================
// Gesture Data
// =============================================================================

typedef struct {
    const char* name;
    const char* emoji;
    float32_t base_embedding[EMBED_DIM];
} gesture_class_t;

static gesture_class_t gesture_classes[NUM_CLASSES] = {
    {"Wave",    "👋", {1.0f, 0.9f, 0.8f, 0.1f, 0.0f, 0.1f, 0.0f, 0.0f}},
    {"Swipe",   "👆", {0.1f, 0.0f, 0.1f, 1.0f, 0.9f, 0.8f, 0.0f, 0.1f}},
    {"Tap",     "👆", {0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.1f, 1.0f, 0.9f}},
    {"Circle",  "⭕", {0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f}},
    {"Pinch",   "🤏", {0.3f, 0.7f, 0.3f, 0.7f, 0.3f, 0.7f, 0.3f, 0.7f}}
};

// Generate noisy sample from base embedding
void generate_sample(const float32_t* base, float32_t* sample, float noise_level) {
    for (int i = 0; i < EMBED_DIM; i++) {
        float noise = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * noise_level;
        sample[i] = base[i] + noise;
    }
}

// =============================================================================
// Visualization
// =============================================================================

void print_embedding(const float32_t* embed, int len) {
    printf("[");
    for (int i = 0; i < len; i++) {
        // Block-based visualization
        int level = (int)(embed[i] * 4);
        if (level < 0) level = 0;
        if (level > 4) level = 4;
        const char* blocks[] = {"░", "▒", "▓", "█", "█"};
        printf("%s", blocks[level]);
    }
    printf("]");
}

void print_prototype_summary(eif_fewshot_t* fs) {
    printf("\n  ┌────────────────────────────────────────────────────────────┐\n");
    printf("  │                   Learned Prototypes                       │\n");
    printf("  ├───────────┬──────────┬─────────────────────────────────────┤\n");
    printf("  │  Gesture  │ Samples  │         Prototype Embedding         │\n");
    printf("  ├───────────┼──────────┼─────────────────────────────────────┤\n");
    
    for (int i = 0; i < fs->num_classes; i++) {
        printf("  │ %s %-7s │    %d     │ ", 
               gesture_classes[fs->prototypes[i].class_id].emoji,
               gesture_classes[fs->prototypes[i].class_id].name,
               fs->prototypes[i].num_samples);
        print_embedding(fs->prototypes[i].embedding, EMBED_DIM);
        printf(" │\n");
    }
    printf("  └───────────┴──────────┴─────────────────────────────────────┘\n");
}

void print_classification_result(int true_class, int pred_class, float distance,
                                  float32_t* probs, int num_classes) {
    bool correct = (true_class == pred_class);
    
    printf("  │ %s vs %s │ %6.3f │ ",
           gesture_classes[true_class].emoji,
           gesture_classes[pred_class].emoji,
           distance);
    
    // Mini probability bar
    for (int c = 0; c < num_classes; c++) {
        int bar_len = (int)(probs[c] * 10);
        if (c == pred_class) printf("\033[32m");  // Green for prediction
        for (int b = 0; b < bar_len; b++) printf("█");
        for (int b = bar_len; b < 10; b++) printf("░");
        if (c == pred_class) printf("\033[0m");
        printf(" ");
    }
    
    printf("│ %s │\n", correct ? "✓" : "✗");
}

// =============================================================================
// Main
// =============================================================================

int main(void) {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║        Few-Shot Learning Demo - Prototypical Networks         ║\n");
    printf("║        Quick Personalization with Just 3 Examples             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    srand(42);
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Show the task
    printf("📋 Task: %d-way %d-shot Classification\n", NUM_CLASSES, SHOTS_PER_CLASS);
    printf("   Learn to recognize %d gestures from %d examples each\n\n", 
           NUM_CLASSES, SHOTS_PER_CLASS);
    
    // Show gesture classes
    printf("  Gesture Classes:\n");
    for (int c = 0; c < NUM_CLASSES; c++) {
        printf("    %s %s: ", gesture_classes[c].emoji, gesture_classes[c].name);
        print_embedding(gesture_classes[c].base_embedding, EMBED_DIM);
        printf("\n");
    }
    printf("\n");
    
    // Initialize few-shot learner
    eif_fewshot_t fewshot;
    eif_fewshot_init(&fewshot, NUM_CLASSES, EMBED_DIM, &pool);
    
    // ==========================================================================
    // Support Set: Learn from K examples per class
    // ==========================================================================
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Phase 1: Support Set (Learning from %d examples per class)\n", SHOTS_PER_CLASS);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    for (int c = 0; c < NUM_CLASSES; c++) {
        printf("  Adding %d-shot examples for %s %s:\n", 
               SHOTS_PER_CLASS, gesture_classes[c].emoji, gesture_classes[c].name);
        
        for (int s = 0; s < SHOTS_PER_CLASS; s++) {
            float32_t sample[EMBED_DIM];
            generate_sample(gesture_classes[c].base_embedding, sample, 0.15f);
            
            printf("    Shot %d: ", s + 1);
            print_embedding(sample, EMBED_DIM);
            printf("\n");
            
            eif_fewshot_add_example(&fewshot, sample, c);
        }
        printf("\n");
    }
    
    // Show learned prototypes
    printf("📊 Prototype Formation (mean of K shots):\n");
    print_prototype_summary(&fewshot);
    
    printf("\n");
    
    // ==========================================================================
    // Query Set: Classify new examples
    // ==========================================================================
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Phase 2: Query Set (Classifying %d new gestures)\n", NUM_QUERIES);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("  ┌───────────┬────────┬");
    for (int c = 0; c < NUM_CLASSES; c++) printf("───────────");
    printf("─┬───┐\n");
    
    printf("  │ True→Pred │ Dist   │");
    for (int c = 0; c < NUM_CLASSES; c++) {
        printf(" %s        ", gesture_classes[c].emoji);
    }
    printf("│ ✓ │\n");
    
    printf("  ├───────────┼────────┼");
    for (int c = 0; c < NUM_CLASSES; c++) printf("───────────");
    printf("─┼───┤\n");
    
    int correct = 0;
    for (int q = 0; q < NUM_QUERIES; q++) {
        // Random class for query
        int true_class = rand() % NUM_CLASSES;
        
        // Generate query sample
        float32_t query[EMBED_DIM];
        generate_sample(gesture_classes[true_class].base_embedding, query, 0.2f);
        
        // Classify
        float32_t distance;
        int pred_class = eif_fewshot_classify(&fewshot, query, &distance);
        
        // Get probabilities
        float32_t probs[NUM_CLASSES];
        eif_fewshot_predict_proba(&fewshot, query, probs);
        
        // Print result
        print_classification_result(true_class, pred_class, distance, probs, NUM_CLASSES);
        
        if (true_class == pred_class) correct++;
    }
    
    printf("  └───────────┴────────┴");
    for (int c = 0; c < NUM_CLASSES; c++) printf("───────────");
    printf("─┴───┘\n\n");
    
    // ==========================================================================
    // Accuracy Analysis
    // ==========================================================================
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Results: %d-way %d-shot Classification\n", NUM_CLASSES, SHOTS_PER_CLASS);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    float accuracy = (float)correct / NUM_QUERIES * 100.0f;
    
    printf("  📊 Accuracy: %d/%d = %.1f%%\n\n", correct, NUM_QUERIES, accuracy);
    
    // Accuracy bar
    printf("  [");
    int bar_filled = (int)(accuracy / 100.0f * 50);
    for (int i = 0; i < 50; i++) {
        if (i < bar_filled) printf("█");
        else printf("░");
    }
    printf("] %.1f%%\n\n", accuracy);
    
    // Per-class analysis (confusion-like)
    printf("  Per-class prototype quality:\n");
    for (int c = 0; c < NUM_CLASSES; c++) {
        float32_t self_dist = eif_euclidean_distance(
            gesture_classes[c].base_embedding,
            fewshot.prototypes[c].embedding,
            EMBED_DIM
        );
        printf("    %s %s: prototype distance from true = %.3f\n",
               gesture_classes[c].emoji, gesture_classes[c].name, self_dist);
    }
    
    // Summary
    printf("\n  Summary:\n");
    printf("    • Training examples per class: %d\n", SHOTS_PER_CLASS);
    printf("    • Total training examples: %d\n", NUM_CLASSES * SHOTS_PER_CLASS);
    printf("    • Number of classes: %d\n", NUM_CLASSES);
    printf("    • Test queries: %d\n", NUM_QUERIES);
    printf("    • Accuracy: %.1f%%\n", accuracy);
    printf("    • Metric: Euclidean distance to prototypes\n");
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("Few-Shot Learning Demo Complete!\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    return 0;
}
