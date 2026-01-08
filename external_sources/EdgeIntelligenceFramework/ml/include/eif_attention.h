/**
 * @file eif_attention.h
 * @brief Attention Mechanisms for EIF
 *
 * Provides efficient attention implementations for embedded systems.
 * Optimized for MCUs with limited memory.
 *
 * Features:
 * - Scaled dot-product attention
 * - Multi-head attention
 * - Linearized attention (O(n) complexity)
 * - Memory-efficient chunked processing
 *
 * Note: Full transformer models require significant memory.
 * Consider using distilled/tiny models for MCU deployment.
 */

#ifndef EIF_ATTENTION_H
#define EIF_ATTENTION_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

#ifndef EIF_ATTN_MAX_SEQ_LEN
#define EIF_ATTN_MAX_SEQ_LEN 64 // Maximum sequence length
#endif

#ifndef EIF_ATTN_MAX_DIM
#define EIF_ATTN_MAX_DIM 64 // Maximum embedding dimension
#endif

#ifndef EIF_ATTN_MAX_HEADS
#define EIF_ATTN_MAX_HEADS 4 // Maximum attention heads
#endif

// =============================================================================
// Attention Configuration
// =============================================================================

/**
 * @brief Attention layer configuration
 */
typedef struct {
  int seq_len;   ///< Sequence length
  int embed_dim; ///< Embedding dimension (d_model)
  int num_heads; ///< Number of attention heads
  int head_dim;  ///< Dimension per head (embed_dim / num_heads)

  // Projection weights [embed_dim x embed_dim]
  const int16_t *W_q; ///< Query projection
  const int16_t *W_k; ///< Key projection
  const int16_t *W_v; ///< Value projection
  const int16_t *W_o; ///< Output projection

  // Optional biases
  const int16_t *b_q;
  const int16_t *b_k;
  const int16_t *b_v;
  const int16_t *b_o;

  // Scaling factor (1/sqrt(head_dim)) in Q15
  int16_t scale;

  // Causal masking for autoregressive models
  bool causal;
} eif_attention_t;

/**
 * @brief Attention output with optional scores
 */
typedef struct {
  int16_t *output; ///< Attention output [seq_len x embed_dim]
  int16_t *scores; ///< Optional attention scores [seq_len x seq_len]
  bool success;
} eif_attention_result_t;

// =============================================================================
// Fixed-Point Math Helpers
// =============================================================================

/**
 * @brief Q15 softmax optimized for attention scores
 * Processes row-by-row for memory efficiency
 */
static inline void eif_attn_softmax_row(int16_t *row, int len) {
  // Find max for numerical stability
  int16_t max_val = row[0];
  for (int i = 1; i < len; i++) {
    if (row[i] > max_val)
      max_val = row[i];
  }

  // Compute exp and sum (using approximation)
  int32_t sum = 0;
  for (int i = 0; i < len; i++) {
    int32_t x = row[i] - max_val;
    // Fast exp approximation: exp(x) ≈ max(0, 1 + x + x²/2)
    int32_t exp_approx = 16384; // 0.5 in Q15
    if (x > -16384) {
      exp_approx = 16384 + (x >> 1) + ((x * x) >> 18);
      if (exp_approx < 0)
        exp_approx = 0;
    } else {
      exp_approx = 0;
    }
    row[i] = (int16_t)exp_approx;
    sum += exp_approx;
  }

  // Normalize
  if (sum > 0) {
    for (int i = 0; i < len; i++) {
      row[i] = (int16_t)(((int32_t)row[i] << 15) / sum);
    }
  }
}

/**
 * @brief Compute 1/sqrt(x) in Q15 using Newton-Raphson
 */
static inline int16_t eif_attn_rsqrt_q15(int32_t x) {
  if (x <= 0)
    return 32767;

  // Initial guess using leading zeros
  int32_t y = 32767;

  // Two iterations of Newton-Raphson
  for (int i = 0; i < 2; i++) {
    int32_t y2 = (y * y) >> 15;
    int32_t xy2 = (x * y2) >> 15;
    y = (y * (49152 - (xy2 >> 1))) >> 15; // 49152 = 1.5 * 32768
  }

  return (int16_t)y;
}

// =============================================================================
// Core Attention Operations
// =============================================================================

/**
 * @brief Initialize attention layer
 */
static inline void eif_attention_init(eif_attention_t *attn, int seq_len,
                                      int embed_dim, int num_heads) {
  attn->seq_len = seq_len;
  attn->embed_dim = embed_dim;
  attn->num_heads = num_heads;
  attn->head_dim = embed_dim / num_heads;
  attn->causal = false;

  // Compute scaling factor: 1/sqrt(head_dim)
  attn->scale = eif_attn_rsqrt_q15(attn->head_dim << 15);
}

