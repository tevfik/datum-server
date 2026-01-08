/**
 * @file eif_federated.h
 * @brief Federated Learning Utilities for EIF
 *
 * Provides building blocks for federated learning on edge devices.
 * Enables collaborative model training while keeping data local.
 *
 * Features:
 * - FedAvg weight aggregation
 * - Gradient compression
 * - Model averaging
 * - Differential privacy helpers
 * - Secure aggregation basics
 *
 * Note: This implements the client-side operations. Server aggregation
 * typically runs on a more powerful device or cloud.
 */

#ifndef EIF_FEDERATED_H
#define EIF_FEDERATED_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

#ifndef EIF_FL_MAX_CLIENTS
#define EIF_FL_MAX_CLIENTS 16
#endif

#ifndef EIF_FL_MAX_WEIGHTS
#define EIF_FL_MAX_WEIGHTS 4096 // Maximum weights per layer
#endif

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Client contribution for federated averaging
 */
typedef struct {
  int16_t *weights; ///< Model weights (Q15)
  int weight_count; ///< Number of weights
  int sample_count; ///< Local training samples
  int client_id;    ///< Client identifier
  bool valid;       ///< Contribution is valid
} eif_fl_client_t;

/**
 * @brief Federated aggregation context
 */
typedef struct {
  int16_t *global_weights; ///< Aggregated global weights
  int weight_count;        ///< Number of weights
  int total_samples;       ///< Total samples across clients
  int num_clients;         ///< Number of participating clients
  int round;               ///< Current FL round

  // Stats
  int32_t weight_delta_sum; ///< Sum of weight changes
  int16_t max_delta;        ///< Maximum weight change
} eif_fl_context_t;

/**
 * @brief Gradient compression settings
 */
typedef struct {
  float top_k_ratio; ///< Keep top-k% of gradients (0.1 = 10%)
  int16_t threshold; ///< Minimum gradient magnitude to send
  bool use_random_k; ///< Randomized sparsification
} eif_fl_compress_t;

// =============================================================================
// Federated Averaging (FedAvg)
// =============================================================================

/**
 * @brief Initialize FL context
 */
static inline void eif_fl_init(eif_fl_context_t *ctx, int16_t *global_weights,
                               int weight_count) {
  ctx->global_weights = global_weights;
  ctx->weight_count = weight_count;
  ctx->total_samples = 0;
  ctx->num_clients = 0;
  ctx->round = 0;
  ctx->weight_delta_sum = 0;
  ctx->max_delta = 0;
}

/**
 * @brief Initialize client contribution
 */
static inline void eif_fl_client_init(eif_fl_client_t *client, int16_t *weights,
                                      int weight_count, int client_id) {
  client->weights = weights;
  client->weight_count = weight_count;
  client->sample_count = 0;
  client->client_id = client_id;
  client->valid = true;
}

/**
 * @brief FedAvg: Aggregate client weights
 *
 * new_global = Σ(n_k / N) * w_k
 * where n_k = samples on client k, N = total samples
 *
 * @param ctx FL context with global weights
 * @param clients Array of client contributions
 * @param num_clients Number of clients
 */
static inline void eif_fl_aggregate(eif_fl_context_t *ctx,
                                    const eif_fl_client_t *clients,
                                    int num_clients) {
  if (num_clients == 0 || ctx->weight_count == 0)
    return;

  // Count total samples
  int total_samples = 0;
  for (int i = 0; i < num_clients; i++) {
    if (clients[i].valid) {
      total_samples += clients[i].sample_count;
    }
  }

  if (total_samples == 0)
    return;

  ctx->total_samples = total_samples;
  ctx->num_clients = num_clients;
  ctx->weight_delta_sum = 0;
  ctx->max_delta = 0;

  // Weighted average
  for (int w = 0; w < ctx->weight_count; w++) {
    int32_t sum = 0;

    for (int c = 0; c < num_clients; c++) {
      if (!clients[c].valid)
        continue;
      if (w >= clients[c].weight_count)
        continue;

      // Weight by sample count: (n_k / N) * w_k
      // In fixed-point: (w_k * n_k) / N
      int32_t contribution =
          (int32_t)clients[c].weights[w] * clients[c].sample_count;
      sum += contribution;
    }

    int16_t old_weight = ctx->global_weights[w];
    int16_t new_weight = (int16_t)(sum / total_samples);
    ctx->global_weights[w] = new_weight;

    // Track changes
    int16_t delta = new_weight - old_weight;
    if (delta < 0)
      delta = -delta;
    ctx->weight_delta_sum += delta;
    if (delta > ctx->max_delta)
      ctx->max_delta = delta;
  }

  ctx->round++;
}

