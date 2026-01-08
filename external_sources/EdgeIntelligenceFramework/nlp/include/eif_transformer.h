/**
 * @file eif_transformer.h
 * @brief Tiny Transformer for Edge AI
 * 
 * Lightweight transformer implementation for embedded NLP/classification:
 * - Multi-head self-attention
 * - Feedforward layers
 * - Layer normalization (optional)
 * - INT8 quantization support
 * 
 * Target: Intent classification, sentiment analysis, NER
 * Memory: 1-10MB model size
 * Hardware: Cortex-M7+ (512KB+ RAM)
 */

#ifndef EIF_TRANSFORMER_H
#define EIF_TRANSFORMER_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constants
// =============================================================================

#define EIF_TRANSFORMER_MAX_LAYERS    8
#define EIF_TRANSFORMER_MAX_HEADS     8
#define EIF_TRANSFORMER_MAX_SEQ_LEN   128
#define EIF_TRANSFORMER_MAX_DIM       256

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Attention configuration
 */
typedef struct {
    int num_heads;
    int embed_dim;
    int head_dim;        /**< embed_dim / num_heads */
    float32_t dropout;   /**< Dropout rate (training only) */
} eif_attention_config_t;

/**
 * @brief Multi-head attention weights
 */
typedef struct {
    float32_t* wq;       /**< Query weights [embed_dim, embed_dim] */
    float32_t* wk;       /**< Key weights [embed_dim, embed_dim] */
    float32_t* wv;       /**< Value weights [embed_dim, embed_dim] */
    float32_t* wo;       /**< Output projection [embed_dim, embed_dim] */
    
    // INT8 quantized versions (optional)
    int8_t* wq_int8;
    int8_t* wk_int8;
    int8_t* wv_int8;
    int8_t* wo_int8;
    float32_t scale_q, scale_k, scale_v, scale_o;
    
    bool is_quantized;
} eif_attention_weights_t;

/**
 * @brief Feedforward network weights
 */
typedef struct {
    float32_t* w1;       /**< First linear [embed_dim, ff_dim] */
    float32_t* w2;       /**< Second linear [ff_dim, embed_dim] */
    float32_t* b1;       /**< Bias 1 [ff_dim] */
    float32_t* b2;       /**< Bias 2 [embed_dim] */
    int ff_dim;
    
    // INT8 versions
    int8_t* w1_int8;
    int8_t* w2_int8;
    float32_t scale_w1, scale_w2;
    bool is_quantized;
} eif_ffn_weights_t;

/**
 * @brief Transformer layer
 */
typedef struct {
    eif_attention_weights_t attention;
    eif_ffn_weights_t ffn;
    
    // Layer norm weights
    float32_t* ln1_gamma;  /**< Pre-attention layer norm */
    float32_t* ln1_beta;
    float32_t* ln2_gamma;  /**< Pre-FFN layer norm */
    float32_t* ln2_beta;
    
    bool use_layer_norm;
} eif_transformer_layer_t;

/**
 * @brief Transformer model
 */
typedef struct {
    // Configuration
    int num_layers;
    int embed_dim;
    int num_heads;
    int ff_dim;
    int max_seq_len;
    int vocab_size;
    
    // Embeddings
    float32_t* token_embed;   /**< [vocab_size, embed_dim] */
    float32_t* pos_embed;     /**< [max_seq_len, embed_dim] */
    
    // Layers
    eif_transformer_layer_t* layers;
    
    // Classification head (optional)
    float32_t* classifier_w;  /**< [embed_dim, num_classes] */
    float32_t* classifier_b;
    int num_classes;
    
    // Scratch buffers
    float32_t* q_buf;
    float32_t* k_buf;
    float32_t* v_buf;
    float32_t* attn_buf;
    float32_t* hidden_buf;
    
    eif_memory_pool_t* pool;
} eif_transformer_t;

// =============================================================================
// Initialization
// =============================================================================

/**
 * @brief Initialize transformer model
 */
eif_status_t eif_transformer_init(eif_transformer_t* model,
                                   int num_layers,
                                   int embed_dim,
                                   int num_heads,
                                   int ff_dim,
                                   int vocab_size,
                                   int max_seq_len,
                                   eif_memory_pool_t* pool);

/**
 * @brief Load pre-trained weights from buffer
 */
eif_status_t eif_transformer_load_weights(eif_transformer_t* model,
                                           const uint8_t* buffer,
                                           size_t size);

/**
 * @brief Quantize model to INT8
 */
eif_status_t eif_transformer_quantize(eif_transformer_t* model);

// =============================================================================
// Inference
// =============================================================================

/**
 * @brief Forward pass through transformer
 * @param model Transformer model
 * @param input_ids Input token IDs [seq_len]
 * @param seq_len Sequence length
 * @param output Output hidden states [seq_len, embed_dim]
 */
eif_status_t eif_transformer_forward(eif_transformer_t* model,
                                      const int32_t* input_ids,
                                      int seq_len,
                                      float32_t* output);

/**
 * @brief Classify input sequence
 * @param model Model with classification head
 * @param input_ids Input token IDs
 * @param seq_len Sequence length
 * @param logits Output logits [num_classes]
 */
eif_status_t eif_transformer_classify(eif_transformer_t* model,
                                       const int32_t* input_ids,
                                       int seq_len,
                                       float32_t* logits);

/**
 * @brief Get embedding for input sequence (for similarity)
 */
eif_status_t eif_transformer_embed(eif_transformer_t* model,
                                    const int32_t* input_ids,
                                    int seq_len,
                                    float32_t* embedding);

// =============================================================================
// Layer Operations
// =============================================================================

/**
 * @brief Multi-head self-attention
 */
eif_status_t eif_attention_forward(const eif_attention_weights_t* weights,
                                    const float32_t* input,
                                    int seq_len,
                                    int embed_dim,
                                    int num_heads,
                                    float32_t* output,
                                    float32_t* scratch);

/**
 * @brief Feedforward network
 */
eif_status_t eif_ffn_forward(const eif_ffn_weights_t* weights,
                              const float32_t* input,
                              int seq_len,
                              int embed_dim,
                              float32_t* output);

/**
 * @brief Layer normalization
 */
void eif_layer_norm(const float32_t* input,
                    const float32_t* gamma,
                    const float32_t* beta,
                    int seq_len,
                    int dim,
                    float32_t* output);

/**
 * @brief Softmax over sequence dimension
 */
void eif_softmax(float32_t* data, int n);

/**
 * @brief GELU activation (approximate)
 */
void eif_gelu(float32_t* data, int n);

// =============================================================================
// Utilities
// =============================================================================

/**
 * @brief Get model memory requirements
 */
size_t eif_transformer_memory_required(int num_layers,
                                        int embed_dim,
                                        int num_heads,
                                        int ff_dim,
                                        int vocab_size,
                                        int max_seq_len);

/**
 * @brief Print model summary
 */
void eif_transformer_print_summary(const eif_transformer_t* model);

#ifdef __cplusplus
}
#endif

#endif // EIF_TRANSFORMER_H
