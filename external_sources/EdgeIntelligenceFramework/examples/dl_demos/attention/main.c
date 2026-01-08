/**
 * @file main.c
 * @brief Self-Attention Mechanism Demo
 *
 * Demonstrates attention mechanism for NLP/sequence tasks:
 * - Scaled dot-product attention
 * - Multi-head attention
 * - Positional encoding
 * - Visualization
 *
 * Usage:
 *   ./attention_demo --help
 *   ./attention_demo --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"
#include "eif_nn_attention.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SEQ_LEN 8
#define D_MODEL 16
#define NUM_HEADS 4

static bool json_mode = false;

// Demo: Attention basics
static void demo_attention_basics(void) {
  if (!json_mode) {
    ascii_section("1. Self-Attention Mechanism");
    printf("  Core of Transformer architecture\n\n");
  }

  if (!json_mode) {
    printf("  Attention Formula:\n");
    printf("    Attention(Q,K,V) = softmax(Q*K^T / sqrt(d_k)) * V\n\n");

    printf("  Components:\n");
    printf("    • Q (Query): What am I looking for?\n");
    printf("    • K (Key): What do I contain?\n");
    printf("    • V (Value): What information do I provide?\n\n");

    printf("  Intuition:\n");
    printf("    • Each position attends to all other positions\n");
    printf("    • Learns which positions are relevant\n");
    printf("    • Enables long-range dependencies\n");
  }
}

// Demo: Scaled dot-product
static void demo_scaled_dot_product(void) {
  if (!json_mode) {
    ascii_section("2. Scaled Dot-Product Attention");
    printf("  Computing attention weights\n\n");
  }

  // Simple example: 4 tokens
  int seq_len = 4;
  int d_k = 4;

  // Initialize attention
  eif_attention_t attn;
  eif_attention_init(&attn, seq_len, d_k, d_k);

  // Mock Q, K, V (would come from linear projections)
  float Q[16] = {
      1, 0, 0, 0, // Token 1 query
      0, 1, 0, 0, // Token 2 query
      0, 0, 1, 0, // Token 3 query
      0, 0, 0, 1  // Token 4 query
  };

  float K[16] = {
      1,   0,   0,   0,  // Token 1 key (similar to Q1)
      0.5, 0.5, 0,   0,  // Token 2 key
      0,   0,   1,   0,  // Token 3 key (similar to Q3)
      0,   0,   0.5, 0.5 // Token 4 key
  };

  float V[16] = {
      1,  2,  3,  4,  // Token 1 value
      5,  6,  7,  8,  // Token 2 value
      9,  10, 11, 12, // Token 3 value
      13, 14, 15, 16  // Token 4 value
  };

  float output[16];
  eif_attention_forward(&attn, Q, K, V, output);

  if (json_mode) {
    printf("{\"demo\": \"scaled_dot_product\", \"seq_len\": %d, \"d_k\": %d}\n",
           seq_len, d_k);
  } else {
    printf("  Attention computation complete.\n");
    printf("  Output (first token values): [%.1f, %.1f, %.1f, %.1f]\n",
           output[0], output[1], output[2], output[3]);

    printf("\n  Interpretation:\n");
    printf("    • Each token attends to all tokens\n");
    printf("    • Similar Q-K pairs get higher weights\n");
    printf("    • Output is weighted sum of values\n");
  }
}

// Demo: Positional encoding
static void demo_positional_encoding(void) {
  if (!json_mode) {
    ascii_section("3. Positional Encoding");
    printf("  Adding position information to embeddings\n\n");
  }

  int d_model = 8;
  float pos_enc[SEQ_LEN * 8];

  // Generate positional encoding
  for (int pos = 0; pos < SEQ_LEN; pos++) {
    for (int i = 0; i < d_model; i++) {
      pos_enc[pos * d_model + i] = eif_position_encoding(pos, i, d_model);
    }
  }

  if (json_mode) {
    printf("{\"demo\": \"positional_encoding\", \"seq_len\": %d}\n", SEQ_LEN);
  } else {
    printf("  Positional Encoding (first 4 dims):\n");
    printf("  Pos   Dim0   Dim1   Dim2   Dim3\n");
    for (int pos = 0; pos < SEQ_LEN; pos++) {
      printf("  %3d  %+5.2f  %+5.2f  %+5.2f  %+5.2f\n", pos,
             pos_enc[pos * d_model + 0], pos_enc[pos * d_model + 1],
             pos_enc[pos * d_model + 2], pos_enc[pos * d_model + 3]);
    }

    printf("\n  Properties:\n");
    printf("    • Unique encoding for each position\n");
    printf("    • Smooth interpolation between positions\n");
    printf("    • Can extrapolate to longer sequences\n");
  }
}

// Demo: Layer normalization
static void demo_layer_norm(void) {
  if (!json_mode) {
    ascii_section("4. Layer Normalization");
    printf("  Stabilizing training with normalization\n\n");
  }

  int size = 8;
  float input[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  float output[8];

  // Copy input to output for normalization
  for (int i = 0; i < size; i++)
    output[i] = input[i];

  eif_layer_norm(output, size, 1e-5f);

  if (json_mode) {
    printf("{\"demo\": \"layer_norm\"}\n");
  } else {
    printf("  Input:  ");
    for (int i = 0; i < size; i++)
      printf("%5.2f ", input[i]);
    printf("\n");

    printf("  Output: ");
    for (int i = 0; i < size; i++)
      printf("%+5.2f ", output[i]);
    printf("\n");

    // Verify mean and std
    float mean = 0, var = 0;
    for (int i = 0; i < size; i++)
      mean += output[i];
    mean /= size;
    for (int i = 0; i < size; i++)
      var += (output[i] - mean) * (output[i] - mean);
    var /= size;

    printf("\n  Normalized stats:\n");
    printf("    Mean: %.4f (should be ~0)\n", mean);
    printf("    Std:  %.4f (should be ~1)\n", sqrtf(var));
  }
}

// Demo: Use cases
static void demo_use_cases(void) {
  if (!json_mode) {
    ascii_section("5. Attention for Edge AI");
    printf("  Practical applications\n\n");
  }

  if (!json_mode) {
    printf("  1. Keyword Spotting\n");
    printf("     - Attend to important audio frames\n");
    printf("     - Ignore silence/noise\n\n");

    printf("  2. Sensor Fusion\n");
    printf("     - Attention over multiple sensors\n");
    printf("     - Dynamic weighting\n\n");

    printf("  3. Sequence Classification\n");
    printf("     - Gesture recognition\n");
    printf("     - Activity detection\n\n");

    printf("  4. Text Commands\n");
    printf("     - Keyword extraction\n");
    printf("     - Intent classification\n");
  }
}

int main(int argc, char **argv) {
  demo_cli_result_t result = demo_parse_args(
      argc, argv, "attention_demo", "Self-attention mechanism demonstration");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Self-Attention Demo");
    printf("  Transformer-style attention for Edge AI\n\n");
  }

  demo_attention_basics();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_scaled_dot_product();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_positional_encoding();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_layer_norm();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_use_cases();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  Attention capabilities:\n");
    printf("    • Scaled dot-product attention\n");
    printf("    • Multi-head attention\n");
    printf("    • Positional encoding\n");
    printf("    • Layer normalization\n\n");
  }

  return 0;
}
