/**
 * @file eif_nn_transformer.h
 * @brief Tiny Transformer Encoder for Edge AI
 *
 * Lightweight Transformer implementation:
 * - Single/multi-head self-attention
 * - Feed-forward network
 * - Layer normalization
 * - Positional encoding
 *
 * Suitable for small sequence tasks: keyword spotting, activity recognition.
 */

#ifndef EIF_NN_TRANSFORMER_H
#define EIF_NN_TRANSFORMER_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_TRANSFORMER_MAX_SEQ 64
#define EIF_TRANSFORMER_MAX_DIM 128
#define EIF_TRANSFORMER_MAX_HEADS 8
#define EIF_TRANSFORMER_MAX_FFN 256

// =============================================================================
// Transformer Configuration
// =============================================================================

/**
 * @brief Transformer encoder configuration
 */
typedef struct {
  int seq_len;   ///< Maximum sequence length
  int d_model;   ///< Model dimension
  int num_heads; ///< Number of attention heads
  int d_ff;      ///< Feed-forward hidden dimension
  float dropout; ///< Dropout rate (for training info only)
} eif_transformer_config_t;

/**
 * @brief Default tiny configuration
 */
static inline void eif_transformer_config_tiny(eif_transformer_config_t *cfg) {
  cfg->seq_len = 32;
  cfg->d_model = 32;
  cfg->num_heads = 4;
  cfg->d_ff = 64;
  cfg->dropout = 0.1f;
}

/**
 * @brief Small configuration
 */
static inline void eif_transformer_config_small(eif_transformer_config_t *cfg) {
  cfg->seq_len = 64;
  cfg->d_model = 64;
  cfg->num_heads = 4;
  cfg->d_ff = 128;
  cfg->dropout = 0.1f;
}

// =============================================================================
// Layer Normalization
// =============================================================================

/**
 * @brief Layer normalization (in-place)
 */
static inline void eif_transformer_layer_norm(float *x, int size, float eps) {
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
  float inv_std = 1.0f / sqrtf(var + eps);
  for (int i = 0; i < size; i++) {
    x[i] = (x[i] - mean) * inv_std;
  }
}

/**
 * @brief Layer norm with learned scale and bias
 */
static inline void eif_transformer_layer_norm_affine(float *x, int size,
                                                     const float *gamma,
                                                     const float *beta,
                                                     float eps) {
  eif_transformer_layer_norm(x, size, eps);

  // Apply affine transform
  for (int i = 0; i < size; i++) {
    x[i] = x[i] * gamma[i] + beta[i];
  }
}

// =============================================================================
// Positional Encoding
// =============================================================================

/**
 * @brief Add sinusoidal positional encoding
 */
static inline void eif_transformer_add_pos_encoding(float *embeddings,
                                                    int seq_len, int d_model) {
  for (int pos = 0; pos < seq_len; pos++) {
    for (int i = 0; i < d_model; i++) {
      float angle = (float)pos / powf(10000.0f, (float)(2 * (i / 2)) / d_model);
      float pe = (i % 2 == 0) ? sinf(angle) : cosf(angle);
      embeddings[pos * d_model + i] += pe;
    }
  }
}

// =============================================================================
// Attention
// =============================================================================

/**
 * @brief Softmax for attention scores
 */
static inline void eif_softmax_row(float *row, int n) {
  float max_val = row[0];
  for (int i = 1; i < n; i++) {
    if (row[i] > max_val)
      max_val = row[i];
  }

  float sum = 0.0f;
  for (int i = 0; i < n; i++) {
    row[i] = expf(row[i] - max_val);
    sum += row[i];
  }

  for (int i = 0; i < n; i++) {
    row[i] /= sum;
  }
}

/**
 * @brief Single-head scaled dot-product attention
 *
 * @param Q Query [seq_len x d_k]
 * @param K Key [seq_len x d_k]
 * @param V Value [seq_len x d_k]
 * @param output [seq_len x d_k]
 */
static inline void eif_attention_head(const float *Q, const float *K,
                                      const float *V, float *output,
                                      int seq_len, int d_k) {
  float scale = 1.0f / sqrtf((float)d_k);
  float scores[EIF_TRANSFORMER_MAX_SEQ];

  for (int i = 0; i < seq_len; i++) {
    // Compute attention scores for row i
    for (int j = 0; j < seq_len; j++) {
      float dot = 0.0f;
      for (int k = 0; k < d_k; k++) {
        dot += Q[i * d_k + k] * K[j * d_k + k];
      }
      scores[j] = dot * scale;
    }

    // Softmax
    eif_softmax_row(scores, seq_len);

    // Weighted sum of values
    for (int k = 0; k < d_k; k++) {
      float sum = 0.0f;
      for (int j = 0; j < seq_len; j++) {
        sum += scores[j] * V[j * d_k + k];
      }
      output[i * d_k + k] = sum;
    }
  }
}

/**
 * @brief Multi-head attention
 *
 * Projects input to Q, K, V for each head, applies attention, concatenates.
 */
typedef struct {
  int num_heads;
  int d_model;
  int d_k; ///< d_model / num_heads

  // Projection weights (should be set externally)
  float *Wq; ///< [d_model x d_model]
  float *Wk; ///< [d_model x d_model]
  float *Wv; ///< [d_model x d_model]
  float *Wo; ///< [d_model x d_model]
} eif_mha_t;

/**
 * @brief Initialize multi-head attention
 */
