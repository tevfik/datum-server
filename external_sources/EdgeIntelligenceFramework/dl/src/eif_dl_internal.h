#ifndef EIF_NEURAL_INTERNAL_H
#define EIF_NEURAL_INTERNAL_H

#include "eif_neural.h"
#include "eif_model.h"
#include <math.h>
#include <string.h>

// --- Layer Implementations ---
void eif_layer_dense(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size);

// SIMD Optimized Layers
void eif_conv2d_simd(const eif_layer_t* layer, const float32_t* input, float32_t* output, 
                     int in_h, int in_w, int in_c, int out_h, int out_w, int out_c);

eif_status_t eif_layer_conv2d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);
void eif_layer_depthwise_conv2d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);
void eif_layer_maxpool2d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_h, int in_w, int in_c, int* out_h, int* out_w);
void eif_layer_avgpool2d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_h, int in_w, int in_c, int* out_h, int* out_w);
void eif_layer_global_avgpool2d(const float32_t* input, float32_t* output, int in_h, int in_w, int in_c);
void eif_layer_add(const float32_t* input1, const float32_t* input2, float32_t* output, int size);
void eif_layer_multiply(const float32_t* input1, const float32_t* input2, float32_t* output, int size);
void eif_layer_layer_norm(const eif_layer_t* layer, const float32_t* input, float32_t* output, int h, int w, int c);
void eif_layer_gelu(const float32_t* input, float32_t* output, int size);
void eif_layer_hard_swish(const float32_t* input, float32_t* output, int size);
void eif_layer_conv1d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_w, int in_c, int* out_w, int* out_c);
void eif_layer_transpose_conv2d(const eif_layer_t* layer, const float32_t* input, float32_t* output, int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);

// Quantization Layers
void eif_layer_quantize(const eif_layer_t* layer, const float32_t* input, int8_t* output, int size);
void eif_layer_dequantize(const eif_layer_t* layer, const int8_t* input, float32_t* output, int size);
void eif_layer_conv2d_int8(const eif_layer_t* layer, const int8_t* input, int8_t* output, 
                           int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);
void eif_layer_depthwise_conv2d_int8(const eif_layer_t* layer, const int8_t* input, int8_t* output,
                                     int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);
void eif_layer_dense_int8(const eif_layer_t* layer, const int8_t* input, int8_t* output, int input_size);

// Im2Col Implementation
void eif_layer_conv2d_im2col(const eif_layer_t* layer, const float32_t* input, float32_t* output, 
                             int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);

// --- Activation Implementations ---
void eif_layer_softmax(const float32_t* input, float32_t* output, int size);
void eif_layer_sigmoid(const float32_t* input, float32_t* output, int size);
void eif_layer_tanh(const float32_t* input, float32_t* output, int size);
void eif_layer_relu(const float32_t* input, float32_t* output, int size);
void eif_layer_relu6(const float32_t* input, float32_t* output, int size);
void eif_layer_leaky_relu(const eif_layer_t* layer, const float32_t* input, float32_t* output, int size);
void eif_layer_clip(const eif_layer_t* layer, const float32_t* input, float32_t* output, int size);

// --- Shape Operations ---
void eif_layer_flatten(const float32_t* input, float32_t* output, int size);
void eif_layer_reshape(const float32_t* input, float32_t* output, int size);

// --- RNN Implementations ---
void eif_layer_rnn(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size, float32_t* state);
void eif_layer_lstm(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size, float32_t* state);
void eif_layer_gru(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size, float32_t* state);

eif_status_t eif_layer_batch_norm(const float32_t* input, float32_t* output, const eif_tensor_shape_t* shape, const eif_layer_param_t* param, const float32_t* mean, const float32_t* var, const float32_t* gamma, const float32_t* beta);
eif_status_t eif_layer_resize(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param);
eif_status_t eif_layer_dropout(const float32_t* input, float32_t* output, const eif_tensor_shape_t* shape, const eif_layer_param_t* param);

// ... existing code ...

// --- Advanced Layers ---

// Self-Attention (Scaled Dot-Product)
// Input: Query (B, S, E), Key (B, S, E), Value (B, S, E)
// Output: (B, S, E)
// Note: Simplified for now, assumes single head or pre-projected
eif_status_t eif_layer_attention(const float32_t* query, const float32_t* key, const float32_t* value, float32_t* output, int batch, int seq_len, int embed_dim);

// Embedding
// Input: Indices (B, S) (int32 or float32 treated as int)
// Weights: (Vocab, Embed)
// Output: (B, S, Embed)
eif_status_t eif_layer_embedding(const float32_t* input, float32_t* output, const float32_t* weights, int batch, int seq_len, int vocab_size, int embed_dim);

// --- New Operators (Priority 1, 2, 3) ---
void eif_layer_sub(const float32_t* input1, const float32_t* input2, float32_t* output, int size);
void eif_layer_div(const float32_t* input1, const float32_t* input2, float32_t* output, int size);
void eif_layer_exp(const float32_t* input, float32_t* output, int size);
void eif_layer_log(const float32_t* input, float32_t* output, int size);
void eif_layer_sqrt(const float32_t* input, float32_t* output, int size);

eif_status_t eif_layer_split(const float32_t* input, void** outputs, const eif_tensor_shape_t* in_shape, const eif_layer_param_t* param, int num_outputs);
eif_status_t eif_layer_pad(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param);
eif_status_t eif_layer_gather(const float32_t* input, const float32_t* indices, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param);
eif_status_t eif_layer_matmul(const float32_t* a, const float32_t* b, float32_t* output, int M, int K, int N);
eif_status_t eif_layer_reduce_mean(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param);
eif_status_t eif_layer_reduce_sum(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param);
eif_status_t eif_layer_topk(const float32_t* input, float32_t* values, float32_t* indices, const eif_tensor_shape_t* in_shape, const eif_layer_param_t* param);

eif_status_t eif_layer_argmax(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_layer_param_t* param);
void eif_layer_minimum(const float32_t* input1, const float32_t* input2, float32_t* output, int size);
void eif_layer_maximum(const float32_t* input1, const float32_t* input2, float32_t* output, int size);

#endif // EIF_NEURAL_INTERNAL_H