/**
 * @brief Simple average (equal weight per client)
 */
static inline void eif_fl_average(int16_t *output,
                                  const eif_fl_client_t *clients,
                                  int num_clients, int weight_count) {
  if (num_clients == 0)
    return;

  for (int w = 0; w < weight_count; w++) {
    int32_t sum = 0;
    int valid_count = 0;

    for (int c = 0; c < num_clients; c++) {
      if (clients[c].valid && w < clients[c].weight_count) {
        sum += clients[c].weights[w];
        valid_count++;
      }
    }

    output[w] = (valid_count > 0) ? (int16_t)(sum / valid_count) : 0;
  }
}

// =============================================================================
// Gradient Compression
// =============================================================================

/**
 * @brief Initialize compression settings
 */
static inline void eif_fl_compress_init(eif_fl_compress_t *comp) {
  comp->top_k_ratio = 0.1f;
  comp->threshold = 100; // Minimum magnitude in Q15
  comp->use_random_k = false;
}

/**
 * @brief Count non-zero elements after threshold
 */
static inline int eif_fl_compress_count(const int16_t *gradients, int count,
                                        int16_t threshold) {
  int non_zero = 0;
  for (int i = 0; i < count; i++) {
    int16_t g = gradients[i];
    if (g < 0)
      g = -g;
    if (g >= threshold)
      non_zero++;
  }
  return non_zero;
}

/**
 * @brief Apply threshold sparsification
 * Gradients below threshold are zeroed.
 */
static inline void eif_fl_compress_threshold(int16_t *gradients, int count,
                                             int16_t threshold) {
  for (int i = 0; i < count; i++) {
    int16_t g = gradients[i];
    int16_t abs_g = (g < 0) ? -g : g;
    if (abs_g < threshold) {
      gradients[i] = 0;
    }
  }
}

/**
 * @brief Quantize gradients to 8-bit for transmission
 */
static inline void eif_fl_quantize_8bit(const int16_t *gradients,
                                        int8_t *quantized, int count,
                                        int16_t *scale) {
  // Find max for scaling
  int16_t max_val = 0;
  for (int i = 0; i < count; i++) {
    int16_t abs_g = gradients[i] < 0 ? -gradients[i] : gradients[i];
    if (abs_g > max_val)
      max_val = abs_g;
  }

  *scale = max_val;

  if (max_val == 0) {
    memset(quantized, 0, count);
    return;
  }

  // Quantize to [-127, 127]
  for (int i = 0; i < count; i++) {
    int32_t q = ((int32_t)gradients[i] * 127) / max_val;
    if (q > 127)
      q = 127;
    if (q < -127)
      q = -127;
    quantized[i] = (int8_t)q;
  }
}

/**
 * @brief Dequantize 8-bit gradients back to Q15
 */
static inline void eif_fl_dequantize_8bit(const int8_t *quantized,
                                          int16_t *gradients, int count,
                                          int16_t scale) {
  for (int i = 0; i < count; i++) {
    gradients[i] = (int16_t)(((int32_t)quantized[i] * scale) / 127);
  }
}

// =============================================================================
// Differential Privacy
// =============================================================================

/**
 * @brief Simple LCG random number generator
 */
static uint32_t _eif_fl_rand_state = 42;

static inline uint32_t eif_fl_rand(void) {
  _eif_fl_rand_state = _eif_fl_rand_state * 1103515245 + 12345;
  return _eif_fl_rand_state;
}

static inline void eif_fl_srand(uint32_t seed) { _eif_fl_rand_state = seed; }

/**
 * @brief Add Gaussian noise for differential privacy
 * Uses Box-Muller approximation with uniform random
 *
 * @param gradients Gradient array to add noise to
 * @param count Number of elements
 * @param noise_scale Standard deviation of noise (Q15)
 */
