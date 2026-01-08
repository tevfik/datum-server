/**
 * @file main.c
 * @brief Tiny Transformer Demo - Intent Classification
 * 
 * Demonstrates:
 * 1. Transformer initialization
 * 2. Token embedding lookup
 * 3. Self-attention forward pass
 * 4. Sequence classification
 * 
 * Use case: Intent classification for voice assistants
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "eif_transformer.h"
#include "eif_memory.h"

// Memory pool (transformer needs more memory)
static uint8_t pool_buffer[512 * 1024];  // 512KB
static eif_memory_pool_t pool;

// Simple vocabulary for demo
const char* vocab[] = {
    "<pad>", "<unk>", "play", "music", "turn", "on", "off", "the",
    "lights", "set", "timer", "for", "minutes", "what", "is",
    "weather", "today", "call", "mom", "send", "message"
};
#define VOCAB_SIZE 21
#define NUM_CLASSES 5

const char* intent_labels[] = {
    "play_music",
    "smart_home", 
    "set_timer",
    "get_weather",
    "communication"
};

// Convert text to token IDs (simple word matching)
int tokenize(const char* text, int32_t* tokens, int max_len) {
    // Simple tokenization by splitting on spaces
    char buffer[256];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    int count = 0;
    char* token = strtok(buffer, " ");
    
    while (token && count < max_len) {
        // Find token in vocabulary
        int found = 1;  // Default to <unk>
        for (int i = 0; i < VOCAB_SIZE; i++) {
            if (strcmp(token, vocab[i]) == 0) {
                found = i;
                break;
            }
        }
        tokens[count++] = found;
        token = strtok(NULL, " ");
    }
    
    // Pad remaining
    while (count < max_len) {
        tokens[count++] = 0;  // <pad>
    }
    
    return count;
}

// Initialize random weights for demo
void init_random_weights(eif_transformer_t* model) {
    srand(42);
    
    // Token embeddings
    for (int i = 0; i < model->vocab_size * model->embed_dim; i++) {
        model->token_embed[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    }
    
    // Positional embeddings (sinusoidal-like)
    for (int pos = 0; pos < model->max_seq_len; pos++) {
        for (int d = 0; d < model->embed_dim; d++) {
            float angle = pos / powf(10000.0f, 2.0f * d / model->embed_dim);
            model->pos_embed[pos * model->embed_dim + d] = 
                (d % 2 == 0) ? sinf(angle) : cosf(angle);
        }
    }
    
    // Layer weights
    for (int l = 0; l < model->num_layers; l++) {
        eif_transformer_layer_t* layer = &model->layers[l];
        
        // Attention weights (Xavier initialization)
        float scale = sqrtf(2.0f / (model->embed_dim + model->embed_dim));
        int attn_size = model->embed_dim * model->embed_dim;
        
        for (int i = 0; i < attn_size; i++) {
            layer->attention.wq[i] = ((float)rand() / RAND_MAX - 0.5f) * scale;
            layer->attention.wk[i] = ((float)rand() / RAND_MAX - 0.5f) * scale;
            layer->attention.wv[i] = ((float)rand() / RAND_MAX - 0.5f) * scale;
            layer->attention.wo[i] = ((float)rand() / RAND_MAX - 0.5f) * scale;
        }
        
        // FFN weights
        int ffn_size1 = model->embed_dim * model->ff_dim;
        int ffn_size2 = model->ff_dim * model->embed_dim;
        
        for (int i = 0; i < ffn_size1; i++) {
            layer->ffn.w1[i] = ((float)rand() / RAND_MAX - 0.5f) * scale;
        }
        for (int i = 0; i < ffn_size2; i++) {
            layer->ffn.w2[i] = ((float)rand() / RAND_MAX - 0.5f) * scale;
        }
        for (int i = 0; i < model->ff_dim; i++) {
            layer->ffn.b1[i] = 0.0f;
        }
        for (int i = 0; i < model->embed_dim; i++) {
            layer->ffn.b2[i] = 0.0f;
        }
    }
}

// Add classification head
void add_classifier(eif_transformer_t* model, int num_classes) {
    model->num_classes = num_classes;
    model->classifier_w = (float32_t*)eif_memory_alloc(model->pool,
        model->embed_dim * num_classes * sizeof(float32_t), sizeof(float32_t));
    model->classifier_b = (float32_t*)eif_memory_alloc(model->pool,
        num_classes * sizeof(float32_t), sizeof(float32_t));
    
    // Random init
    float scale = sqrtf(2.0f / model->embed_dim);
    for (int i = 0; i < model->embed_dim * num_classes; i++) {
        model->classifier_w[i] = ((float)rand() / RAND_MAX - 0.5f) * scale;
    }
    for (int i = 0; i < num_classes; i++) {
        model->classifier_b[i] = 0.0f;
    }
}

int main(void) {
    printf("========================================\n");
    printf("Tiny Transformer Demo\n");
    printf("Intent Classification\n");
    printf("========================================\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // =========================================
    // Model Configuration
    // =========================================
    int num_layers = 2;
    int embed_dim = 64;
    int num_heads = 4;
    int ff_dim = 128;
    int max_seq_len = 16;
    
    printf("Model Configuration:\n");
    printf("  Layers:     %d\n", num_layers);
    printf("  Embed dim:  %d\n", embed_dim);
    printf("  Heads:      %d\n", num_heads);
    printf("  FF dim:     %d\n", ff_dim);
    printf("  Max seq:    %d\n", max_seq_len);
    printf("  Vocab size: %d\n", VOCAB_SIZE);
    printf("  Classes:    %d\n\n", NUM_CLASSES);
    
    // =========================================
    // Initialize Model
    // =========================================
    printf("Initializing transformer...\n");
    
    eif_transformer_t model;
    eif_status_t status = eif_transformer_init(&model, num_layers, embed_dim,
                                                num_heads, ff_dim, VOCAB_SIZE,
                                                max_seq_len, &pool);
    
    if (status != EIF_STATUS_OK) {
        printf("Failed to initialize transformer: %d\n", status);
        return 1;
    }
    
    init_random_weights(&model);
    add_classifier(&model, NUM_CLASSES);
    
    eif_transformer_print_summary(&model);
    
    // =========================================
    // Demo: Intent Classification
    // =========================================
    printf("=== Intent Classification Demo ===\n\n");
    
    const char* test_sentences[] = {
        "play music",
        "turn on the lights",
        "set timer for minutes",
        "what is weather today",
        "call mom"
    };
    
    int expected_intents[] = {0, 1, 2, 3, 4};
    
    printf("%-30s %-15s %s\n", "Input", "Predicted", "Logits");
    printf("%-30s %-15s %s\n", "------------------------------", "---------------", 
           "---------------------");
    
    for (int i = 0; i < 5; i++) {
        // Tokenize
        int32_t tokens[16];
        tokenize(test_sentences[i], tokens, max_seq_len);
        
        // Forward pass
        float32_t hidden[16 * 64];  // seq_len * embed_dim
        status = eif_transformer_forward(&model, tokens, max_seq_len, hidden);
        
        // Classification (use mean pooling for simplicity)
        float32_t logits[NUM_CLASSES] = {0};
        
        // Mean pool over sequence
        float32_t cls_embed[64] = {0};
        for (int s = 0; s < max_seq_len; s++) {
            for (int d = 0; d < embed_dim; d++) {
                cls_embed[d] += hidden[s * embed_dim + d];
            }
        }
        for (int d = 0; d < embed_dim; d++) {
            cls_embed[d] /= max_seq_len;
        }
        
        // Linear classifier
        for (int c = 0; c < NUM_CLASSES; c++) {
            logits[c] = model.classifier_b[c];
            for (int d = 0; d < embed_dim; d++) {
                logits[c] += cls_embed[d] * model.classifier_w[d * NUM_CLASSES + c];
            }
        }
        
        // Find argmax
        int pred = 0;
        float max_logit = logits[0];
        for (int c = 1; c < NUM_CLASSES; c++) {
            if (logits[c] > max_logit) {
                max_logit = logits[c];
                pred = c;
            }
        }
        
        printf("%-30s %-15s [%.2f, %.2f, %.2f, %.2f, %.2f]\n",
               test_sentences[i], intent_labels[pred],
               logits[0], logits[1], logits[2], logits[3], logits[4]);
    }
    
    // =========================================
    // Memory Usage
    // =========================================
    printf("\n=== Memory Usage ===\n");
    
    size_t model_mem = eif_transformer_memory_required(num_layers, embed_dim,
                                                        num_heads, ff_dim,
                                                        VOCAB_SIZE, max_seq_len);
    printf("Model memory:     %.2f KB\n", model_mem / 1024.0f);
    printf("Pool used:        %.2f KB\n", pool.used / 1024.0f);
    printf("Pool peak:        %.2f KB\n", pool.peak / 1024.0f);
    printf("Pool available:   %.2f KB\n", eif_memory_available(&pool) / 1024.0f);
    
    // =========================================
    // Scaling Analysis
    // =========================================
    printf("\n=== Scaling Analysis ===\n");
    printf("Estimated memory for different configurations:\n\n");
    
    printf("%-25s %12s\n", "Configuration", "Memory");
    printf("%-25s %12s\n", "-------------------------", "------------");
    
    struct {
        const char* name;
        int layers, dim, heads, ff, vocab, seq;
    } configs[] = {
        {"Tiny (current)", 2, 64, 4, 128, 21, 16},
        {"Small", 4, 128, 4, 256, 1000, 32},
        {"Medium", 6, 256, 8, 512, 4000, 64},
        {"Large", 8, 512, 8, 1024, 8000, 128},
    };
    
    for (int i = 0; i < 4; i++) {
        size_t mem = eif_transformer_memory_required(
            configs[i].layers, configs[i].dim, configs[i].heads,
            configs[i].ff, configs[i].vocab, configs[i].seq);
        printf("%-25s %10.2f KB\n", configs[i].name, mem / 1024.0f);
    }
    
    printf("\n========================================\n");
    printf("Demo Complete!\n");
    printf("========================================\n");
    
    return 0;
}
