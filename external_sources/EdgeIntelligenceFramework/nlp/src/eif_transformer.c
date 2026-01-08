/**
 * @file eif_transformer.c
 * @brief Tiny Transformer Implementation
 */

#include "eif_transformer.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// =============================================================================
// Helper Functions
// =============================================================================

static inline float32_t fast_exp(float32_t x) {
    // Fast exp approximation for softmax
    x = 1.0f + x / 256.0f;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    return x;
}

// =============================================================================
// Activation Functions
// =============================================================================

void eif_softmax(float32_t* data, int n) {
    if (n <= 0) return;
    
    // Find max for numerical stability
    float32_t max_val = data[0];
    for (int i = 1; i < n; i++) {
        if (data[i] > max_val) max_val = data[i];
    }
    
    // Exp and sum
    float32_t sum = 0.0f;
    for (int i = 0; i < n; i++) {
        data[i] = expf(data[i] - max_val);
        sum += data[i];
    }
    
    // Normalize
    float32_t inv_sum = 1.0f / (sum + 1e-10f);
    for (int i = 0; i < n; i++) {
        data[i] *= inv_sum;
    }
}

void eif_gelu(float32_t* data, int n) {
    // GELU(x) = x * 0.5 * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    // Fast approximation: GELU(x) ≈ x * sigmoid(1.702 * x)
    for (int i = 0; i < n; i++) {
        float32_t x = data[i];
        float32_t sig = 1.0f / (1.0f + expf(-1.702f * x));
        data[i] = x * sig;
    }
}

void eif_layer_norm(const float32_t* input,
                    const float32_t* gamma,
                    const float32_t* beta,
                    int seq_len,
                    int dim,
                    float32_t* output) {
    for (int s = 0; s < seq_len; s++) {
        const float32_t* in = &input[s * dim];
        float32_t* out = &output[s * dim];
        
        // Compute mean
        float32_t mean = 0.0f;
        for (int d = 0; d < dim; d++) {
            mean += in[d];
        }
        mean /= dim;
        
        // Compute variance
        float32_t var = 0.0f;
        for (int d = 0; d < dim; d++) {
            float32_t diff = in[d] - mean;
            var += diff * diff;
        }
        var /= dim;
        
        // Normalize
        float32_t inv_std = 1.0f / sqrtf(var + 1e-5f);
        for (int d = 0; d < dim; d++) {
            float32_t normalized = (in[d] - mean) * inv_std;
            if (gamma && beta) {
                out[d] = gamma[d] * normalized + beta[d];
            } else {
                out[d] = normalized;
            }
        }
    }
}

// =============================================================================
// Matrix Operations (for Transformer)
// =============================================================================

static void matmul(const float32_t* a, const float32_t* b, float32_t* c,
                   int m, int n, int k) {
    // C[m,n] = A[m,k] x B[k,n]
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float32_t sum = 0.0f;
            for (int l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

static void matmul_transposed_b(const float32_t* a, const float32_t* b, float32_t* c,
                                 int m, int n, int k) {
    // C[m,n] = A[m,k] x B^T[n,k]
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float32_t sum = 0.0f;
            for (int l = 0; l < k; l++) {
                sum += a[i * k + l] * b[j * k + l];
            }
            c[i * n + j] = sum;
        }
    }
}

// =============================================================================
// Attention
// =============================================================================

