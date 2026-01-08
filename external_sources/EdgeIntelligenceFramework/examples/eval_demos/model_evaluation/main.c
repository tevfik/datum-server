/**
 * @file main.c
 * @brief Model Evaluation Demo
 *
 * Demonstrates onboard evaluation of a classifier model.
 * Shows confusion matrix, per-class metrics, and timing.
 *
 * Build: make model_evaluation_demo
 * Run:   ./bin/model_evaluation_demo
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EIF_HAS_PRINTF 1
#include "eif_eval.h"
#include "eif_model.h"

// =============================================================================
// Simulated Dataset
// =============================================================================

#define NUM_CLASSES 5
#define NUM_SAMPLES 100
#define INPUT_SIZE 16

// Class labels for display
static const char *class_names[] = {"Cat", "Dog", "Bird", "Fish", "Mouse"};

// Simulated ground truth
static int test_labels[NUM_SAMPLES];

// Simulated weights (for 16->5 classifier)
static int16_t w_dense[INPUT_SIZE * NUM_CLASSES];
static int16_t b_dense[NUM_CLASSES];

static void init_simulated_data(void) {
  srand(42);

  // Random weights
  for (int i = 0; i < INPUT_SIZE * NUM_CLASSES; i++) {
    w_dense[i] = (int16_t)(rand() % 4000 - 2000);
  }
  for (int i = 0; i < NUM_CLASSES; i++) {
    b_dense[i] = (int16_t)(rand() % 1000 - 500);
  }

  // Generate labels - imbalanced for realistic confusion
  for (int i = 0; i < NUM_SAMPLES; i++) {
    // More samples of classes 0 and 1
    int r = rand() % 10;
    if (r < 3)
      test_labels[i] = 0;
    else if (r < 6)
      test_labels[i] = 1;
    else if (r < 8)
      test_labels[i] = 2;
    else if (r < 9)
      test_labels[i] = 3;
    else
      test_labels[i] = 4;
  }
}

// Simple softmax for demonstration
static void softmax_q15(int16_t *data, int size) {
  int16_t max_val = data[0];
  for (int i = 1; i < size; i++) {
    if (data[i] > max_val)
      max_val = data[i];
  }

  int32_t sum = 0;
  for (int i = 0; i < size; i++) {
    data[i] = data[i] - max_val;
    int32_t exp_approx = (data[i] > -8192) ? (data[i] + 16384) : 0;
    if (exp_approx < 0)
      exp_approx = 0;
    data[i] = (int16_t)exp_approx;
    sum += exp_approx;
  }

  if (sum > 0) {
    for (int i = 0; i < size; i++) {
      data[i] = (int16_t)(((int32_t)data[i] * 32767) / sum);
    }
  }
}

// Simulate model inference
static void simulate_inference(int sample_idx, int16_t *output) {
  // Generate "features" based on true label (biased towards correct class)
  int16_t input[INPUT_SIZE];
  int true_class = test_labels[sample_idx];

  for (int i = 0; i < INPUT_SIZE; i++) {
    // Add bias for true class features
    input[i] = (int16_t)(rand() % 10000 - 5000);
    if (i % NUM_CLASSES == true_class) {
      input[i] += 8000; // Bias for correct class
    }
  }

  // Dense layer
  for (int o = 0; o < NUM_CLASSES; o++) {
    int32_t acc = 0;
    for (int i = 0; i < INPUT_SIZE; i++) {
      acc += (int32_t)input[i] * w_dense[i * NUM_CLASSES + o];
    }
    output[o] = (int16_t)((acc >> 15) + b_dense[o]);
  }

  // Softmax
  softmax_q15(output, NUM_CLASSES);
}

// =============================================================================
// Demo
// =============================================================================

static void print_header(void) {
  printf(
      "\n╔═══════════════════════════════════════════════════════════════╗\n");
  printf(
      "║          📊 Model Evaluation Demo                              ║\n");
  printf("╠═══════════════════════════════════════════════════════════════╣\n");
  printf(
      "║  Evaluate classifier with confusion matrix and metrics         ║\n");
  printf(
      "╚═══════════════════════════════════════════════════════════════╝\n\n");
}

int main(void) {
  print_header();

  // Initialize
  init_simulated_data();

  printf("Dataset:\n");
  printf("───────────────────────────────────────────────────────\n");
  printf("  Classes: %d (%s, %s, %s, %s, %s)\n", NUM_CLASSES, class_names[0],
         class_names[1], class_names[2], class_names[3], class_names[4]);
  printf("  Samples: %d\n\n", NUM_SAMPLES);

  // Initialize evaluator
  eif_eval_t eval;
  eif_eval_init(&eval, NUM_CLASSES);

  // Initialize profiler
  eif_profiler_t profiler;
  eif_profiler_init(&profiler);
  int dense_layer = eif_profiler_add_layer(&profiler, "Dense");
  int softmax_layer = eif_profiler_add_layer(&profiler, "Softmax");

  printf("🔄 Evaluating model on %d samples...\n", NUM_SAMPLES);

  // Evaluate all samples
  int16_t output[NUM_CLASSES];

  for (int i = 0; i < NUM_SAMPLES; i++) {
    clock_t start = clock();

    // Run inference
    simulate_inference(i, output);

    clock_t end = clock();
    uint32_t elapsed_us = (uint32_t)((end - start) * 1000000 / CLOCKS_PER_SEC);

    // Update evaluation
    eif_eval_update(&eval, output, test_labels[i]);
    eif_eval_update_timing(&eval, elapsed_us);

    // Update profiler (simulated layer times)
    eif_profiler_record(&profiler, dense_layer, elapsed_us * 7 / 10);
    eif_profiler_record(&profiler, softmax_layer, elapsed_us * 3 / 10);
    eif_profiler_inference(&profiler, elapsed_us);
  }

  // Print results
  eif_eval_print(&eval);

  // Print profiler
  eif_profiler_print(&profiler);

  // Print class names mapping
  printf("\nClass Names:\n");
  printf("───────────────────────────────────────────────────────\n");
  for (int i = 0; i < NUM_CLASSES; i++) {
    printf("  %d = %s\n", i, class_names[i]);
  }

  printf("\n✅ Evaluation complete!\n\n");

  return 0;
}