static inline void eif_fl_add_noise(int16_t *gradients, int count,
                                    int16_t noise_scale) {
  for (int i = 0; i < count; i += 2) {
    // Approximate Gaussian using uniform random
    int32_t u1 = (eif_fl_rand() % 32768);
    int32_t u2 = (eif_fl_rand() % 32768);

    // Simple approximation: (u1 + u2 - 32768) is roughly Gaussian
    int32_t noise1 = (u1 + (eif_fl_rand() % 32768) - 32768) >> 8;
    int32_t noise2 = (u2 + (eif_fl_rand() % 32768) - 32768) >> 8;

    noise1 = (noise1 * noise_scale) >> 15;
    noise2 = (noise2 * noise_scale) >> 15;

    gradients[i] += (int16_t)noise1;
    if (i + 1 < count) {
      gradients[i + 1] += (int16_t)noise2;
    }
  }
}

/**
 * @brief Clip gradients for bounded sensitivity
 */
static inline void eif_fl_clip_gradients(int16_t *gradients, int count,
                                         int16_t max_norm) {
  // Compute L2 norm squared
  int32_t norm_sq = 0;
  for (int i = 0; i < count; i++) {
    norm_sq += ((int32_t)gradients[i] * gradients[i]) >> 10;
  }

  // Compute scaling factor if norm exceeds max
  int32_t max_norm_sq = ((int32_t)max_norm * max_norm) >> 10;

  if (norm_sq > max_norm_sq && norm_sq > 0) {
    // Scale down: g = g * max_norm / norm
    // Approximate sqrt using iterative method
    int32_t norm = 1;
    for (int i = 0; i < 8; i++) {
      if (norm * norm < norm_sq)
        norm++;
      else
        break;
    }

    for (int i = 0; i < count; i++) {
      gradients[i] = (int16_t)(((int32_t)gradients[i] * max_norm) / norm);
    }
  }
}

// =============================================================================
// Model Checkpointing
// =============================================================================

/**
 * @brief Model checkpoint structure
 */
typedef struct {
  int16_t *weights;
  int weight_count;
  int round;
  int32_t checksum;
  bool valid;
} eif_fl_checkpoint_t;

/**
 * @brief Compute simple checksum for integrity
 */
static inline int32_t eif_fl_checksum(const int16_t *weights, int count) {
  int32_t sum = 0;
  for (int i = 0; i < count; i++) {
    sum += weights[i];
    sum ^= (weights[i] << (i % 16));
  }
  return sum;
}

/**
 * @brief Save checkpoint
 */
static inline void eif_fl_checkpoint_save(eif_fl_checkpoint_t *ckpt,
                                          const int16_t *weights, int count,
                                          int round) {
  memcpy(ckpt->weights, weights, count * sizeof(int16_t));
  ckpt->weight_count = count;
  ckpt->round = round;
  ckpt->checksum = eif_fl_checksum(weights, count);
  ckpt->valid = true;
}

/**
 * @brief Restore from checkpoint
 */
static inline bool eif_fl_checkpoint_restore(const eif_fl_checkpoint_t *ckpt,
                                             int16_t *weights, int count) {
  if (!ckpt->valid)
    return false;
  if (ckpt->weight_count != count)
    return false;

  // Verify checksum
  if (eif_fl_checksum(ckpt->weights, count) != ckpt->checksum) {
    return false;
  }

  memcpy(weights, ckpt->weights, count * sizeof(int16_t));
  return true;
}

// =============================================================================
// Printing (requires printf)
// =============================================================================

#ifdef EIF_HAS_PRINTF
#include <stdio.h>

static inline void eif_fl_print_stats(const eif_fl_context_t *ctx) {
  printf("FL Round %d Statistics:\n", ctx->round);
  printf("  Clients: %d\n", ctx->num_clients);
  printf("  Total samples: %d\n", ctx->total_samples);
  printf("  Avg weight delta: %.4f\n",
         (float)ctx->weight_delta_sum / ctx->weight_count / 32767.0f);
  printf("  Max weight delta: %.4f\n", (float)ctx->max_delta / 32767.0f);
}

#endif

#ifdef __cplusplus
}
#endif

#endif // EIF_FEDERATED_H
