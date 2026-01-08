/**
 * @file eif_ts_dtw.c
 * @brief Dynamic Time Warping Implementation
 */

#include "eif_ts.h"
#include <float.h>
#include <math.h>

// Helper macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN3(a, b, c) MIN(MIN(a, b), c)
#define ADIST(a, b) fabsf((a) - (b))

/**
 * @brief Compute DTW distance with optional Sakoe-Chiba window
 */
float32_t eif_ts_dtw_compute(const float32_t *s1, int len1, const float32_t *s2,
                             int len2, int window, eif_memory_pool_t *pool) {
  if (!s1 || !s2 || len1 <= 0 || len2 <= 0)
    return FLT_MAX;

  // Allocate cost matrix [len1+1][len2+1]
  // To save memory, we can use two rows (current and previous) since we only
  // look back 1 step But for full path reconstruction we'd need full matrix.
  // Here we just return distance. Let's use 2 rows approach to respect "Edge"
  // constraints.

  int cols = len2 + 1;
  float32_t *prev = eif_memory_alloc(pool, cols * sizeof(float32_t), 4);
  float32_t *curr = eif_memory_alloc(pool, cols * sizeof(float32_t), 4);

  if (!prev || !curr)
    return FLT_MAX; // Memory error

  // Initialize
  for (int j = 0; j < cols; j++)
    prev[j] = FLT_MAX;
  prev[0] = 0.0f;

  // Compute
  for (int i = 1; i <= len1; i++) {
    curr[0] = FLT_MAX;

    // Window constraints (Sakoe-Chiba)
    int start = 1;
    int end = len2;

    if (window > 0) {
      start = MAX(1, i - window);
      end = MIN(len2, i + window);
    }

    // Fill outside window with infinity if sparse
    // Since we reuse 'curr', we should reset it or handle carefully.
    // But simpler to just loop j and check condition inside or pre-calc bounds.
    // Optimized loop:
    for (int j = 1; j < start; j++)
      curr[j] = FLT_MAX;
    for (int j = end + 1; j <= len2; j++)
      curr[j] = FLT_MAX;

    for (int j = start; j <= end; j++) {
      float32_t cost = ADIST(s1[i - 1], s2[j - 1]);
      curr[j] = cost + MIN3(prev[j],      // insertion
                            curr[j - 1],  // deletion
                            prev[j - 1]); // match
    }

    // Swap rows
    float32_t *temp = prev;
    prev = curr;
    curr = temp;
  }

  float32_t result = prev[len2];

  // Free (if pool supports it, but simple pool doesn't free individual)
  // In edge context, this is fine if pool is scoped per operation.

  return result;
}
