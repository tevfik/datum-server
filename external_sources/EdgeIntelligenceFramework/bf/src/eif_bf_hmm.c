/**
 * @file eif_bf_hmm.c
 * @brief Discrete Hidden Markov Model Implementation
 */

#include "eif_bf_hmm.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define MAX_STATES                                                             \
  16 // Small limit for stack buffers if needed, but we use pool.

// Safe log
static inline float32_t safe_log(float32_t x) {
  return (x > 1e-9f) ? logf(x)
                     : -1e9f; // -1e9 fits in float and effectively -inf
}

eif_status_t eif_hmm_init(eif_hmm_t *hmm, int N, int M,
                          eif_memory_pool_t *pool) {
  if (!hmm || N <= 0 || M <= 0 || !pool)
    return EIF_STATUS_INVALID_ARGUMENT;

  hmm->N = N;
  hmm->M = M;

  hmm->A = eif_memory_alloc(pool, N * N * sizeof(float32_t), 4);
  hmm->B = eif_memory_alloc(pool, N * M * sizeof(float32_t), 4);
  hmm->pi = eif_memory_alloc(pool, N * sizeof(float32_t), 4);

  if (!hmm->A || !hmm->B || !hmm->pi)
    return EIF_STATUS_OUT_OF_MEMORY;

  return EIF_STATUS_OK;
}

eif_status_t eif_hmm_set_params(eif_hmm_t *hmm, const float32_t *A,
                                const float32_t *B, const float32_t *pi) {
  if (!hmm || !A || !B || !pi)
    return EIF_STATUS_INVALID_ARGUMENT;

  memcpy(hmm->A, A, hmm->N * hmm->N * sizeof(float32_t));
  memcpy(hmm->B, B, hmm->N * hmm->M * sizeof(float32_t));
  memcpy(hmm->pi, pi, hmm->N * sizeof(float32_t));

  return EIF_STATUS_OK;
}

// Forward Algorithm with Scaling
float32_t eif_hmm_forward(const eif_hmm_t *hmm, const int *obs, int T) {
  if (!hmm || !obs || T <= 0)
    return -INFINITY;

  int N = hmm->N;
  // We need alpha buffer: [current] and [prev] is enough.
  // However, A matrix access A[i][j] is A[i*N + j].

  float32_t alpha[MAX_STATES];
  float32_t alpha_prev[MAX_STATES];

  if (N > MAX_STATES)
    return -INFINITY; // Simplification

  float32_t log_prob = 0.0f;

  // Init (t=0)
  float32_t c0 = 0.0f;
  for (int i = 0; i < N; i++) {
    alpha_prev[i] = hmm->pi[i] * hmm->B[i * hmm->M + obs[0]];
    c0 += alpha_prev[i];
  }

  // Scale
  c0 = (c0 > 0) ? 1.0f / c0 : 1.0f; // Prevent div by zero
  log_prob -= logf(c0);
  for (int i = 0; i < N; i++)
    alpha_prev[i] *= c0;

  // Recursion
  for (int t = 1; t < T; t++) {
    float32_t c = 0.0f;
    for (int j = 0; j < N; j++) {
      float32_t sum = 0.0f;
      for (int i = 0; i < N; i++) {
        sum += alpha_prev[i] * hmm->A[i * N + j];
      }
      alpha[j] = sum * hmm->B[j * hmm->M + obs[t]];
      c += alpha[j];
    }

    // Scale
    c = (c > 0) ? 1.0f / c : 1.0f;
    log_prob -= logf(c); // Add log scale factor
    for (int j = 0; j < N; j++) {
      alpha[j] *= c;
      alpha_prev[j] = alpha[j];
    }
  }

  // Final prob is sum(alpha[T-1]) which is 1.0 due to scaling,
  // so log P = -sum(log(c_t)).

  // Usually Forward returns P(O|lambda).
  return log_prob; // Return log probability
}

// Viterbi Algorithm (Log domain)
eif_status_t eif_hmm_viterbi(const eif_hmm_t *hmm, const int *obs, int T,
                             int *path, eif_memory_pool_t *pool) {
  if (!hmm || !obs || !path || !pool)
    return EIF_STATUS_INVALID_ARGUMENT;

  int N = hmm->N;
  if (N > MAX_STATES)
    return EIF_STATUS_ERROR;

  // Allocate Delta [T][N] and Psi [T][N]
  // We need full table for backtracking
  float32_t *delta = eif_memory_alloc(pool, T * N * sizeof(float32_t), 4);
  int *psi = eif_memory_alloc(pool, T * N * sizeof(int), 4);

  if (!delta || !psi)
    return EIF_STATUS_OUT_OF_MEMORY;

  // Init (Log domain)
  for (int i = 0; i < N; i++) {
    delta[0 * N + i] =
        safe_log(hmm->pi[i]) + safe_log(hmm->B[i * hmm->M + obs[0]]);
    psi[0 * N + i] = 0;
  }

  // Recursion
  for (int t = 1; t < T; t++) {
    for (int j = 0; j < N; j++) {
      float32_t max_val = -1e9f;
      int max_idx = 0;

      for (int i = 0; i < N; i++) {
        float32_t val = delta[(t - 1) * N + i] + safe_log(hmm->A[i * N + j]);
        if (val > max_val) {
          max_val = val;
          max_idx = i;
        }
      }

      delta[t * N + j] = max_val + safe_log(hmm->B[j * hmm->M + obs[t]]);
      psi[t * N + j] = max_idx;
    }
  }

  // Termination
  float32_t max_val = -1e9f;
  int q_star = 0;
  for (int i = 0; i < N; i++) {
    if (delta[(T - 1) * N + i] > max_val) {
      max_val = delta[(T - 1) * N + i];
      q_star = i;
    }
  }

  // Backtrack
  path[T - 1] = q_star;
  for (int t = T - 2; t >= 0; t--) {
    path[t] = psi[(t + 1) * N + path[t + 1]];
  }

  return EIF_STATUS_OK;
}
