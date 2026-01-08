/**
 * @file main.c
 * @brief Edge Learning Demo - On-Device Learning Algorithms
 * 
 * Demonstrates 4 edge learning techniques:
 * 1. Federated Learning (FedAvg) - Privacy-preserving distributed learning
 * 2. Continual Learning (EWC) - Learn without forgetting
 * 3. Online Learning - Streaming data adaptation
 * 4. Few-Shot Learning - Quick personalization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "eif_el.h"
#include "eif_memory.h"

// Memory pool
static uint8_t pool_buffer[256 * 1024];
static eif_memory_pool_t pool;

// =============================================================================
// Demo 1: Federated Learning
// =============================================================================

void demo_federated_learning(void) {
    printf("\n");
    printf("=================================================\n");
    printf("Demo 1: Federated Learning (FedAvg)\n");
    printf("=================================================\n");
    printf("Scenario: 3 edge devices collaboratively train\n");
    printf("          without sharing raw data\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    int num_weights = 10;  // Simple linear model: 10 input features -> 1 output
    
    // Initialize 3 federated clients
    eif_federated_client_t client1, client2, client3;
    eif_federated_init(&client1, num_weights, 0.1f, &pool);
    eif_federated_init(&client2, num_weights, 0.1f, &pool);
    eif_federated_init(&client3, num_weights, 0.1f, &pool);
    
    // Global model weights
    float32_t global_weights[10] = {0};
    
    // Set initial weights
    eif_federated_set_weights(&client1, global_weights);
    eif_federated_set_weights(&client2, global_weights);
    eif_federated_set_weights(&client3, global_weights);
    
    printf("Round 0: Initial global weights = [0, 0, ..., 0]\n\n");
    
    // Simulate 3 rounds of federated training
    for (int round = 1; round <= 3; round++) {
        printf("--- Round %d ---\n", round);
        
        // Each client trains on their local data
        float32_t inputs1[20] = {1,0,0,0,0,0,0,0,0,0, 0,1,0,0,0,0,0,0,0,0};
        float32_t targets1[2] = {1.0f, 0.5f};
        
        float32_t inputs2[20] = {0,0,1,0,0,0,0,0,0,0, 0,0,0,1,0,0,0,0,0,0};
        float32_t targets2[2] = {0.8f, 0.3f};
        
        float32_t inputs3[20] = {0,0,0,0,1,0,0,0,0,0, 0,0,0,0,0,1,0,0,0,0};
        float32_t targets3[2] = {0.6f, 0.9f};
        
        eif_federated_train_batch(&client1, inputs1, targets1, 2, 10, 1, eif_linear_gradient);
        eif_federated_train_batch(&client2, inputs2, targets2, 2, 10, 1, eif_linear_gradient);
        eif_federated_train_batch(&client3, inputs3, targets3, 2, 10, 1, eif_linear_gradient);
        
        printf("  Client 1: Trained on 2 samples\n");
        printf("  Client 2: Trained on 2 samples\n");
        printf("  Client 3: Trained on 2 samples\n");
        
        // Get updates
        float32_t delta1[10], delta2[10], delta3[10];
        eif_federated_get_update(&client1, delta1);
        eif_federated_get_update(&client2, delta2);
        eif_federated_get_update(&client3, delta3);
        
        // Aggregate (server side)
        const float32_t* deltas[3] = {delta1, delta2, delta3};
        int samples[3] = {2, 2, 2};
        eif_federated_aggregate(global_weights, deltas, samples, 3, num_weights);
        
        // Broadcast updated model
        eif_federated_set_weights(&client1, global_weights);
        eif_federated_set_weights(&client2, global_weights);
        eif_federated_set_weights(&client3, global_weights);
        
        printf("  Aggregated global weights[0:3]: [%.3f, %.3f, %.3f]\n\n",
               global_weights[0], global_weights[1], global_weights[2]);
    }
    
    printf("Result: Model trained across 3 devices without sharing data!\n");
}

// =============================================================================
// Demo 2: Continual Learning (EWC)
// =============================================================================

static void simple_gradient(const float32_t* w, const float32_t* x, 
                             float32_t* grad, int n) {
    // Simplified gradient for demo
    for (int i = 0; i < n; i++) {
        grad[i] = x[i % 5] * 0.1f;
    }
}

void demo_ewc(void) {
    printf("\n");
    printf("=================================================\n");
    printf("Demo 2: Continual Learning (EWC)\n");
    printf("=================================================\n");
    printf("Scenario: Learn Task B without forgetting Task A\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    int num_weights = 10;
    
    eif_ewc_t ewc;
    eif_ewc_init(&ewc, num_weights, 100.0f, &pool);  // lambda = 100
    
    // Initial weights (pretend we trained on Task A)
    float32_t task_a_weights[10] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f,
                                     0.1f, 0.4f, 0.6f, 0.9f, 0.7f};
    eif_ewc_set_weights(&ewc, task_a_weights);
    
    printf("Task A: Trained with weights = [1.0, 0.5, 0.3, ...]\n");
    
    // Compute Fisher Information (importance of each weight)
    float32_t task_a_data[25] = {1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,1,0, 0,0,0,0,1};
    eif_ewc_compute_fisher(&ewc, task_a_data, 5, 5, simple_gradient);
    
    printf("        Fisher computed (weight importance)\n");
    
    // Consolidate Task A
    eif_ewc_consolidate(&ewc);
    printf("        Consolidated for continual learning\n\n");
    
    // Now train on Task B with EWC regularization
    printf("Task B: Learning new task...\n");
    
    float32_t task_b_gradient[10] = {-0.5f, 0.3f, -0.2f, 0.1f, -0.4f,
                                      0.2f, -0.3f, 0.5f, -0.1f, 0.4f};
    
    // Without EWC: weights would change dramatically
    printf("        Gradient pushes weights toward Task B\n");
    
    // EWC penalty prevents forgetting
    float32_t penalty_before = eif_ewc_penalty(&ewc);
    
    // Update with EWC
    eif_ewc_update(&ewc, task_b_gradient, 0.01f);
    
    float32_t penalty_after = eif_ewc_penalty(&ewc);
    
    printf("        EWC penalty: %.3f -> %.3f\n", penalty_before, penalty_after);
    printf("        Updated weights[0:3]: [%.3f, %.3f, %.3f]\n",
           ewc.weights[0], ewc.weights[1], ewc.weights[2]);
    
    printf("\nResult: Weights constrained to preserve Task A performance!\n");
}

// =============================================================================
// Demo 3: Online Learning
// =============================================================================

void demo_online_learning(void) {
    printf("\n");
    printf("=================================================\n");
    printf("Demo 3: Online Learning + Drift Detection\n");
    printf("=================================================\n");
    printf("Scenario: Adapt to streaming data with distribution shift\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    int num_weights = 5;
    
    eif_online_learner_t learner;
    eif_online_init(&learner, num_weights, 0.1f, 20, &pool);
    
    // Initial weights
    float32_t initial_w[5] = {0.5f, 0.3f, 0.2f, 0.1f, 0.4f};
    eif_online_set_weights(&learner, initial_w);
    
    printf("Initial weights: [0.5, 0.3, 0.2, 0.1, 0.4]\n\n");
    
    // Phase 1: Normal distribution
    printf("Phase 1: Normal data stream (low error)\n");
    for (int i = 0; i < 20; i++) {
        float32_t gradient[5] = {0.01f, -0.01f, 0.02f, -0.02f, 0.01f};
        float32_t loss = 0.1f + 0.05f * ((float)rand() / RAND_MAX);
        eif_online_update(&learner, gradient, loss);
    }
    printf("  Error rate: %.2f\n", eif_online_error_rate(&learner));
    printf("  Drift detected: %s\n\n", 
           eif_online_drift_detected(&learner) ? "YES" : "NO");
    
    // Phase 2: Distribution shift (concept drift)
    printf("Phase 2: Distribution shift (high error)\n");
    for (int i = 0; i < 20; i++) {
        float32_t gradient[5] = {0.1f, -0.1f, 0.2f, -0.2f, 0.1f};
        float32_t loss = 0.5f + 0.2f * ((float)rand() / RAND_MAX);  // Higher loss
        eif_online_update(&learner, gradient, loss);
    }
    printf("  Error rate: %.2f\n", eif_online_error_rate(&learner));
    printf("  Drift detected: %s\n", 
           eif_online_drift_detected(&learner) ? "YES" : "NO");
    
    if (eif_online_drift_detected(&learner)) {
        printf("  -> Triggering model retraining!\n");
        eif_online_reset_drift(&learner);
    }
    
    printf("\nResult: System detected concept drift and can adapt!\n");
}

// =============================================================================
// Demo 4: Few-Shot Learning
// =============================================================================

void demo_fewshot_learning(void) {
    printf("\n");
    printf("=================================================\n");
    printf("Demo 4: Few-Shot Learning (Prototypes)\n");
    printf("=================================================\n");
    printf("Scenario: Recognize new gestures from 2 examples each\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    int embed_dim = 8;  // 8-dimensional embeddings
    
    eif_fewshot_t fewshot;
    eif_fewshot_init(&fewshot, 5, embed_dim, &pool);
    
    printf("Support set (training examples):\n");
    
    // Add examples for "Wave" gesture
    float32_t wave1[8] = {1.0f, 0.9f, 0.8f, 0.1f, 0.0f, 0.1f, 0.0f, 0.0f};
    float32_t wave2[8] = {0.9f, 1.0f, 0.7f, 0.2f, 0.1f, 0.0f, 0.1f, 0.0f};
    eif_fewshot_add_example(&fewshot, wave1, 0);
    eif_fewshot_add_example(&fewshot, wave2, 0);
    printf("  Gesture 0 (Wave): 2 examples\n");
    
    // Add examples for "Swipe" gesture
    float32_t swipe1[8] = {0.1f, 0.0f, 0.1f, 1.0f, 0.9f, 0.8f, 0.0f, 0.1f};
    float32_t swipe2[8] = {0.0f, 0.1f, 0.0f, 0.9f, 1.0f, 0.9f, 0.1f, 0.0f};
    eif_fewshot_add_example(&fewshot, swipe1, 1);
    eif_fewshot_add_example(&fewshot, swipe2, 1);
    printf("  Gesture 1 (Swipe): 2 examples\n");
    
    // Add examples for "Tap" gesture
    float32_t tap1[8] = {0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.1f, 1.0f, 0.9f};
    float32_t tap2[8] = {0.1f, 0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.9f, 1.0f};
    eif_fewshot_add_example(&fewshot, tap1, 2);
    eif_fewshot_add_example(&fewshot, tap2, 2);
    printf("  Gesture 2 (Tap): 2 examples\n");
    
    printf("\nRegistered %d gesture classes\n\n", eif_fewshot_num_classes(&fewshot));
    
    // Classify new queries
    printf("Query classification:\n");
    
    const char* gesture_names[] = {"Wave", "Swipe", "Tap"};
    
    // Query similar to Wave
    float32_t query1[8] = {0.95f, 0.85f, 0.75f, 0.15f, 0.05f, 0.1f, 0.05f, 0.0f};
    float32_t dist1;
    int pred1 = eif_fewshot_classify(&fewshot, query1, &dist1);
    printf("  Query 1 -> %s (distance: %.3f)\n", gesture_names[pred1], dist1);
    
    // Query similar to Swipe
    float32_t query2[8] = {0.05f, 0.1f, 0.05f, 0.85f, 0.95f, 0.85f, 0.05f, 0.1f};
    float32_t dist2;
    int pred2 = eif_fewshot_classify(&fewshot, query2, &dist2);
    printf("  Query 2 -> %s (distance: %.3f)\n", gesture_names[pred2], dist2);
    
    // Query similar to Tap
    float32_t query3[8] = {0.05f, 0.05f, 0.0f, 0.05f, 0.0f, 0.05f, 0.85f, 0.95f};
    float32_t dist3;
    int pred3 = eif_fewshot_classify(&fewshot, query3, &dist3);
    printf("  Query 3 -> %s (distance: %.3f)\n", gesture_names[pred3], dist3);
    
    // Class probabilities
    printf("\nClass probabilities for Query 1:\n");
    float32_t probs[3];
    eif_fewshot_predict_proba(&fewshot, query1, probs);
    printf("  Wave: %.1f%%, Swipe: %.1f%%, Tap: %.1f%%\n",
           probs[0] * 100, probs[1] * 100, probs[2] * 100);
    
    printf("\nResult: Classified new gestures from just 2 examples each!\n");
}

// =============================================================================
// Main
// =============================================================================

int main(void) {
    printf("========================================\n");
    printf("Edge Learning Demo\n");
    printf("On-Device Learning Algorithms\n");
    printf("========================================\n");
    
    srand(42);
    
    demo_federated_learning();
    demo_ewc();
    demo_online_learning();
    demo_fewshot_learning();
    
    printf("\n========================================\n");
    printf("All Edge Learning Demos Complete!\n");
    printf("========================================\n");
    
    return 0;
}
