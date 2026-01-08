/**
 * @file main.c
 * @brief Dynamic Time Warping (DTW) Pattern Matching Demo
 *
 * Demonstrates matching a noisy input signal against known templates.
 *
 * Usage:
 *   ./dtw_match_demo --json
 */

#include "eif_ts.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEQ_LEN 50
#define NUM_TEMPLATES 3

// Templates: 0=Sine, 1=Square, 2=Triangle
static float32_t templates[NUM_TEMPLATES][SEQ_LEN];

void generate_templates() {
  for (int i = 0; i < SEQ_LEN; i++) {
    float t = (float)i / SEQ_LEN * 2 * M_PI;
    templates[0][i] = sinf(t); // Sine

    templates[1][i] = (i < SEQ_LEN / 2) ? 1.0f : -1.0f; // Square

    // Triangle
    if (i < SEQ_LEN / 2)
      templates[2][i] = (float)i / (SEQ_LEN / 2) * 2 - 1;
    else
      templates[2][i] = 1.0f - (float)(i - SEQ_LEN / 2) / (SEQ_LEN / 2) * 2;
  }
}

int main(int argc, char **argv) {
  int json_mode = 0;
  if (argc > 1 && strcmp(argv[1], "--json") == 0)
    json_mode = 1;

  // Init memory pool
  uint8_t pool_buf[8192];
  eif_memory_pool_t pool;
  eif_memory_pool_init(&pool, pool_buf, sizeof(pool_buf));

  generate_templates();

  // Generate noisy input (Noisy Sine)
  float32_t input[SEQ_LEN];
  for (int i = 0; i < SEQ_LEN; i++) {
    // Shifted, scaled, noisy sine
    float t = (float)(i + 5) / SEQ_LEN * 2 * M_PI; // Phase shift
    input[i] = sinf(t) * 1.1f + ((float)(rand() % 100) / 1000.0f - 0.05f);
  }

  // Compute DTW distances
  float32_t distances[NUM_TEMPLATES];
  int best_match = -1;
  float32_t min_dist = 1e9f;

  for (int k = 0; k < NUM_TEMPLATES; k++) {
    distances[k] =
        eif_ts_dtw_compute(input, SEQ_LEN, templates[k], SEQ_LEN, 10, &pool);
    if (distances[k] < min_dist) {
      min_dist = distances[k];
      best_match = k;
    }
    eif_memory_reset(&pool); // Reuse pool for next computation
  }

  const char *names[] = {"Sine", "Square", "Triangle"};

  if (json_mode) {
    printf("{\"type\": \"dtw_match\", \"best_match\": \"%s\", \"distances\": {",
           names[best_match]);
    for (int k = 0; k < NUM_TEMPLATES; k++) {
      printf("\"%s\": %.4f%s", names[k], distances[k],
             (k < NUM_TEMPLATES - 1) ? ", " : "");
    }
    printf("}, \"input\": [");
    for (int i = 0; i < SEQ_LEN; i++)
      printf("%.3f%s", input[i], (i < SEQ_LEN - 1) ? ", " : "");
    printf("]}\n");
  } else {
    printf("=== DTW Pattern Matching ===\n\n");
    printf("Input: Noisy Sine wave (shifted)\n\n");
    printf("Distances:\n");
    for (int k = 0; k < NUM_TEMPLATES; k++) {
      printf("  %s: %.4f%s\n", names[k], distances[k],
             (k == best_match) ? " <--- MATCH" : "");
    }
  }

  return 0;
}
