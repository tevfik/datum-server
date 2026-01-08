/**
 * @file main.c
 * @brief Runtime Model Build Demo
 *
 * Demonstrates building and running a model at runtime using
 * the structured model API.
 *
 * Build: make runtime_model_demo
 * Run:   ./bin/runtime_model_demo
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EIF_HAS_PRINTF 1
#include "eif_model.h"

// =============================================================================
// Simulated Weights (random for demo)
// =============================================================================

// Conv1: 3x3, 1->8 channels
static int16_t w_conv1[3 * 3 * 1 * 8];
static int16_t b_conv1[8];

// Conv2: 3x3, 8->16 channels
static int16_t w_conv2[3 * 3 * 8 * 16];
static int16_t b_conv2[16];

// Dense: 16->10
static int16_t w_dense[16 * 10];
static int16_t b_dense[10];

static void init_random_weights(void) {
  srand(42);

  for (int i = 0; i < 3 * 3 * 1 * 8; i++)
    w_conv1[i] = (int16_t)(rand() % 2000 - 1000);
  for (int i = 0; i < 8; i++)
    b_conv1[i] = (int16_t)(rand() % 200 - 100);

  for (int i = 0; i < 3 * 3 * 8 * 16; i++)
    w_conv2[i] = (int16_t)(rand() % 2000 - 1000);
  for (int i = 0; i < 16; i++)
    b_conv2[i] = (int16_t)(rand() % 200 - 100);

  for (int i = 0; i < 16 * 10; i++)
    w_dense[i] = (int16_t)(rand() % 2000 - 1000);
  for (int i = 0; i < 10; i++)
    b_dense[i] = (int16_t)(rand() % 200 - 100);
}

// =============================================================================
// Demo
// =============================================================================

static int argmax(const int16_t *data, int size) {
  int max_idx = 0;
  for (int i = 1; i < size; i++) {
    if (data[i] > data[max_idx])
      max_idx = i;
  }
  return max_idx;
}

static void print_header(void) {
  printf(
      "\n╔═══════════════════════════════════════════════════════════════╗\n");
  printf(
      "║          🏗️  Runtime Model Build Demo                          ║\n");
  printf("╠═══════════════════════════════════════════════════════════════╣\n");
  printf("║  Build and run a CNN at runtime using structured API          ║\n");
  printf(
      "╚═══════════════════════════════════════════════════════════════╝\n\n");
}

int main(void) {
  print_header();

  // Initialize weights
  init_random_weights();

  // =========================================================================
  // Define model using macros
  // =========================================================================
  printf("📐 Defining model architecture...\n\n");

  eif_layer_t layers[] = {
      // Input: 8x8x1 (small image for demo)
      EIF_INPUT(8, 8, 1),

      // Conv block 1
      EIF_CONV2D(8, 3, 1, w_conv1, b_conv1), EIF_RELU(), EIF_MAXPOOL(2),

      // Conv block 2
      EIF_CONV2D(16, 3, 1, w_conv2, b_conv2), EIF_RELU(),

      // Classifier
      EIF_GLOBAL_AVGPOOL(), EIF_DENSE(10, w_dense, b_dense), EIF_SOFTMAX()};

  int num_layers = sizeof(layers) / sizeof(layers[0]);

  // =========================================================================
  // Create model
  // =========================================================================
  printf("🔧 Creating model...\n\n");

  eif_model_t model;
  eif_status_t status = eif_model_create(&model, layers, num_layers);

  if (status != EIF_STATUS_OK) {
    printf("❌ Model creation failed: %d\n", status);
    return 1;
  }

  // Print model info
  printf("Model Configuration:\n");
  printf("═══════════════════════════════════════════════════\n");
  printf("%-20s %-15s %10s %10s\n", "Layer", "Type", "In Size", "Out Size");
  printf("───────────────────────────────────────────────────\n");

  for (int i = 0; i < model.num_layers; i++) {
    eif_layer_t *l = &model.layers[i];
    printf("%-20s %-15s %10d %10d\n", "", eif_layer_names[l->type],
           l->input_size, l->output_size);
  }

  printf("═══════════════════════════════════════════════════\n");
  printf("Total layers: %d\n", model.num_layers);
  printf("Input size: %d\n", model.input_size);
  printf("Output size: %d\n", model.output_size);
  printf("Workspace: %d elements (%d bytes)\n\n", model.workspace_size,
         model.workspace_size * 2);

  // =========================================================================
  // Run inference
  // =========================================================================
  printf("🚀 Running inference...\n\n");

  // Create test input (8x8 image)
  int16_t input[64];
  for (int i = 0; i < 64; i++) {
    input[i] = (int16_t)(rand() % 16384); // Random image
  }

  int16_t output[10];

  // Time inference
  clock_t start = clock();
  int n_runs = 1000;

  for (int i = 0; i < n_runs; i++) {
    eif_model_infer(&model, input, output);
  }

  clock_t end = clock();
  double time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0 / n_runs;

  // Print results
  printf("Output probabilities:\n");
  printf("─────────────────────────────────────\n");

  for (int i = 0; i < 10; i++) {
    float prob = output[i] / 32767.0f;
    if (prob < 0)
      prob = 0;
    if (prob > 1)
      prob = 1;

    int bar_len = (int)(prob * 25);
    printf("  Class %d: ", i);
    for (int j = 0; j < bar_len; j++)
      printf("█");
    for (int j = bar_len; j < 25; j++)
      printf("░");
    printf(" %.1f%%\n", prob * 100);
  }

  printf("\n═══════════════════════════════════════\n");
  printf("Predicted class: %d\n", argmax(output, 10));
  printf("Inference time: %.3f ms (%d runs averaged)\n", time_ms, n_runs);
  printf("═══════════════════════════════════════\n");

  // Cleanup
  eif_model_destroy(&model);

  printf("\n✅ Demo complete!\n");

  return 0;
}
