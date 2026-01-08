/**
 * @file main.c
 * @brief Federated Learning Demo - Privacy-Preserving Collaborative Training
 * 
 * This demo simulates a real-world federated learning scenario where:
 * - 5 edge devices (hospitals/banks/phones) hold private local data
 * - They collaboratively train a shared model WITHOUT sharing raw data
 * - Uses FedAvg algorithm for gradient aggregation
 * - Demonstrates privacy metrics and convergence visualization
 * 
 * Scenario: Temperature prediction from sensor data across 5 factory sensors
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

#define NUM_CLIENTS     5       // Number of federated clients (edge devices)
#define NUM_WEIGHTS     8       // Model size (8 input features -> 1 output)
#define NUM_ROUNDS      10      // Federated training rounds
#define LOCAL_EPOCHS    3       // Local training epochs per round
#define SAMPLES_PER_CLIENT 20   // Data samples per client
#define BATCH_SIZE      4       // Mini-batch size

// Memory pools
static uint8_t pool_buffer[512 * 1024];
static eif_memory_pool_t pool;

// =============================================================================
// Data Generation (Simulated Non-IID Data)
// =============================================================================

typedef struct {
    float32_t inputs[SAMPLES_PER_CLIENT * NUM_WEIGHTS];
    float32_t targets[SAMPLES_PER_CLIENT];
    int num_samples;
    const char* name;
} client_data_t;

static client_data_t client_data[NUM_CLIENTS];

// Generate synthetic sensor data with non-IID distribution
// Each client has slightly different data distribution (real-world scenario)
void generate_client_data(void) {
    const char* client_names[] = {
        "Factory-A", "Factory-B", "Factory-C", "Factory-D", "Factory-E"
    };
    
    for (int c = 0; c < NUM_CLIENTS; c++) {
        client_data[c].name = client_names[c];
        client_data[c].num_samples = SAMPLES_PER_CLIENT;
        
        // Each client has a bias (non-IID)
        float bias = 0.2f * (c - 2);  // Range: -0.4 to +0.4
        
        for (int s = 0; s < SAMPLES_PER_CLIENT; s++) {
            // Generate input features (sensor readings)
            float sum = 0.0f;
            for (int f = 0; f < NUM_WEIGHTS; f++) {
                float val = (float)rand() / RAND_MAX * 2.0f - 1.0f;
                val += bias;  // Add client-specific bias
                client_data[c].inputs[s * NUM_WEIGHTS + f] = val;
                sum += val * (0.5f + 0.1f * f);  // True weights
            }
            
            // Target: linear combination + noise
            float noise = (float)rand() / RAND_MAX * 0.2f - 0.1f;
            client_data[c].targets[s] = sum + noise;
        }
    }
}

// =============================================================================
// Privacy Metrics
// =============================================================================

typedef struct {
    float32_t gradient_norm;
    float32_t data_leakage_risk;
    int samples_protected;
} privacy_metrics_t;

void compute_privacy_metrics(const eif_federated_client_t* client,
                              privacy_metrics_t* metrics) {
    // Compute gradient L2 norm (indicator of information exposure)
    float32_t norm = 0.0f;
    for (int i = 0; i < client->num_weights; i++) {
        norm += client->gradients[i] * client->gradients[i];
    }
    metrics->gradient_norm = sqrtf(norm);
    
    // Differential privacy risk estimate (simplified)
    // Lower gradient norm = lower data leakage risk
    metrics->data_leakage_risk = metrics->gradient_norm / 10.0f;
    if (metrics->data_leakage_risk > 1.0f) {
        metrics->data_leakage_risk = 1.0f;
    }
    
    metrics->samples_protected = client->num_samples;
}

// =============================================================================
// Visualization
// =============================================================================

void print_progress_bar(int round, int total_rounds, float loss) {
    int width = 40;
    int filled = (round * width) / total_rounds;
    
    printf("\r  Progress: [");
    for (int i = 0; i < width; i++) {
        if (i < filled) printf("█");
        else printf("░");
    }
    printf("] %d/%d | Loss: %.4f", round, total_rounds, loss);
    fflush(stdout);
}

void print_client_contribution(const char* name, int samples, 
                                float weight, privacy_metrics_t* privacy) {
    printf("  │ %-10s │ %3d samples │ Weight: %.1f%% │ Privacy: %.1f%% │\n",
           name, samples, weight * 100, (1.0f - privacy->data_leakage_risk) * 100);
}

// =============================================================================
// Main Training Loop
// =============================================================================

float32_t compute_loss(const float32_t* weights, const client_data_t* data) {
    float32_t total_loss = 0.0f;
    
    for (int s = 0; s < data->num_samples; s++) {
        const float32_t* x = &data->inputs[s * NUM_WEIGHTS];
        float32_t y = data->targets[s];
        
        // Prediction
        float32_t pred = 0.0f;
        for (int i = 0; i < NUM_WEIGHTS; i++) {
            pred += weights[i] * x[i];
        }
        
        // MSE
        float32_t error = pred - y;
        total_loss += error * error;
    }
    
    return total_loss / data->num_samples;
}

int main(void) {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          Federated Learning Demo - FedAvg Algorithm           ║\n");
    printf("║       Privacy-Preserving Collaborative Model Training         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    srand(42);
    
    // Initialize memory
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Generate client data
    printf("📊 Generating non-IID client data...\n\n");
    generate_client_data();
    
    // Show data distribution
    printf("┌──────────────────────────────────────────────────────────────┐\n");
    printf("│                    Client Data Summary                       │\n");
    printf("├─────────────┬──────────────┬──────────────┬─────────────────┤\n");
    printf("│   Client    │   Samples    │  Data Bias   │   Data Range    │\n");
    printf("├─────────────┼──────────────┼──────────────┼─────────────────┤\n");
    
    for (int c = 0; c < NUM_CLIENTS; c++) {
        float bias = 0.2f * (c - 2);
        float min_val = 1e10f, max_val = -1e10f;
        for (int i = 0; i < client_data[c].num_samples * NUM_WEIGHTS; i++) {
            if (client_data[c].inputs[i] < min_val) min_val = client_data[c].inputs[i];
            if (client_data[c].inputs[i] > max_val) max_val = client_data[c].inputs[i];
        }
        printf("│ %-10s  │     %3d      │   %+.2f      │  [%+.2f, %+.2f]  │\n",
               client_data[c].name, SAMPLES_PER_CLIENT, bias, min_val, max_val);
    }
    printf("└─────────────┴──────────────┴──────────────┴─────────────────┘\n\n");
    
    // Initialize federated clients
    printf("🔌 Initializing %d federated clients...\n\n", NUM_CLIENTS);
    
    eif_federated_client_t clients[NUM_CLIENTS];
    for (int c = 0; c < NUM_CLIENTS; c++) {
        eif_federated_init(&clients[c], NUM_WEIGHTS, 0.05f, &pool);
        clients[c].local_epochs = LOCAL_EPOCHS;
        clients[c].batch_size = BATCH_SIZE;
    }
    
    // Global model
    float32_t global_weights[NUM_WEIGHTS] = {0};
    float32_t loss_history[NUM_ROUNDS];
    
    // Broadcast initial weights
    for (int c = 0; c < NUM_CLIENTS; c++) {
        eif_federated_set_weights(&clients[c], global_weights);
    }
    
    // Federated training
    printf("🚀 Starting Federated Training\n");
    printf("   Rounds: %d | Local Epochs: %d | Batch Size: %d\n\n",
           NUM_ROUNDS, LOCAL_EPOCHS, BATCH_SIZE);
    
    for (int round = 1; round <= NUM_ROUNDS; round++) {
        // Local training on each client
        for (int c = 0; c < NUM_CLIENTS; c++) {
            client_data_t* data = &client_data[c];
            
            for (int epoch = 0; epoch < LOCAL_EPOCHS; epoch++) {
                // Train on mini-batches
                for (int b = 0; b < data->num_samples; b += BATCH_SIZE) {
                    int actual_batch = (b + BATCH_SIZE <= data->num_samples) ? 
                                       BATCH_SIZE : (data->num_samples - b);
                    
                    eif_federated_train_batch(
                        &clients[c],
                        &data->inputs[b * NUM_WEIGHTS],
                        &data->targets[b],
                        actual_batch,
                        NUM_WEIGHTS, 1,
                        eif_linear_gradient
                    );
                }
            }
        }
        
        // Collect updates
        float32_t* client_deltas[NUM_CLIENTS];
        int samples[NUM_CLIENTS];
        
        for (int c = 0; c < NUM_CLIENTS; c++) {
            client_deltas[c] = (float32_t*)malloc(NUM_WEIGHTS * sizeof(float32_t));
            eif_federated_get_update(&clients[c], client_deltas[c]);
            samples[c] = clients[c].num_samples;
        }
        
        // Aggregate (FedAvg)
        eif_federated_aggregate(global_weights, 
                                 (const float32_t**)client_deltas,
                                 samples, NUM_CLIENTS, NUM_WEIGHTS);
        
        // Broadcast updated model
        for (int c = 0; c < NUM_CLIENTS; c++) {
            eif_federated_set_weights(&clients[c], global_weights);
            free(client_deltas[c]);
        }
        
        // Compute global loss
        float32_t total_loss = 0.0f;
        for (int c = 0; c < NUM_CLIENTS; c++) {
            total_loss += compute_loss(global_weights, &client_data[c]);
        }
        loss_history[round - 1] = total_loss / NUM_CLIENTS;
        
        print_progress_bar(round, NUM_ROUNDS, loss_history[round - 1]);
    }
    
    printf("\n\n");
    
    // Show per-client contributions
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│                   Client Contributions (Final Round)            │\n");
    printf("├─────────────┬─────────────┬─────────────┬───────────────────────┤\n");
    printf("│   Client    │   Samples   │   Weight    │   Privacy Score       │\n");
    printf("├─────────────┼─────────────┼─────────────┼───────────────────────┤\n");
    
    int total_samples = 0;
    for (int c = 0; c < NUM_CLIENTS; c++) {
        total_samples += client_data[c].num_samples;
    }
    
    for (int c = 0; c < NUM_CLIENTS; c++) {
        privacy_metrics_t privacy;
        compute_privacy_metrics(&clients[c], &privacy);
        float weight = (float)client_data[c].num_samples / total_samples;
        print_client_contribution(client_data[c].name, 
                                   client_data[c].num_samples,
                                   weight, &privacy);
    }
    printf("└─────────────┴─────────────┴─────────────┴───────────────────────┘\n\n");
    
    // Loss curve (ASCII visualization)
    printf("📈 Training Loss Curve:\n\n");
    printf("  Loss │\n");
    
    float max_loss = loss_history[0];
    float min_loss = loss_history[NUM_ROUNDS - 1];
    
    for (int row = 5; row >= 0; row--) {
        float threshold = min_loss + (max_loss - min_loss) * row / 5.0f;
        printf("  %5.2f │", threshold);
        for (int r = 0; r < NUM_ROUNDS; r++) {
            if (loss_history[r] >= threshold) {
                printf(" ●");
            } else {
                printf("  ");
            }
        }
        printf("\n");
    }
    printf("       └");
    for (int r = 0; r < NUM_ROUNDS; r++) printf("──");
    printf("─ Round\n");
    printf("         ");
    for (int r = 1; r <= NUM_ROUNDS; r++) printf("%2d", r);
    printf("\n\n");
    
    // Final model weights
    printf("📊 Final Global Model Weights:\n   [");
    for (int i = 0; i < NUM_WEIGHTS; i++) {
        printf("%.3f%s", global_weights[i], i < NUM_WEIGHTS - 1 ? ", " : "");
    }
    printf("]\n\n");
    
    // Privacy summary
    printf("🔒 Privacy Summary:\n");
    printf("   ✓ Raw data NEVER left the devices\n");
    printf("   ✓ Only aggregated gradients were shared\n");
    printf("   ✓ Total samples protected: %d\n", total_samples);
    printf("   ✓ Model trained collaboratively across %d devices\n\n", NUM_CLIENTS);
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Federated Learning Demo Complete!\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    return 0;
}