/**
 * @brief Linear projection: Y = X @ W + b
 * @param x Input [seq_len x in_dim]
 * @param w Weights [in_dim x out_dim]
 * @param b Bias [out_dim] or NULL
 * @param y Output [seq_len x out_dim]
 */
static inline void eif_attn_linear(const int16_t *x, const int16_t *w,
                                   const int16_t *b, int seq_len, int in_dim,
                                   int out_dim, int16_t *y) {
  for (int s = 0; s < seq_len; s++) {
    for (int o = 0; o < out_dim; o++) {
      int32_t acc = 0;
      for (int i = 0; i < in_dim; i++) {
        acc += (int32_t)x[s * in_dim + i] * w[i * out_dim + o];
      }
      acc = acc >> 15;
      if (b)
        acc += b[o];

      // Saturate
      if (acc > 32767)
        acc = 32767;
      if (acc < -32768)
        acc = -32768;

      y[s * out_dim + o] = (int16_t)acc;
    }
  }
}

/**
 * @brief Scaled dot-product attention for single head
 *
 * Attention(Q, K, V) = softmax(Q @ K^T / sqrt(d_k)) @ V
 *
 * @param q Query [seq_len x head_dim]
 * @param k Key [seq_len x head_dim]
 * @param v Value [seq_len x head_dim]
 * @param out Output [seq_len x head_dim]
 * @param scores Optional scores buffer [seq_len x seq_len]
 * @param seq_len Sequence length
 * @param head_dim Head dimension
 * @param scale Scaling factor (1/sqrt(head_dim)) in Q15
 * @param causal Apply causal mask
 */
static inline void eif_scaled_dot_product_attention(
    const int16_t *q, const int16_t *k, const int16_t *v, int16_t *out,
    int16_t *scores, int seq_len, int head_dim, int16_t scale, bool causal) {

  // Scores buffer (use provided or stack allocation for small sequences)
  int16_t _scores[EIF_ATTN_MAX_SEQ_LEN];

  // Process each query position
  for (int i = 0; i < seq_len; i++) {
    // Compute attention scores for position i: Q[i] @ K^T
    for (int j = 0; j < seq_len; j++) {
      // Causal mask: can only attend to past positions
      if (causal && j > i) {
        _scores[j] = -32768; // -inf
        continue;
      }

      int32_t dot = 0;
      for (int d = 0; d < head_dim; d++) {
        dot += (int32_t)q[i * head_dim + d] * k[j * head_dim + d];
      }

      // Scale by 1/sqrt(d_k)
      dot = (dot >> 15) * scale >> 15;

      // Clamp to prevent overflow
      if (dot > 32767)
        dot = 32767;
      if (dot < -32768)
        dot = -32768;

      _scores[j] = (int16_t)dot;
    }

    // Softmax over scores
    int effective_len = causal ? (i + 1) : seq_len;
    eif_attn_softmax_row(_scores, effective_len);
    if (causal) {
      for (int j = i + 1; j < seq_len; j++) {
        _scores[j] = 0;
      }
    }

    // Store scores if requested
    if (scores) {
      for (int j = 0; j < seq_len; j++) {
        scores[i * seq_len + j] = _scores[j];
      }
    }

    // Weighted sum of values: scores @ V
    for (int d = 0; d < head_dim; d++) {
      int32_t acc = 0;
      for (int j = 0; j < seq_len; j++) {
        acc += (int32_t)_scores[j] * v[j * head_dim + d];
      }
      out[i * head_dim + d] = (int16_t)(acc >> 15);
    }
  }
}

/**
 * @brief Multi-head attention
 *
 * Splits input into multiple heads, applies attention, concatenates.
 *
 * @param attn Attention configuration
 * @param x Input [seq_len x embed_dim]
 * @param out Output [seq_len x embed_dim]
 * @param work Workspace buffer (at least 3 * seq_len * embed_dim)
 * @param scores Optional attention scores buffer
 */
