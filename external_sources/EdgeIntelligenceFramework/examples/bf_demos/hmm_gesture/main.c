/**
 * @file main.c
 * @brief Discrete HMM Gesture Recognition Demo
 *
 * Demonstrates:
 * - HMM Initialization
 * - Forward Algorithm (Probability of sequence)
 * - Viterbi Algorithm (Most likely state path)
 *
 * Models:
 * - Model A: "Swipe Up" (Transitions: Start -> Up -> Up -> End)
 * - Model B: "Swipe Down" (Transitions: Start -> Down -> Down -> End)
 */

#include "eif_bf_hmm.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Symbols
#define SYM_STABLE 0
#define SYM_UP 1
#define SYM_DOWN 2

// States
#define ST_START 0
#define ST_MOVE 1
#define ST_END 2

void setup_model_up(eif_hmm_t *hmm, eif_memory_pool_t *pool) {
  eif_hmm_init(hmm, 3, 3, pool);

  // Transitions (Left-to-Right)
  // 0->0, 0->1
  hmm->A[0 * 3 + 0] = 0.5f;
  hmm->A[0 * 3 + 1] = 0.5f;
  hmm->A[0 * 3 + 2] = 0.0f;
  // 1->1, 1->2
  hmm->A[1 * 3 + 0] = 0.0f;
  hmm->A[1 * 3 + 1] = 0.5f;
  hmm->A[1 * 3 + 2] = 0.5f;
  // 2->2
  hmm->A[2 * 3 + 0] = 0.0f;
  hmm->A[2 * 3 + 1] = 0.0f;
  hmm->A[2 * 3 + 2] = 1.0f;

  // Emissions (Prefer UP in state 1)
  // State 0 (Start): Stable
  hmm->B[0 * 3 + SYM_STABLE] = 0.9f;
  hmm->B[0 * 3 + SYM_UP] = 0.05f;
  hmm->B[0 * 3 + SYM_DOWN] = 0.05f;
  // State 1 (Move): Up
  hmm->B[1 * 3 + SYM_STABLE] = 0.1f;
  hmm->B[1 * 3 + SYM_UP] = 0.8f;
  hmm->B[1 * 3 + SYM_DOWN] = 0.1f;
  // State 2 (End): Stable
  hmm->B[2 * 3 + SYM_STABLE] = 0.9f;
  hmm->B[2 * 3 + SYM_UP] = 0.05f;
  hmm->B[2 * 3 + SYM_DOWN] = 0.05f;

  // Pi
  hmm->pi[0] = 1.0f;
  hmm->pi[1] = 0.0f;
  hmm->pi[2] = 0.0f;
}

void setup_model_down(eif_hmm_t *hmm, eif_memory_pool_t *pool) {
  eif_hmm_init(hmm, 3, 3, pool);

  // Transitions same as Up
  hmm->A[0 * 3 + 0] = 0.5f;
  hmm->A[0 * 3 + 1] = 0.5f;
  hmm->A[0 * 3 + 2] = 0.0f;
  hmm->A[1 * 3 + 0] = 0.0f;
  hmm->A[1 * 3 + 1] = 0.5f;
  hmm->A[1 * 3 + 2] = 0.5f;
  hmm->A[2 * 3 + 0] = 0.0f;
  hmm->A[2 * 3 + 1] = 0.0f;
  hmm->A[2 * 3 + 2] = 1.0f;

  // Emissions (Prefer DOWN in state 1)
  hmm->B[0 * 3 + SYM_STABLE] = 0.9f;
  hmm->B[0 * 3 + SYM_UP] = 0.05f;
  hmm->B[0 * 3 + SYM_DOWN] = 0.05f;
  hmm->B[1 * 3 + SYM_STABLE] = 0.1f;
  hmm->B[1 * 3 + SYM_UP] = 0.1f;
  hmm->B[1 * 3 + SYM_DOWN] = 0.8f;
  hmm->B[2 * 3 + SYM_STABLE] = 0.9f;
  hmm->B[2 * 3 + SYM_UP] = 0.05f;
  hmm->B[2 * 3 + SYM_DOWN] = 0.05f;

  hmm->pi[0] = 1.0f;
  hmm->pi[1] = 0.0f;
  hmm->pi[2] = 0.0f;
}

int main(int argc, char **argv) {
  int json_mode = 0;
  if (argc > 1 && strcmp(argv[1], "--json") == 0)
    json_mode = 1;

  uint8_t buf1[4096], buf2[4096], buf3[4096];
  eif_memory_pool_t pool1, pool2, pool3;
  eif_memory_pool_init(&pool1, buf1, 4096);
  eif_memory_pool_init(&pool2, buf2, 4096);
  eif_memory_pool_init(&pool3, buf3, 4096);

  eif_hmm_t hmm_up, hmm_down;
  setup_model_up(&hmm_up, &pool1);
  setup_model_down(&hmm_down, &pool2);

  // Test Sequence: Stable, Up, Up, Stable (Should match Up)
  int obs[] = {SYM_STABLE, SYM_UP, SYM_UP, SYM_STABLE};
  int T = 4;

  float32_t p_up = eif_hmm_forward(&hmm_up, obs, T);
  float32_t p_down = eif_hmm_forward(&hmm_down, obs, T);

  // Viterbi on Up model
  int path[4];
  eif_hmm_viterbi(&hmm_up, obs, T, path, &pool3);

  const char *pred = (p_up > p_down) ? "SWIPE UP" : "SWIPE DOWN";

  if (json_mode) {
    printf("{\"type\": \"hmm_gesture\", \"prediction\": \"%s\", ", pred);
    printf("\"log_probs\": {\"up\": %.2f, \"down\": %.2f}, ", p_up, p_down);
    printf("\"viterbi_path\": [%d, %d, %d, %d]}\n", path[0], path[1], path[2],
           path[3]);
  } else {
    printf("=== HMM Gesture Recognition ===\n\n");
    printf("Sequence: STABLE -> UP -> UP -> STABLE\n\n");
    printf("Log Probabilities:\n");
    printf("  Model UP:   %.2f\n", p_up);
    printf("  Model DOWN: %.2f\n", p_down);
    printf("\nResult: %s\n", pred);
    printf("\nViterbi Path (Most Likely States):\n");
    for (int i = 0; i < T; i++)
      printf("  t=%d: State %d\n", i, path[i]);
  }

  return 0;
}
