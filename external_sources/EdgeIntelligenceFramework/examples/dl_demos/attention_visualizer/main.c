/**
 * @file main.c
 * @brief Attention Visualization Demo
 *
 * Demonstrates self-attention mechanism with ASCII visualization.
 * Shows how attention weights highlight relevant parts of sequences.
 *
 * Build: make attention_visualizer
 * Run:   ./bin/attention_visualizer
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EIF_HAS_PRINTF 1
#include "eif_attention.h"

// =============================================================================
// Configuration
// =============================================================================

#define SEQ_LEN 8
#define EMBED_DIM 16

// Token names for visualization
static const char *tokens[] = {"The", "cat", "sat", "on",
                               "the", "mat", ".",   "[END]"};

// =============================================================================
// Visualization Helpers
// =============================================================================

/**
 * @brief Convert Q15 value to intensity character
 */
static char intensity_char(int16_t val) {
  float f = (float)val / 32767.0f;
  if (f > 0.8f)
    return '@';
  if (f > 0.6f)
    return '#';
  if (f > 0.4f)
    return '*';
  if (f > 0.2f)
    return '+';
  if (f > 0.1f)
    return '.';
  return ' ';
}

/**
 * @brief Print attention heatmap
 */
static void print_attention_heatmap(const int16_t *scores, int seq_len) {
  printf("\nAttention Heatmap:\n");
  printf("─────────────────────────────────────────────────\n");

  // Header
  printf("        ");
  for (int j = 0; j < seq_len; j++) {
    printf("%-5.4s ", tokens[j]);
  }
  printf("\n");

  // Rows
  for (int i = 0; i < seq_len; i++) {
    printf("%-6.5s  ", tokens[i]);
    for (int j = 0; j < seq_len; j++) {
      int16_t score = scores[i * seq_len + j];
      float f = (float)score / 32767.0f;

      // Color intensity
      if (f > 0.5f)
        printf("█%.2f ", f);
      else if (f > 0.2f)
        printf("▓%.2f ", f);
      else if (f > 0.1f)
        printf("▒%.2f ", f);
      else
        printf("░%.2f ", f);
    }
    printf("\n");
  }
}

/**
 * @brief Print ASCII attention bars for a query position
 */
static void print_attention_bars(const int16_t *scores, int query_pos,
                                 int seq_len) {
  printf("\nAttention from '%s' to all tokens:\n", tokens[query_pos]);
  printf("─────────────────────────────────────────────────\n");

  for (int j = 0; j < seq_len; j++) {
    int16_t score = scores[query_pos * seq_len + j];
    float f = (float)score / 32767.0f;
    if (f < 0)
      f = 0;

    int bar_len = (int)(f * 30);

    printf("  %-6.5s [", tokens[j]);
    for (int k = 0; k < bar_len; k++)
      printf("█");
    for (int k = bar_len; k < 30; k++)
      printf("░");
    printf("] %.2f\n", f);
  }
}

// =============================================================================
// Demo
// =============================================================================

static void print_header(void) {
  printf(
      "\n╔═══════════════════════════════════════════════════════════════╗\n");
  printf(
      "║          👁️  Attention Visualization Demo                       ║\n");
  printf("╠═══════════════════════════════════════════════════════════════╣\n");
  printf(
      "║  See how attention weights distribute across sequence tokens   ║\n");
  printf(
      "╚═══════════════════════════════════════════════════════════════╝\n\n");
}