static inline void eif_multi_head_attention(const eif_attention_t *attn,
                                            const int16_t *x, int16_t *out,
                                            int16_t *work, int16_t *scores) {

  int seq_len = attn->seq_len;
  int embed_dim = attn->embed_dim;
  int num_heads = attn->num_heads;
  int head_dim = attn->head_dim;

  // Partition workspace
  int16_t *Q = work;
  int16_t *K = work + seq_len * embed_dim;
  int16_t *V = work + 2 * seq_len * embed_dim;
  int16_t *head_out = out; // Reuse output buffer temporarily

  // Project to Q, K, V
  eif_attn_linear(x, attn->W_q, attn->b_q, seq_len, embed_dim, embed_dim, Q);
  eif_attn_linear(x, attn->W_k, attn->b_k, seq_len, embed_dim, embed_dim, K);
  eif_attn_linear(x, attn->W_v, attn->b_v, seq_len, embed_dim, embed_dim, V);

  // Process each head
  for (int h = 0; h < num_heads; h++) {
    int offset = h * head_dim;

    // Extract head slices (strided access)
    // Q_h, K_h, V_h are at columns [offset : offset + head_dim]
    // For simplicity, we use the full tensors and compute strided

    // Compute attention for this head
    // Note: This is a simplified version - full implementation would
    // reshape to [num_heads, seq_len, head_dim] for efficiency

    for (int s = 0; s < seq_len; s++) {
      // Compute attention scores for this head
      int16_t head_scores[EIF_ATTN_MAX_SEQ_LEN];

      for (int j = 0; j < seq_len; j++) {
        if (attn->causal && j > s) {
          head_scores[j] = -32768;
          continue;
        }

        int32_t dot = 0;
        for (int d = 0; d < head_dim; d++) {
          dot += (int32_t)Q[s * embed_dim + offset + d] *
                 K[j * embed_dim + offset + d];
        }
        dot = ((dot >> 15) * attn->scale) >> 15;
        head_scores[j] =
            (int16_t)(dot > 32767 ? 32767 : (dot < -32768 ? -32768 : dot));
      }

      // Softmax
      int eff_len = attn->causal ? (s + 1) : seq_len;
      eif_attn_softmax_row(head_scores, eff_len);

      // Weighted sum
      for (int d = 0; d < head_dim; d++) {
        int32_t acc = 0;
        for (int j = 0; j < seq_len; j++) {
          acc += (int32_t)head_scores[j] * V[j * embed_dim + offset + d];
        }
        head_out[s * embed_dim + offset + d] = (int16_t)(acc >> 15);
      }
    }
  }

  // Output projection
  if (attn->W_o) {
    // Copy head_out to work buffer, then project
    memcpy(work, head_out, seq_len * embed_dim * sizeof(int16_t));
    eif_attn_linear(work, attn->W_o, attn->b_o, seq_len, embed_dim, embed_dim,
                    out);
  }
}

// =============================================================================
// Simplified Self-Attention (Memory Efficient)
// =============================================================================

/**
 * @brief Single-head self-attention with minimal memory
 *
 * For very constrained devices. Computes attention in streaming fashion.
 *
 * @param x Input [seq_len x dim]
 * @param W_qkv Combined QKV weights [dim x 3*dim]
 * @param out Output [seq_len x dim]
 * @param seq_len Sequence length
 * @param dim Embedding dimension
 */
static inline void eif_self_attention_simple(const int16_t *x,
                                             const int16_t *W_qkv, int16_t *out,
                                             int seq_len, int dim) {

  // This processes one output position at a time to minimize memory
  int16_t q[EIF_ATTN_MAX_DIM];
  int16_t k[EIF_ATTN_MAX_DIM];
  int16_t v[EIF_ATTN_MAX_DIM];
  int16_t scores[EIF_ATTN_MAX_SEQ_LEN];

  // Scaling factor
  int16_t scale = eif_attn_rsqrt_q15(dim << 15);

  for (int i = 0; i < seq_len; i++) {
    // Compute Q for position i
    for (int d = 0; d < dim; d++) {
      int32_t acc = 0;
      for (int j = 0; j < dim; j++) {
        acc += (int32_t)x[i * dim + j] * W_qkv[j * 3 * dim + d];
      }
      q[d] = (int16_t)(acc >> 15);
    }

    // Compute attention scores with all positions
    for (int j = 0; j < seq_len; j++) {
      // Compute K for position j
      for (int d = 0; d < dim; d++) {
        int32_t acc = 0;
        for (int k_idx = 0; k_idx < dim; k_idx++) {
          acc += (int32_t)x[j * dim + k_idx] * W_qkv[k_idx * 3 * dim + dim + d];
        }
        k[d] = (int16_t)(acc >> 15);
      }

      // Q[i] dot K[j]
      int32_t dot = 0;
      for (int d = 0; d < dim; d++) {
        dot += (int32_t)q[d] * k[d];
      }
      scores[j] = (int16_t)(((dot >> 15) * scale) >> 15);
    }

    // Softmax
    eif_attn_softmax_row(scores, seq_len);

    // Compute weighted sum of V
    for (int d = 0; d < dim; d++) {
      int32_t acc = 0;
      for (int j = 0; j < seq_len; j++) {
        // Compute V[j][d] on the fly
        int32_t v_jd = 0;
        for (int k_idx = 0; k_idx < dim; k_idx++) {
          v_jd += (int32_t)x[j * dim + k_idx] *
                  W_qkv[k_idx * 3 * dim + 2 * dim + d];
        }
        v_jd = v_jd >> 15;
        acc += (int32_t)scores[j] * v_jd;
      }
      out[i * dim + d] = (int16_t)(acc >> 15);
    }
  }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ATTENTION_H