eif_status_t eif_attention_forward(const eif_attention_weights_t* weights,
                                    const float32_t* input,
                                    int seq_len,
                                    int embed_dim,
                                    int num_heads,
                                    float32_t* output,
                                    float32_t* scratch) {
    if (!weights || !input || !output || !scratch) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int head_dim = embed_dim / num_heads;
    float32_t scale = 1.0f / sqrtf((float32_t)head_dim);
    
    // Scratch layout: Q, K, V, attn_scores
    float32_t* Q = scratch;
    float32_t* K = Q + seq_len * embed_dim;
    float32_t* V = K + seq_len * embed_dim;
    float32_t* attn = V + seq_len * embed_dim;
    
    // Compute Q, K, V projections
    matmul(input, weights->wq, Q, seq_len, embed_dim, embed_dim);
    matmul(input, weights->wk, K, seq_len, embed_dim, embed_dim);
    matmul(input, weights->wv, V, seq_len, embed_dim, embed_dim);
    
    // Multi-head attention
    float32_t* attn_out = output;  // Reuse output buffer
    memset(attn_out, 0, seq_len * embed_dim * sizeof(float32_t));
    
    for (int h = 0; h < num_heads; h++) {
        int offset = h * head_dim;
        
        // For each query position
        for (int q_pos = 0; q_pos < seq_len; q_pos++) {
            float32_t* q_vec = &Q[q_pos * embed_dim + offset];
            
            // Compute attention scores for this query
            float32_t scores[EIF_TRANSFORMER_MAX_SEQ_LEN];
            for (int k_pos = 0; k_pos < seq_len; k_pos++) {
                float32_t* k_vec = &K[k_pos * embed_dim + offset];
                
                // Dot product
                float32_t score = 0.0f;
                for (int d = 0; d < head_dim; d++) {
                    score += q_vec[d] * k_vec[d];
                }
                scores[k_pos] = score * scale;
            }
            
            // Softmax
            eif_softmax(scores, seq_len);
            
            // Weighted sum of values
            for (int v_pos = 0; v_pos < seq_len; v_pos++) {
                float32_t* v_vec = &V[v_pos * embed_dim + offset];
                float32_t weight = scores[v_pos];
                
                for (int d = 0; d < head_dim; d++) {
                    attn_out[q_pos * embed_dim + offset + d] += weight * v_vec[d];
                }
            }
        }
    }
    
    // Output projection
    float32_t* proj_out = scratch;  // Reuse scratch
    matmul(attn_out, weights->wo, proj_out, seq_len, embed_dim, embed_dim);
    memcpy(output, proj_out, seq_len * embed_dim * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

// =============================================================================
// Feedforward Network
// =============================================================================

eif_status_t eif_ffn_forward(const eif_ffn_weights_t* weights,
                              const float32_t* input,
                              int seq_len,
                              int embed_dim,
                              float32_t* output) {
    if (!weights || !input || !output) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int ff_dim = weights->ff_dim;
    
    // First linear + GELU
    for (int s = 0; s < seq_len; s++) {
        const float32_t* in = &input[s * embed_dim];
        
        // Compute hidden layer
        float32_t hidden[EIF_TRANSFORMER_MAX_DIM * 4];  // Stack allocation
        for (int j = 0; j < ff_dim; j++) {
            float32_t sum = weights->b1 ? weights->b1[j] : 0.0f;
            for (int i = 0; i < embed_dim; i++) {
                sum += in[i] * weights->w1[i * ff_dim + j];
            }
            hidden[j] = sum;
        }
        
        // GELU activation
        eif_gelu(hidden, ff_dim);
        
        // Second linear
        float32_t* out = &output[s * embed_dim];
        for (int j = 0; j < embed_dim; j++) {
            float32_t sum = weights->b2 ? weights->b2[j] : 0.0f;
            for (int i = 0; i < ff_dim; i++) {
                sum += hidden[i] * weights->w2[i * embed_dim + j];
            }
            out[j] = sum;
        }
    }
    
    return EIF_STATUS_OK;
}

// =============================================================================
// Model Initialization
// =============================================================================

eif_status_t eif_transformer_init(eif_transformer_t* model,
                                   int num_layers,
                                   int embed_dim,
                                   int num_heads,
                                   int ff_dim,
                                   int vocab_size,
                                   int max_seq_len,
                                   eif_memory_pool_t* pool) {
    if (!model || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    if (num_layers > EIF_TRANSFORMER_MAX_LAYERS ||
        num_heads > EIF_TRANSFORMER_MAX_HEADS ||
        max_seq_len > EIF_TRANSFORMER_MAX_SEQ_LEN ||
        embed_dim > EIF_TRANSFORMER_MAX_DIM) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    model->num_layers = num_layers;
    model->embed_dim = embed_dim;
    model->num_heads = num_heads;
    model->ff_dim = ff_dim;
    model->max_seq_len = max_seq_len;
    model->vocab_size = vocab_size;
    model->num_classes = 0;
    model->pool = pool;
    
    // Allocate embeddings
    model->token_embed = (float32_t*)eif_memory_alloc(pool,
        vocab_size * embed_dim * sizeof(float32_t), sizeof(float32_t));
    model->pos_embed = (float32_t*)eif_memory_alloc(pool,
        max_seq_len * embed_dim * sizeof(float32_t), sizeof(float32_t));
    
    if (!model->token_embed || !model->pos_embed) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Allocate layers
    model->layers = (eif_transformer_layer_t*)eif_memory_alloc(pool,
        num_layers * sizeof(eif_transformer_layer_t), sizeof(void*));
    if (!model->layers) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Initialize each layer
    for (int l = 0; l < num_layers; l++) {
        eif_transformer_layer_t* layer = &model->layers[l];
        
        // Attention weights
        int attn_size = embed_dim * embed_dim;
        layer->attention.wq = (float32_t*)eif_memory_alloc(pool, attn_size * sizeof(float32_t), sizeof(float32_t));
        layer->attention.wk = (float32_t*)eif_memory_alloc(pool, attn_size * sizeof(float32_t), sizeof(float32_t));
        layer->attention.wv = (float32_t*)eif_memory_alloc(pool, attn_size * sizeof(float32_t), sizeof(float32_t));
        layer->attention.wo = (float32_t*)eif_memory_alloc(pool, attn_size * sizeof(float32_t), sizeof(float32_t));
        layer->attention.is_quantized = false;
        
        // FFN weights
        layer->ffn.w1 = (float32_t*)eif_memory_alloc(pool, embed_dim * ff_dim * sizeof(float32_t), sizeof(float32_t));
        layer->ffn.w2 = (float32_t*)eif_memory_alloc(pool, ff_dim * embed_dim * sizeof(float32_t), sizeof(float32_t));
        layer->ffn.b1 = (float32_t*)eif_memory_alloc(pool, ff_dim * sizeof(float32_t), sizeof(float32_t));
        layer->ffn.b2 = (float32_t*)eif_memory_alloc(pool, embed_dim * sizeof(float32_t), sizeof(float32_t));
        layer->ffn.ff_dim = ff_dim;
        layer->ffn.is_quantized = false;
        
        // Layer norm
        layer->ln1_gamma = (float32_t*)eif_memory_alloc(pool, embed_dim * sizeof(float32_t), sizeof(float32_t));
        layer->ln1_beta = (float32_t*)eif_memory_alloc(pool, embed_dim * sizeof(float32_t), sizeof(float32_t));
        layer->ln2_gamma = (float32_t*)eif_memory_alloc(pool, embed_dim * sizeof(float32_t), sizeof(float32_t));
        layer->ln2_beta = (float32_t*)eif_memory_alloc(pool, embed_dim * sizeof(float32_t), sizeof(float32_t));
        layer->use_layer_norm = true;
        
        // Initialize layer norm to identity
        for (int i = 0; i < embed_dim; i++) {
            layer->ln1_gamma[i] = 1.0f;
            layer->ln1_beta[i] = 0.0f;
            layer->ln2_gamma[i] = 1.0f;
            layer->ln2_beta[i] = 0.0f;
        }
    }
    
    // Allocate scratch buffers
    int scratch_size = max_seq_len * embed_dim;
    model->q_buf = (float32_t*)eif_memory_alloc(pool, scratch_size * sizeof(float32_t), sizeof(float32_t));
    model->k_buf = (float32_t*)eif_memory_alloc(pool, scratch_size * sizeof(float32_t), sizeof(float32_t));
    model->v_buf = (float32_t*)eif_memory_alloc(pool, scratch_size * sizeof(float32_t), sizeof(float32_t));
    model->attn_buf = (float32_t*)eif_memory_alloc(pool, max_seq_len * max_seq_len * sizeof(float32_t), sizeof(float32_t));
    model->hidden_buf = (float32_t*)eif_memory_alloc(pool, scratch_size * sizeof(float32_t), sizeof(float32_t));
    
    // Classification head (optional, null by default)
    model->classifier_w = NULL;
    model->classifier_b = NULL;
    
    return EIF_STATUS_OK;
}

// =============================================================================
// Forward Pass
// =============================================================================

eif_status_t eif_transformer_forward(eif_transformer_t* model,
                                      const int32_t* input_ids,
                                      int seq_len,
                                      float32_t* output) {
    if (!model || !input_ids || !output || seq_len > model->max_seq_len) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int embed_dim = model->embed_dim;
    
    // Token + positional embedding
    float32_t* hidden = model->hidden_buf;
    for (int i = 0; i < seq_len; i++) {
        int token_id = input_ids[i];
        if (token_id < 0 || token_id >= model->vocab_size) {
            token_id = 0;  // Fallback to <unk>
        }
        
        for (int d = 0; d < embed_dim; d++) {
            hidden[i * embed_dim + d] = 
                model->token_embed[token_id * embed_dim + d] +
                model->pos_embed[i * embed_dim + d];
        }
    }
    
    // Transformer layers
    float32_t* layer_out = output;
    float32_t* residual = model->q_buf;  // Reuse as residual buffer
    
    // Scratch for attention: needs Q, K, V, attn_scores
    int scratch_size = 4 * seq_len * embed_dim;
    float32_t* attn_scratch = model->k_buf;  // Simplified, would need more in production
    
    for (int l = 0; l < model->num_layers; l++) {
        eif_transformer_layer_t* layer = &model->layers[l];
        
        // Save residual
        memcpy(residual, hidden, seq_len * embed_dim * sizeof(float32_t));
        
        // Pre-attention layer norm
        if (layer->use_layer_norm) {
            eif_layer_norm(hidden, layer->ln1_gamma, layer->ln1_beta,
                          seq_len, embed_dim, layer_out);
            memcpy(hidden, layer_out, seq_len * embed_dim * sizeof(float32_t));
        }
        
        // Self-attention
        eif_attention_forward(&layer->attention, hidden, seq_len, embed_dim,
                             model->num_heads, layer_out, attn_scratch);
        
        // Residual connection
        for (int i = 0; i < seq_len * embed_dim; i++) {
            hidden[i] = residual[i] + layer_out[i];
        }
        
        // Save residual for FFN
        memcpy(residual, hidden, seq_len * embed_dim * sizeof(float32_t));
        
        // Pre-FFN layer norm
        if (layer->use_layer_norm) {
            eif_layer_norm(hidden, layer->ln2_gamma, layer->ln2_beta,
                          seq_len, embed_dim, layer_out);
            memcpy(hidden, layer_out, seq_len * embed_dim * sizeof(float32_t));
        }
        
        // Feedforward
        eif_ffn_forward(&layer->ffn, hidden, seq_len, embed_dim, layer_out);
        
        // Residual connection
        for (int i = 0; i < seq_len * embed_dim; i++) {
            hidden[i] = residual[i] + layer_out[i];
        }
    }
    
    // Copy final hidden states to output
    memcpy(output, hidden, seq_len * embed_dim * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_transformer_classify(eif_transformer_t* model,
                                       const int32_t* input_ids,
                                       int seq_len,
                                       float32_t* logits) {
    if (!model || !model->classifier_w || model->num_classes <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Get hidden states
    float32_t* hidden = model->hidden_buf;
    eif_status_t status = eif_transformer_forward(model, input_ids, seq_len, hidden);
    if (status != EIF_STATUS_OK) return status;
    
    // Use [CLS] token (position 0) for classification
    float32_t* cls_hidden = hidden;  // First token
    
    // Classification linear layer
    for (int c = 0; c < model->num_classes; c++) {
        float32_t sum = model->classifier_b ? model->classifier_b[c] : 0.0f;
        for (int d = 0; d < model->embed_dim; d++) {
            sum += cls_hidden[d] * model->classifier_w[d * model->num_classes + c];
        }
        logits[c] = sum;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_transformer_embed(eif_transformer_t* model,
                                    const int32_t* input_ids,
                                    int seq_len,
                                    float32_t* embedding) {
    if (!model || !embedding) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Get hidden states
    float32_t* hidden = model->hidden_buf;
    eif_status_t status = eif_transformer_forward(model, input_ids, seq_len, hidden);
    if (status != EIF_STATUS_OK) return status;
    
    // Mean pooling over sequence
    memset(embedding, 0, model->embed_dim * sizeof(float32_t));
    for (int s = 0; s < seq_len; s++) {
        for (int d = 0; d < model->embed_dim; d++) {
            embedding[d] += hidden[s * model->embed_dim + d];
        }
    }
    
    float32_t inv_len = 1.0f / seq_len;
    for (int d = 0; d < model->embed_dim; d++) {
        embedding[d] *= inv_len;
    }
    
    return EIF_STATUS_OK;
}

// =============================================================================
// Utilities
// =============================================================================

size_t eif_transformer_memory_required(int num_layers,
                                        int embed_dim,
                                        int num_heads,
                                        int ff_dim,
                                        int vocab_size,
                                        int max_seq_len) {
    size_t size = 0;
    
    // Embeddings
    size += vocab_size * embed_dim * sizeof(float32_t);  // Token embeddings
    size += max_seq_len * embed_dim * sizeof(float32_t); // Position embeddings
    
    // Per-layer
    size_t per_layer = 0;
    per_layer += 4 * embed_dim * embed_dim * sizeof(float32_t);  // Q, K, V, O projections
    per_layer += embed_dim * ff_dim * sizeof(float32_t);         // FFN W1
    per_layer += ff_dim * embed_dim * sizeof(float32_t);         // FFN W2
    per_layer += (ff_dim + embed_dim) * sizeof(float32_t);       // Biases
    per_layer += 4 * embed_dim * sizeof(float32_t);              // Layer norm params
    size += num_layers * per_layer;
    
    // Scratch buffers
    size += 5 * max_seq_len * embed_dim * sizeof(float32_t);
    
    return size;
}

void eif_transformer_print_summary(const eif_transformer_t* model) {
    if (!model) return;
    
    size_t mem = eif_transformer_memory_required(
        model->num_layers, model->embed_dim, model->num_heads,
        model->ff_dim, model->vocab_size, model->max_seq_len);
    
    printf("\n=== Tiny Transformer Summary ===\n");
    printf("Layers:       %d\n", model->num_layers);
    printf("Embed dim:    %d\n", model->embed_dim);
    printf("Num heads:    %d\n", model->num_heads);
    printf("FF dim:       %d\n", model->ff_dim);
    printf("Vocab size:   %d\n", model->vocab_size);
    printf("Max seq len:  %d\n", model->max_seq_len);
    printf("Memory:       %.2f KB (%.2f MB)\n", 
           mem / 1024.0f, mem / (1024.0f * 1024.0f));
    printf("================================\n\n");
}