int main(void) {
  print_header();
  srand(42);

  printf("📝 Input sequence:\n");
  printf("   \"");
  for (int i = 0; i < SEQ_LEN; i++) {
    printf("%s", tokens[i]);
    if (i < SEQ_LEN - 1)
      printf(" ");
  }
  printf("\"\n\n");

  // Generate random embeddings (in practice, these come from embedding layer)
  int16_t embeddings[SEQ_LEN * EMBED_DIM];
  for (int i = 0; i < SEQ_LEN * EMBED_DIM; i++) {
    embeddings[i] = (int16_t)(rand() % 16384 - 8192);
  }

  // Add semantic patterns to make attention more interesting
  // "cat" should attend to "sat" and "mat" (rhyming/related)
  for (int d = 0; d < EMBED_DIM / 2; d++) {
    embeddings[1 * EMBED_DIM + d] =
        embeddings[2 * EMBED_DIM + d] + (rand() % 2000 - 1000);
    embeddings[1 * EMBED_DIM + d] =
        embeddings[5 * EMBED_DIM + d] + (rand() % 2000 - 1000);
  }

  // Generate random QKV weights
  int16_t W_qkv[EMBED_DIM * 3 * EMBED_DIM];
  for (int i = 0; i < EMBED_DIM * 3 * EMBED_DIM; i++) {
    W_qkv[i] = (int16_t)(rand() % 4000 - 2000);
  }

  // Compute self-attention
  printf("🔧 Computing self-attention...\n");

  int16_t output[SEQ_LEN * EMBED_DIM];

  // Use simple self-attention
  eif_self_attention_simple(embeddings, W_qkv, output, SEQ_LEN, EMBED_DIM);

  // For visualization, compute attention scores separately
  int16_t scores[SEQ_LEN * SEQ_LEN];

  // Compute Q, K for visualization
  int16_t Q[SEQ_LEN * EMBED_DIM];
  int16_t K[SEQ_LEN * EMBED_DIM];

  for (int s = 0; s < SEQ_LEN; s++) {
    for (int d = 0; d < EMBED_DIM; d++) {
      int32_t q_acc = 0, k_acc = 0;
      for (int i = 0; i < EMBED_DIM; i++) {
        q_acc += (int32_t)embeddings[s * EMBED_DIM + i] *
                 W_qkv[i * 3 * EMBED_DIM + d];
        k_acc += (int32_t)embeddings[s * EMBED_DIM + i] *
                 W_qkv[i * 3 * EMBED_DIM + EMBED_DIM + d];
      }
      Q[s * EMBED_DIM + d] = (int16_t)(q_acc >> 15);
      K[s * EMBED_DIM + d] = (int16_t)(k_acc >> 15);
    }
  }

  // Compute attention scores: Q @ K^T
  int16_t scale = eif_attn_rsqrt_q15(EMBED_DIM << 15);

  for (int i = 0; i < SEQ_LEN; i++) {
    for (int j = 0; j < SEQ_LEN; j++) {
      int32_t dot = 0;
      for (int d = 0; d < EMBED_DIM; d++) {
        dot += (int32_t)Q[i * EMBED_DIM + d] * K[j * EMBED_DIM + d];
      }
      dot = ((dot >> 15) * scale) >> 15;
      if (dot > 32767)
        dot = 32767;
      if (dot < -32768)
        dot = -32768;
      scores[i * SEQ_LEN + j] = (int16_t)dot;
    }

    // Apply softmax to row
    eif_attn_softmax_row(&scores[i * SEQ_LEN], SEQ_LEN);
  }

  // Visualize
  print_attention_heatmap(scores, SEQ_LEN);

  // Show detailed attention for specific queries
  printf("\n───────────────────────────────────────────────────\n");
  printf("Detailed attention distributions:\n");

  print_attention_bars(scores, 1, SEQ_LEN); // "cat"
  print_attention_bars(scores, 3, SEQ_LEN); // "on"

  // Summary statistics
  printf("\n📊 Attention Statistics:\n");
  printf("───────────────────────────────────────────────────\n");

  for (int i = 0; i < SEQ_LEN; i++) {
    // Find max attention
    int max_j = 0;
    int16_t max_score = scores[i * SEQ_LEN];
    float entropy = 0.0f;

    for (int j = 0; j < SEQ_LEN; j++) {
      if (scores[i * SEQ_LEN + j] > max_score) {
        max_score = scores[i * SEQ_LEN + j];
        max_j = j;
      }
      float p = (float)scores[i * SEQ_LEN + j] / 32767.0f;
      if (p > 0.001f) {
        entropy -= p * log2f(p);
      }
    }

    printf("  %-6.5s → %-6.5s (%.2f) | entropy: %.2f bits\n", tokens[i],
           tokens[max_j], (float)max_score / 32767.0f, entropy);
  }

  printf("\n✅ Attention visualization complete!\n\n");

  printf("💡 Interpretation:\n");
  printf("   - High attention = tokens are semantically related\n");
  printf("   - Low entropy = focused attention (single target)\n");
  printf("   - High entropy = distributed attention (multiple targets)\n\n");

  return 0;
}
