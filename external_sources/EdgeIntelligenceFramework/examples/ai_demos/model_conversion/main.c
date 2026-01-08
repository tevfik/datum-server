/**
 * @file main.c
 * @brief Model Conversion Demo - TFLite to EIF Workflow
 * 
 * This tutorial demonstrates how to use the EIF model converter
 * to deploy TensorFlow Lite models to embedded devices.
 * 
 * SCENARIO:
 * A developer has a trained TFLite model and needs to deploy
 * it to a microcontroller using the EIF framework.
 * 
 * WORKFLOW:
 * 1. Convert TFLite model using tflite_to_eif.py
 * 2. Load converted model in C code
 * 3. Run inference
 * 4. Process results
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_neural.h"
#include "eif_model.h"
#include "../common/ascii_plot.h"

// ============================================================================
// Configuration
// ============================================================================

#define INPUT_SIZE  784     // 28x28 image for MNIST-like model
#define OUTPUT_SIZE 10      // 10 digit classes

// ============================================================================
// Mock Model Data (Simulated converted model)
// ============================================================================

// In real usage, this would be loaded from an .eif file created by tflite_to_eif.py
// Here we create a simple mock model for demonstration

typedef struct {
    int num_layers;
    int input_size;
    int output_size;
    const char* name;
} mock_model_info_t;

static mock_model_info_t mock_model = {
    .num_layers = 3,
    .input_size = 784,
    .output_size = 10,
    .name = "mnist_classifier"
};

// Simulate model inference with simple computation
static void mock_inference(const float32_t* input, int input_len, float32_t* output, int output_len) {
    // Simple mock: sum input regions and apply softmax-like
    float32_t sums[10] = {0};
    
    for (int c = 0; c < 10; c++) {
        int start = c * input_len / 10;
        int end = (c + 1) * input_len / 10;
        for (int i = start; i < end; i++) {
            sums[c] += input[i];
        }
        sums[c] = expf(sums[c] * 0.1f);
    }
    
    float32_t total = 0;
    for (int c = 0; c < 10; c++) total += sums[c];
    for (int c = 0; c < 10; c++) output[c] = sums[c] / total;
}

// ============================================================================
// Simulated Image Data
// ============================================================================

static void generate_digit_image(float32_t* image, int width, int height, int digit) {
    memset(image, 0, width * height * sizeof(float32_t));
    
    // Draw simple digit pattern
    int cx = width / 2;
    int cy = height / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = (float)(x - cx) / (width / 4);
            float dy = (float)(y - cy) / (height / 4);
            float dist = sqrtf(dx*dx + dy*dy);
            
            // Create a ring pattern based on digit
            float ring = fabsf(dist - 1.0f - digit * 0.1f);
            if (ring < 0.3f) {
                image[y * width + x] = 1.0f - ring / 0.3f;
            }
        }
    }
}

// ============================================================================
// Visualization
// ============================================================================

static void display_image(const float32_t* image, int width, int height) {
    printf("\n  Input Image (%dx%d):\n", width, height);
    printf("  ┌");
    for (int x = 0; x < width/2; x++) printf("─");
    printf("┐\n");
    
    for (int y = 0; y < height; y += 2) {
        printf("  │");
        for (int x = 0; x < width; x += 2) {
            float val = image[y * width + x];
            if (val > 0.7f) printf("█");
            else if (val > 0.4f) printf("▓");
            else if (val > 0.2f) printf("░");
            else printf(" ");
        }
        printf("│\n");
    }
    
    printf("  └");
    for (int x = 0; x < width/2; x++) printf("─");
    printf("┘\n");
}

static void display_predictions(const float32_t* probs, int num_classes) {
    printf("\n  %s┌─ Classification Results ──────────────────────┐%s\n", ASCII_CYAN, ASCII_RESET);
    
    int best = 0;
    for (int i = 1; i < num_classes; i++) {
        if (probs[i] > probs[best]) best = i;
    }
    
    for (int i = 0; i < num_classes; i++) {
        int bar_len = (int)(probs[i] * 25);
        printf("  │  Digit %d: ", i);
        
        if (i == best) printf("%s", ASCII_GREEN);
        for (int b = 0; b < bar_len; b++) printf("█");
        for (int b = bar_len; b < 25; b++) printf("░");
        printf(" %5.1f%%%s", probs[i] * 100, ASCII_RESET);
        
        if (i == best) printf(" ◄");
        printf("  │\n");
    }
    
    printf("  %s└────────────────────────────────────────────────┘%s\n", ASCII_CYAN, ASCII_RESET);
}

// ============================================================================
// Main Tutorial
// ============================================================================

int main(void) {
    printf("\n");
    ascii_section("EIF Tutorial: Model Conversion & Deployment");
    
    printf("  This tutorial demonstrates the TFLite to EIF workflow.\n\n");
    printf("  %sWorkflow:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    1. Train model in TensorFlow/Keras\n");
    printf("    2. Export to TFLite format\n");
    printf("    3. Convert using tflite_to_eif.py\n");
    printf("    4. Load and run in EIF\n");
    printf("\n  Press Enter to continue...");
    getchar();
    
    // ========================================================================
    // Step 1: Show Conversion Command
    // ========================================================================
    ascii_section("Step 1: Model Conversion");
    
    printf("  %sConvert TFLite to EIF:%s\n\n", ASCII_BOLD, ASCII_RESET);
    printf("  %s┌─ Command ─────────────────────────────────────────────┐%s\n", ASCII_CYAN, ASCII_RESET);
    printf("  │                                                        │\n");
    printf("  │  python tools/tflite_to_eif.py model.tflite model.eif  │\n");
    printf("  │                                                        │\n");
    printf("  %s└────────────────────────────────────────────────────────┘%s\n", ASCII_CYAN, ASCII_RESET);
    
    printf("\n  %sSupported Layers:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    • FULLY_CONNECTED (Dense)\n");
    printf("    • CONV_2D, DEPTHWISE_CONV_2D\n");
    printf("    • MAX_POOL_2D, AVERAGE_POOL_2D\n");
    printf("    • SOFTMAX, RELU, SIGMOID, TANH\n");
    printf("    • RESHAPE, ADD, CONCATENATION\n");
    printf("    • GRU, LSTM, RNN (experimental)\n");
    
    printf("\n  Press Enter to continue...");
    getchar();
    
    // ========================================================================
    // Step 2: Load Model
    // ========================================================================
    ascii_section("Step 2: Load Model in C");
    
    printf("  %sLoading converted model...%s\n\n", ASCII_BOLD, ASCII_RESET);
    
    // Initialize memory pool
    static uint8_t pool_buffer[256 * 1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    printf("  %s┌─ Model Info ───────────────────────────────────┐%s\n", ASCII_CYAN, ASCII_RESET);
    printf("  │  Name:         %-30s │\n", mock_model.name);
    printf("  │  Layers:       %-30d │\n", mock_model.num_layers);
    printf("  │  Input Size:   %-30d │\n", mock_model.input_size);
    printf("  │  Output Size:  %-30d │\n", mock_model.output_size);
    printf("  %s└─────────────────────────────────────────────────┘%s\n", ASCII_CYAN, ASCII_RESET);
    
    printf("\n  %sC Code Example:%s\n\n", ASCII_BOLD, ASCII_RESET);
    printf("    // Load model from file\n");
    printf("    eif_model_t model;\n");
    printf("    eif_model_load(&model, \"model.eif\", &pool);\n");
    printf("    \n");
    printf("    // Initialize neural context\n");
    printf("    eif_neural_context_t ctx;\n");
    printf("    eif_neural_init(&ctx, &model, arena, &pool);\n");
    
    printf("\n  Press Enter to continue...");
    getchar();
    
    // ========================================================================
    // Step 3: Run Inference
    // ========================================================================
    ascii_section("Step 3: Run Inference");
    
    // Allocate buffers
    float32_t* input = eif_memory_alloc(&pool, INPUT_SIZE * sizeof(float32_t), 4);
    float32_t* output = eif_memory_alloc(&pool, OUTPUT_SIZE * sizeof(float32_t), 4);
    
    // Test with different "digits"
    for (int digit = 0; digit < 3; digit++) {
        printf("  %sTest %d: Classifying digit %d%s\n", ASCII_BOLD, digit + 1, digit * 3, ASCII_RESET);
        
        // Generate test image
        generate_digit_image(input, 28, 28, digit * 3);
        display_image(input, 28, 28);
        
        // Run inference
        printf("  Running inference...\n");
        mock_inference(input, INPUT_SIZE, output, OUTPUT_SIZE);
        
        // Bias output for demo
        output[digit * 3] += 0.5f;
        float sum = 0;
        for (int i = 0; i < OUTPUT_SIZE; i++) sum += output[i];
        for (int i = 0; i < OUTPUT_SIZE; i++) output[i] /= sum;
        
        display_predictions(output, OUTPUT_SIZE);
        
        if (digit < 2) {
            printf("\n  Press Enter for next test...");
            getchar();
        }
    }
    
    // ========================================================================
    // Summary
    // ========================================================================
    printf("\n");
    ascii_section("Tutorial Summary");
    
    printf("  %sModel Conversion Pipeline:%s\n\n", ASCII_BOLD, ASCII_RESET);
    
    printf("    ┌───────────┐    ┌───────────┐    ┌───────────┐\n");
    printf("    │ TensorFlow│───►│  TFLite   │───►│    EIF    │\n");
    printf("    │  /Keras   │    │  .tflite  │    │   .eif    │\n");
    printf("    └───────────┘    └───────────┘    └───────────┘\n");
    printf("         Train          Export         Convert\n\n");
    
    printf("  %sKey EIF APIs:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    • eif_model_load()     - Load .eif model file\n");
    printf("    • eif_neural_init()    - Initialize inference context\n");
    printf("    • eif_neural_invoke()  - Run forward pass\n");
    printf("    • eif_neural_input()   - Get input tensor pointer\n");
    printf("    • eif_neural_output()  - Get output tensor pointer\n");
    
    printf("\n  %sConverter Tool:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    tools/tflite_to_eif.py - Convert TFLite to EIF format\n");
    printf("    tools/model-compiler/  - Advanced model compilation\n");
    
    printf("\n  %sTutorial Complete!%s\n\n", ASCII_GREEN ASCII_BOLD, ASCII_RESET);
    
    return 0;
}
