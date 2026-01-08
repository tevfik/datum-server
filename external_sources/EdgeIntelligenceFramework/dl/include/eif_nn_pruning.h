/**
 * @file eif_nn_pruning.h
 * @brief Neural Network Pruning for Edge Deployment
 *
 * Weight pruning algorithms to reduce model size:
 * - Magnitude-based pruning
 * - Structured pruning (channel/filter)
 * - Sparse matrix utilities
 *
 * Enables 2-10x model compression with minimal accuracy loss.
 */

#ifndef EIF_NN_PRUNING_H
#define EIF_NN_PRUNING_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Pruning Statistics
// =============================================================================

/**
 * @brief Pruning statistics
 */
typedef struct {
  int total_weights;
  int zero_weights;
  int non_zero_weights;
  float sparsity;          ///< Fraction of zero weights
  float compression_ratio; ///< Original / compressed size
} eif_pruning_stats_t;

/**
 * @brief Calculate pruning statistics
 */
static inline void eif_pruning_stats(const float *weights, int size,
                                     eif_pruning_stats_t *stats) {
  stats->total_weights = size;
  stats->zero_weights = 0;

  for (int i = 0; i < size; i++) {
    if (weights[i] == 0.0f) {
      stats->zero_weights++;
    }
  }

  stats->non_zero_weights = size - stats->zero_weights;
  stats->sparsity = (float)stats->zero_weights / size;

  // Compression assumes CSR format: non_zero values + indices
  // Dense: size * 4 bytes
  // Sparse: non_zero * (4 + 4) bytes + overhead
  if (stats->non_zero_weights > 0) {
    float dense_size = size * 4.0f;
    float sparse_size = stats->non_zero_weights * 8.0f + 16.0f;
    stats->compression_ratio = dense_size / sparse_size;
  } else {
    stats->compression_ratio = size; // All zeros
  }
}

// =============================================================================
// Magnitude Pruning
// =============================================================================

/**
 * @brief Find threshold for target sparsity (magnitude-based)
 */
static inline float eif_prune_find_threshold(const float *weights, int size,
                                             float target_sparsity) {
  // Simple approximation: find percentile of absolute values
  // For exact, would need to sort all values

  float max_abs = 0.0f;
  float sum_abs = 0.0f;

  for (int i = 0; i < size; i++) {
    float abs_val = fabsf(weights[i]);
    if (abs_val > max_abs)
      max_abs = abs_val;
    sum_abs += abs_val;
  }

  // Estimate threshold as fraction of mean
  float mean_abs = sum_abs / size;

  // Iterative approximation
  float threshold = mean_abs * target_sparsity * 2.0f;

  for (int iter = 0; iter < 10; iter++) {
    int count = 0;
    for (int i = 0; i < size; i++) {
      if (fabsf(weights[i]) < threshold)
        count++;
    }

    float current_sparsity = (float)count / size;

    if (fabsf(current_sparsity - target_sparsity) < 0.01f)
      break;

    if (current_sparsity < target_sparsity) {
      threshold *= 1.1f;
    } else {
      threshold *= 0.9f;
    }
  }

  return threshold;
}

/**
 * @brief Apply magnitude pruning (in-place)
 */
static inline int eif_prune_magnitude(float *weights, int size,
                                      float threshold) {
  int pruned = 0;

  for (int i = 0; i < size; i++) {
    if (fabsf(weights[i]) < threshold) {
      weights[i] = 0.0f;
      pruned++;
    }
  }

  return pruned;
}

/**
 * @brief Prune to target sparsity
 */
static inline int eif_prune_to_sparsity(float *weights, int size,
                                        float target_sparsity) {
  float threshold = eif_prune_find_threshold(weights, size, target_sparsity);
  return eif_prune_magnitude(weights, size, threshold);
}

// =============================================================================
// Structured Pruning (Channels/Filters)
// =============================================================================

/**
 * @brief Calculate L1 norm of a channel/filter
 */
static inline float eif_channel_l1_norm(const float *weights,
                                        int channel_size) {
  float norm = 0.0f;
  for (int i = 0; i < channel_size; i++) {
    norm += fabsf(weights[i]);
  }
  return norm;
}