static inline void eif_mha_init(eif_mha_t *mha, int num_heads, int d_model) {
  mha->num_heads = num_heads;
  mha->d_model = d_model;
  mha->d_k = d_model / num_heads;
  mha->Wq = NULL;
  mha->Wk = NULL;
  mha->Wv = NULL;
  mha->Wo = NULL;
}

// =============================================================================
// Feed-Forward Network
// =============================================================================

/**
 * @brief Feed-forward network: Linear -> ReLU -> Linear
 */
typedef struct {
  int d_model;
  int d_ff;
  float *W1; ///< [d_model x d_ff]
  float *b1; ///< [d_ff]
  float *W2; ///< [d_ff x d_model]
  float *b2; ///< [d_model]
} eif_ffn_t;

/**
 * @brief Initialize FFN
 */
static inline void eif_ffn_init(eif_ffn_t *ffn, int d_model, int d_ff) {
  ffn->d_model = d_model;
  ffn->d_ff = d_ff;
  ffn->W1 = NULL;
  ffn->b1 = NULL;
  ffn->W2 = NULL;
  ffn->b2 = NULL;
}

/**
 * @brief FFN forward pass for single position
 */
static inline void eif_ffn_forward(eif_ffn_t *ffn, const float *input,
                                   float *output, float *scratch) {
  // Linear 1 + ReLU
  for (int i = 0; i < ffn->d_ff; i++) {
    float sum = ffn->b1[i];
    for (int j = 0; j < ffn->d_model; j++) {
      sum += ffn->W1[j * ffn->d_ff + i] * input[j];
    }
    scratch[i] = sum > 0.0f ? sum : 0.0f; // ReLU
  }

  // Linear 2
  for (int i = 0; i < ffn->d_model; i++) {
    float sum = ffn->b2[i];
    for (int j = 0; j < ffn->d_ff; j++) {
      sum += ffn->W2[j * ffn->d_model + i] * scratch[j];
    }
    output[i] = sum;
  }
}

// =============================================================================
// Transformer Encoder Block
// =============================================================================

/**
 * @brief Complete Transformer encoder block
 */
typedef struct {
  eif_transformer_config_t config;
  eif_mha_t mha;
  eif_ffn_t ffn;

  // Layer norm parameters
  float *ln1_gamma;
  float *ln1_beta;
  float *ln2_gamma;
  float *ln2_beta;
} eif_transformer_block_t;

/**
 * @brief Initialize encoder block
 */
static inline void eif_transformer_block_init(eif_transformer_block_t *block,
                                              eif_transformer_config_t *cfg) {
  block->config = *cfg;
  eif_mha_init(&block->mha, cfg->num_heads, cfg->d_model);
  eif_ffn_init(&block->ffn, cfg->d_model, cfg->d_ff);

  block->ln1_gamma = NULL;
  block->ln1_beta = NULL;
  block->ln2_gamma = NULL;
  block->ln2_beta = NULL;
}

/**
 * @brief Estimate memory for transformer block
 */
static inline int eif_transformer_block_memory(eif_transformer_config_t *cfg) {
  int mha_params = 4 * cfg->d_model * cfg->d_model; // Wq, Wk, Wv, Wo
  int ffn_params = cfg->d_model * cfg->d_ff * 2;    // W1, W2
  int ln_params = cfg->d_model * 4;                 // gamma, beta x2

  return (mha_params + ffn_params + ln_params) * sizeof(float);
}

/**
 * @brief Estimate total model memory
 */
static inline int eif_transformer_model_memory(eif_transformer_config_t *cfg,
                                               int num_layers, int vocab_size,
                                               int num_classes) {
  int block_mem = eif_transformer_block_memory(cfg) * num_layers;
  int embed_mem = vocab_size * cfg->d_model * sizeof(float);
  int output_mem = cfg->d_model * num_classes * sizeof(float);
  int activation_mem = cfg->seq_len * cfg->d_model * sizeof(float) * 4;

  return block_mem + embed_mem + output_mem + activation_mem;
}

// =============================================================================
// Classifier Head
// =============================================================================

/**
 * @brief Classification head (mean pooling + linear)
 */
static inline void eif_transformer_classify(const float *hidden_states,
                                            int seq_len, int d_model,
                                            const float *Wc, const float *bc,
                                            int num_classes, float *output) {
  // Mean pooling over sequence
  float pooled[EIF_TRANSFORMER_MAX_DIM] = {0};

  for (int i = 0; i < seq_len; i++) {
    for (int j = 0; j < d_model; j++) {
      pooled[j] += hidden_states[i * d_model + j];
    }
  }

  for (int j = 0; j < d_model; j++) {
    pooled[j] /= seq_len;
  }

  // Linear projection to classes
  for (int c = 0; c < num_classes; c++) {
    float sum = bc[c];
    for (int j = 0; j < d_model; j++) {
      sum += Wc[j * num_classes + c] * pooled[j];
    }
    output[c] = sum;
  }
}

/**
 * @brief Softmax for classification output
 */
static inline void eif_transformer_softmax(float *logits, int num_classes) {
  float max_val = logits[0];
  for (int i = 1; i < num_classes; i++) {
    if (logits[i] > max_val)
      max_val = logits[i];
  }

  float sum = 0.0f;
  for (int i = 0; i < num_classes; i++) {
    logits[i] = expf(logits[i] - max_val);
    sum += logits[i];
  }

  for (int i = 0; i < num_classes; i++) {
    logits[i] /= sum;
  }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_TRANSFORMER_H
