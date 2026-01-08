/**
 * @file eif_bf_hmm.h
 * @brief Discrete Hidden Markov Model
 */

#ifndef EIF_BF_HMM_H
#define EIF_BF_HMM_H

#include "eif_memory.h"
#include "eif_status.h"
#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int N; // Number of states
  int M; // Number of observation symbols

  // Probabilities (row-major)
  float32_t *A;  // Transition matrix [N x N] (A[i][j] = P(j|i))
  float32_t *B;  // Emission matrix [N x M]   (B[i][k] = P(k|i))
  float32_t *pi; // Initial state distribution [N]

} eif_hmm_t;

/**
 * @brief Initialize HMM
 */
eif_status_t eif_hmm_init(eif_hmm_t *hmm, int N, int M,
                          eif_memory_pool_t *pool);

/**
 * @brief Set model parameters (copies data)
 */
eif_status_t eif_hmm_set_params(eif_hmm_t *hmm, const float32_t *A,
                                const float32_t *B, const float32_t *pi);

/**
 * @brief Forward algorithm: Compute probability of observation sequence
 */
float32_t eif_hmm_forward(const eif_hmm_t *hmm, const int *obs, int T);

/**
 * @brief Viterbi algorithm: Find most likely state sequence
 * @param path Output array for state indices [T]
 */
eif_status_t eif_hmm_viterbi(const eif_hmm_t *hmm, const int *obs, int T,
                             int *path, eif_memory_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif // EIF_BF_HMM_H