/**
 * @brief Calculate L2 norm of a channel/filter
 */
static inline float eif_channel_l2_norm(const float *weights,
                                        int channel_size) {
  float norm = 0.0f;
  for (int i = 0; i < channel_size; i++) {
    norm += weights[i] * weights[i];
  }
  return sqrtf(norm);
}

/**
 * @brief Find channels to prune based on L1 norm
 * @param norms Array to store channel norms [num_channels]
 * @param prune_mask Boolean array indicating channels to prune [num_channels]
 * @param num_channels Number of channels/filters
 * @param prune_ratio Fraction of channels to remove
 */
static inline int eif_find_channels_to_prune(const float *norms,
                                             bool *prune_mask, int num_channels,
                                             float prune_ratio) {
  int to_prune = (int)(num_channels * prune_ratio);

  // Initialize mask
  for (int i = 0; i < num_channels; i++) {
    prune_mask[i] = false;
  }

  // Find smallest norm channels
  for (int p = 0; p < to_prune; p++) {
    float min_norm = 1e30f;
    int min_idx = -1;

    for (int i = 0; i < num_channels; i++) {
      if (!prune_mask[i] && norms[i] < min_norm) {
        min_norm = norms[i];
        min_idx = i;
      }
    }

    if (min_idx >= 0) {
      prune_mask[min_idx] = true;
    }
  }

  return to_prune;
}

// =============================================================================
// Sparse Matrix Format (CSR-like)
// =============================================================================

/**
 * @brief Compressed sparse row format for inference
 */
typedef struct {
  float *values;    ///< Non-zero values
  int32_t *col_idx; ///< Column indices
  int32_t *row_ptr; ///< Row pointers
  int num_rows;
  int num_cols;
  int nnz; ///< Number of non-zeros
} eif_sparse_matrix_t;

/**
 * @brief Convert dense matrix to sparse (CSR)
 */
static inline bool eif_dense_to_sparse(const float *dense, int rows, int cols,
                                       eif_sparse_matrix_t *sparse) {
  // Count non-zeros
  int nnz = 0;
  for (int i = 0; i < rows * cols; i++) {
    if (dense[i] != 0.0f)
      nnz++;
  }

  sparse->num_rows = rows;
  sparse->num_cols = cols;
  sparse->nnz = nnz;

  // Allocate (caller should provide pre-allocated buffers in production)
  sparse->values = (float *)malloc(nnz * sizeof(float));
  sparse->col_idx = (int32_t *)malloc(nnz * sizeof(int32_t));
  sparse->row_ptr = (int32_t *)malloc((rows + 1) * sizeof(int32_t));

  if (!sparse->values || !sparse->col_idx || !sparse->row_ptr) {
    return false;
  }

  // Fill CSR
  int idx = 0;
  for (int r = 0; r < rows; r++) {
    sparse->row_ptr[r] = idx;
    for (int c = 0; c < cols; c++) {
      float val = dense[r * cols + c];
      if (val != 0.0f) {
        sparse->values[idx] = val;
        sparse->col_idx[idx] = c;
        idx++;
      }
    }
  }
  sparse->row_ptr[rows] = nnz;

  return true;
}

/**
 * @brief Sparse matrix-vector multiply: y = A * x
 */
static inline void eif_sparse_matvec(const eif_sparse_matrix_t *A,
                                     const float *x, float *y) {
  for (int r = 0; r < A->num_rows; r++) {
    float sum = 0.0f;
    for (int i = A->row_ptr[r]; i < A->row_ptr[r + 1]; i++) {
      sum += A->values[i] * x[A->col_idx[i]];
    }
    y[r] = sum;
  }
}

/**
 * @brief Free sparse matrix memory
 */
static inline void eif_sparse_free(eif_sparse_matrix_t *sparse) {
  if (sparse->values)
    free(sparse->values);
  if (sparse->col_idx)
    free(sparse->col_idx);
  if (sparse->row_ptr)
    free(sparse->row_ptr);
  sparse->values = NULL;
  sparse->col_idx = NULL;
  sparse->row_ptr = NULL;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_PRUNING_H
