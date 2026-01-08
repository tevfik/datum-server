/**
 * @file eif_nn_attention.h
 * @brief Self-Attention Mechanism for Transformers
 *
 * Lightweight attention implementation for embedded systems:
 * - Scaled dot-product attention
 * - Multi-head attention
 * - Positional encoding
 *
 * Suitable for small NLP models and sequence processing.
 */

#ifndef EIF_NN_ATTENTION_H
#define EIF_NN_ATTENTION_H

#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum sequence length and dimensions (memory constraint)
#define EIF_ATTN_MAX_SEQ 32
#define EIF_ATTN_MAX_DIM 64
#define EIF_ATTN_MAX_HEADS 4

/**
 * @brief Softmax over a vector
 */
static inline void softmax_vec(float *x, int n) {
  float max_val = x[0];
  for (int i = 1; i < n; i++) {
    if (x[i] > max_val)
      max_val = x[i];
  }

  float sum = 0.0f;
  for (int i = 0; i < n; i++) {
    x[i] = expf(x[i] - max_val);
    sum += x[i];
  }

  for (int i = 0; i < n; i++) {
    x[i] /= sum;
  }
}

/**
 * @brief Scaled dot-product attention state
 */
typedef struct {
  int seq_len; ///< Sequence length
  int d_k;     ///< Key/Query dimension
  int d_v;     ///< Value dimension
  float scale; ///< 1 / sqrt(d_k)
} eif_attention_t;

/**
 * @brief Initialize attention
 */
static inline void eif_attention_init(eif_attention_t *attn, int seq_len,
                                      int d_k, int d_v) {
  attn->seq_len = seq_len;
  attn->d_k = d_k;
  attn->d_v = d_v;
  attn->scale = 1.0f / sqrtf((float)d_k);
}

/**
 * @brief Scaled dot-product attention
 *
 * Attention(Q, K, V) = softmax(Q * K^T / sqrt(d_k)) * V
 *
 * @param attn Attention state
 * @param Q Query matrix [seq_len x d_k]
 * @param K Key matrix [seq_len x d_k]
 * @param V Value matrix [seq_len x d_v]
 * @param output Output matrix [seq_len x d_v]
 */
static inline void eif_attention_forward(eif_attention_t *attn, const float *Q,
                                         const float *K, const float *V,
                                         float *output) {
  int seq = attn->seq_len;
  int dk = attn->d_k;
  int dv = attn->d_v;

  // Attention scores: Q * K^T
  float scores[EIF_ATTN_MAX_SEQ][EIF_ATTN_MAX_SEQ];

  for (int i = 0; i < seq; i++) {
    for (int j = 0; j < seq; j++) {
      float dot = 0.0f;
      for (int k = 0; k < dk; k++) {
        dot += Q[i * dk + k] * K[j * dk + k];
      }
      scores[i][j] = dot * attn->scale;
    }
    // Apply softmax to row
    softmax_vec(scores[i], seq);
  }

  // Output: scores * V
  for (int i = 0; i < seq; i++) {
    for (int j = 0; j < dv; j++) {
      float sum = 0.0f;
      for (int k = 0; k < seq; k++) {
        sum += scores[i][k] * V[k * dv + j];
      }
      output[i * dv + j] = sum;
    }
  }
}

/**
 * @brief Multi-head attention state
 */
typedef struct {
  int num_heads;
  int d_model; ///< Model dimension
  int d_k;     ///< Per-head key dimension (d_model / num_heads)

  // Weight matrices (should be set externally)
  float *Wq, *Wk, *Wv, *Wo; // [d_model x d_model] each
} eif_multihead_attention_t;

/**
 * @brief Initialize multi-head attention
 */
static inline void eif_multihead_init(eif_multihead_attention_t *mha,
                                      int num_heads, int d_model) {
  mha->num_heads = num_heads;
  mha->d_model = d_model;
  mha->d_k = d_model / num_heads;
}

/**
 * @brief Sinusoidal position encoding
 * @param pos Position index
 * @param i Dimension index
 * @param d_model Model dimension
 * @return Position encoding value
 */
static inline float eif_position_encoding(int pos, int i, int d_model) {
  float angle = (float)pos / powf(10000.0f, (float)(2 * (i / 2)) / d_model);
  return (i % 2 == 0) ? sinf(angle) : cosf(angle);
}

/**
 * @brief Apply position encoding to embeddings
 * @param embeddings Input/output embeddings [seq_len x d_model]
 * @param seq_len Sequence length
 * @param d_model Model dimension
 */
static inline void eif_add_position_encoding(float *embeddings, int seq_len,
                                             int d_model) {
  for (int pos = 0; pos < seq_len; pos++) {
    for (int i = 0; i < d_model; i++) {
      embeddings[pos * d_model + i] += eif_position_encoding(pos, i, d_model);
    }
  }
}

/**
 * @brief Layer normalization
 */
static inline void eif_layer_norm(float *x, int size, float eps) {
  // Compute mean
  float mean = 0.0f;
  for (int i = 0; i < size; i++) {
    mean += x[i];
  }
  mean /= size;

  // Compute variance
  float var = 0.0f;
  for (int i = 0; i < size; i++) {
    float diff = x[i] - mean;
    var += diff * diff;
  }
  var /= size;

  // Normalize
  float std = sqrtf(var + eps);
  for (int i = 0; i < size; i++) {
    x[i] = (x[i] - mean) / std;
  }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_ATTENTION_H
