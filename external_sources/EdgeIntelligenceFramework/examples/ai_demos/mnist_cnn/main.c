/**
 * @file main.c
 * @brief MNIST CNN Inference Demo
 * 
 * Demonstrates neural network inference on MNIST digit images:
 * - Model loading and deserialization
 * - CNN inference with softmax output
 * - JSON output for automation
 * 
 * Usage:
 *   ./mnist_cnn_demo <model.eif> <image.bin>                    # Standard
 *   ./mnist_cnn_demo <model.eif> <image.bin> --json             # JSON output
 *   ./mnist_cnn_demo --simulate --json                          # Simulated inference
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "eif_neural.h"

// 1MB Pool
static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;

static bool json_mode = false;
static bool simulate_mode = false;

// Helper to load file
static uint8_t* load_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* buf = malloc(*size);
    if (buf) {
        size_t read = fread(buf, 1, *size, f);
        (void)read; // Suppress unused variable warning
    }
    fclose(f);
    return buf;
}

// Simulated inference for testing
static void simulate_inference(float* output, int num_classes, int true_label) {
    // Generate softmax-like output with true_label having highest probability
    float sum = 0;
    for (int i = 0; i < num_classes; i++) {
        output[i] = (i == true_label) ? 5.0f : (float)(rand() % 100) / 100.0f;
        output[i] = expf(output[i]);
        sum += output[i];
    }
    for (int i = 0; i < num_classes; i++) {
        output[i] /= sum;
    }
}

static void output_json(float* output, int num_classes, int predicted, float confidence) {
    printf("{\"type\": \"inference\", \"model\": \"mnist_cnn\"");
    printf(", \"prediction\": %d", predicted);
    printf(", \"confidence\": %.4f", confidence);
    printf(", \"probs\": [");
    for (int i = 0; i < num_classes; i++) {
        printf("%.4f%s", output[i], i < num_classes - 1 ? ", " : "");
    }
    printf("]}\n");
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS] [model.eif] [image.bin]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON for automation\n");
    printf("  --simulate    Run simulated inference (no model needed)\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s model.eif digit_5.bin --json\n", prog);
    printf("  %s --simulate --json\n", prog);
}

int main(int argc, char** argv) {
    const char* model_path = NULL;
    const char* image_path = NULL;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--simulate") == 0) {
            simulate_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (!model_path) {
            model_path = argv[i];
        } else if (!image_path) {
            image_path = argv[i];
        }
    }
    
    if (!simulate_mode && (!model_path || !image_path)) {
        print_usage(argv[0]);
        return 1;
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    float output[10] = {0};
    int predicted = -1;
    float max_val = -1000.0f;
    
    if (simulate_mode) {
        // Simulated inference
        if (!json_mode) {
            printf("\n=== MNIST CNN Demo (Simulated) ===\n\n");
        }
        
        srand(42);
        int true_label = rand() % 10;
        simulate_inference(output, 10, true_label);
        
        for (int i = 0; i < 10; i++) {
            if (output[i] > max_val) {
                max_val = output[i];
                predicted = i;
            }
        }
        
    } else {
        // Real inference
        if (!json_mode) {
            printf("\n=== MNIST CNN Demo ===\n\n");
            printf("Loading model: %s\n", model_path);
        }
        
        size_t model_size;
        uint8_t* model_buf = load_file(model_path, &model_size);
        if (!model_buf) {
            if (json_mode) {
                printf("{\"error\": \"Failed to load model: %s\"}\n", model_path);
            } else {
                printf("Failed to load model: %s\n", model_path);
            }
            return 1;
        }
        
        eif_model_t model;
        if (eif_model_deserialize(&model, model_buf, model_size, &pool) != EIF_STATUS_OK) {
            if (json_mode) {
                printf("{\"error\": \"Failed to deserialize model\"}\n");
            } else {
                printf("Failed to deserialize model\n");
            }
            free(model_buf);
            return 1;
        }
        
        if (!json_mode) {
            printf("Model loaded. Nodes: %d, Tensors: %d\n", model.num_nodes, model.num_tensors);
        }
        
        // Load input
        size_t img_size;
        uint8_t* img_buf = load_file(image_path, &img_size);
        if (!img_buf) {
            if (json_mode) {
                printf("{\"error\": \"Failed to load image: %s\"}\n", image_path);
            } else {
                printf("Failed to load image: %s\n", image_path);
            }
            free(model_buf);
            return 1;
        }
        
        // Initialize context
        eif_neural_context_t ctx;
        if (eif_neural_init(&ctx, &model, &pool) != EIF_STATUS_OK) {
            printf("{\"error\": \"Failed to init context\"}\n");
            free(model_buf);
            free(img_buf);
            return 1;
        }
        
        // Set input and run inference
        eif_neural_set_input(&ctx, 0, img_buf, img_size);
        
        if (!json_mode) {
            printf("Running inference...\n");
        }
        
        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) {
            printf("{\"error\": \"Inference failed\"}\n");
            return 1;
        }
        
        eif_neural_get_output(&ctx, 0, output, sizeof(output));
        
        for (int i = 0; i < 10; i++) {
            if (output[i] > max_val) {
                max_val = output[i];
                predicted = i;
            }
        }
        
        free(model_buf);
        free(img_buf);
    }
    
    // Output results
    if (json_mode) {
        output_json(output, 10, predicted, max_val);
    } else {
        printf("\nPredictions:\n");
        for (int i = 0; i < 10; i++) {
            printf("  %d: %.4f %s\n", i, output[i], i == predicted ? "<-- PREDICTED" : "");
        }
        printf("\nPredicted Class: %d (confidence: %.2f%%)\n\n", predicted, max_val * 100);
    }
    
    return 0;
}
