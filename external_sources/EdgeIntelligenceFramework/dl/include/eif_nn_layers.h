/**
 * @file eif_nn_layers.h
 * @brief Neural Network Layer Types and Parameters
 * 
 * Layer type enumeration and parameter structures.
 * Previously in eif_model.h - consolidated here for consistency.
 */

#ifndef EIF_NN_LAYERS_H
#define EIF_NN_LAYERS_H

#include "eif_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Layer Types
// ============================================================================

typedef enum {
    // Core layers
    EIF_LAYER_DENSE = 0,
    EIF_LAYER_CONV2D = 1,
    EIF_LAYER_CONV1D = 21,
    EIF_LAYER_DEPTHWISE_CONV2D = 6,
    EIF_LAYER_TRANSPOSE_CONV2D = 22,
    
    // Pooling
    EIF_LAYER_MAXPOOL2D = 4,
    EIF_LAYER_AVGPOOL2D = 7,
    EIF_LAYER_GLOBAL_AVGPOOL2D = 12,
    
    // Activations
    EIF_LAYER_RELU = 2,
    EIF_LAYER_RELU6 = 30,
    EIF_LAYER_LEAKY_RELU = 17,
    EIF_LAYER_SIGMOID = 10,
    EIF_LAYER_TANH = 11,
    EIF_LAYER_SOFTMAX = 3,
    EIF_LAYER_GELU = 31,
    EIF_LAYER_HARD_SWISH = 32,
    
    // Shape operations
    EIF_LAYER_FLATTEN = 5,
    EIF_LAYER_RESHAPE = 15,
    EIF_LAYER_RESIZE = 25,  // Fixed: was conflicting with BATCH_NORM
    
    // Element-wise
    EIF_LAYER_ADD = 8,
    EIF_LAYER_MULTIPLY = 14,
    EIF_LAYER_CONCAT = 9,
    EIF_LAYER_CLIP = 16,
    
    // Normalization
    EIF_LAYER_LAYER_NORM = 13,
    EIF_LAYER_BATCH_NORM = 26,  // Explicit value
    EIF_LAYER_DROPOUT = 27,
    
    // Recurrent
    EIF_LAYER_RNN = 18,
    EIF_LAYER_LSTM = 19,
    EIF_LAYER_GRU = 20,
    
    // Quantization
    EIF_LAYER_QUANTIZE = 23,
    EIF_LAYER_DEQUANTIZE = 24,
    
    // Attention/Embedding
    EIF_LAYER_ATTENTION = 28,
    EIF_LAYER_EMBEDDING = 29,

    // Priority 1: Object Detection & Math
    EIF_LAYER_SPLIT = 33,
    EIF_LAYER_SUB = 34,
    EIF_LAYER_DIV = 35,
    EIF_LAYER_PAD = 36,

    // Priority 2: NLP & General
    EIF_LAYER_GATHER = 37,
    EIF_LAYER_MATMUL = 38,
    EIF_LAYER_REDUCE_MEAN = 39,
    EIF_LAYER_REDUCE_SUM = 40,

    // Priority 3: Special Ops
    EIF_LAYER_EXP = 41,
    EIF_LAYER_LOG = 42,
    EIF_LAYER_SQRT = 43,
    EIF_LAYER_TOPK = 44,

    // Priority 4: Missing Ops
    EIF_LAYER_ARGMAX = 45,
    EIF_LAYER_MINIMUM = 46,
    EIF_LAYER_MAXIMUM = 47,

    // Custom Operator
    EIF_LAYER_CUSTOM = 100
} eif_layer_type_t;

// ============================================================================
// Activation Types (fused)
// ============================================================================

typedef enum {
    EIF_ACT_NONE = 0,
    EIF_ACT_RELU = 1,
    EIF_ACT_RELU6 = 2
} eif_activation_t;

// ============================================================================
// Layer Parameters
// ============================================================================

typedef union {
    struct { uint16_t units; } dense;
    
    struct {
        uint16_t filters;
        uint8_t kernel_h, kernel_w;
        uint8_t stride_h, stride_w;
        uint8_t pad_h, pad_w;
    } conv2d;
    
    struct {
        uint16_t filters;
        uint8_t kernel_size;
        uint8_t stride, pad;
    } conv1d;
    
    struct {
        uint8_t pool_h, pool_w;
        uint8_t stride_h, stride_w;
    } maxpool2d;
    
    struct {
        uint8_t pool_h, pool_w;
        uint8_t stride_h, stride_w;
    } avgpool2d;
    
    struct {
        uint8_t kernel_h, kernel_w;
        uint8_t stride_h, stride_w;
        uint8_t pad_h, pad_w;
        uint8_t depth_multiplier;
    } depthwise_conv2d;
    
    struct {
        uint16_t filters;
        uint8_t kernel_h, kernel_w;
        uint8_t stride_h, stride_w;
        uint8_t pad_h, pad_w;
    } transpose_conv2d;
    
    struct { uint8_t axis; } concat;
    struct { float epsilon; } layer_norm;
    struct { float min_val, max_val; } clip;
    struct { float alpha; } leaky_relu;
    struct { int target_shape[4]; } reshape;
    struct { float32_t scale_h, scale_w; int method; } resize;
    struct { float32_t rate; } dropout;
    
    // RNN variants
    struct { uint16_t units; uint8_t return_sequences; } rnn;
    struct { uint16_t units; uint8_t return_sequences; } lstm;
    struct { uint16_t units; uint8_t return_sequences; } gru;
    
    // Quantization
    struct { float scale; int32_t zero_point, min_val, max_val; } quantize;
    struct { float scale; int32_t zero_point; } dequantize;
    
    // Batch norm
    struct { int axis; float32_t epsilon; } batch_norm;

    // New Ops Params
    struct { int axis; int num_splits; } split;
    struct { int mode; float32_t constant_value; int pads[4]; } pad; // pads: [top, bottom, left, right]
    struct { int axis; } gather;
    struct { int axis; int keep_dims; } reduce;
    struct { int k; } topk;
    struct { int axis; } argmax;
} eif_layer_param_t;

// ============================================================================
// Quantization Parameters (Int8)
// ============================================================================

typedef struct {
    int32_t input_offset;
    int32_t output_offset;
    int32_t output_multiplier;
    int32_t output_shift;
    int32_t quantized_activation_min;
    int32_t quantized_activation_max;
} eif_quant_param_t;

// ============================================================================
// Layer Configuration
// ============================================================================

typedef struct {
    eif_layer_type_t type;
    eif_activation_t activation;
    uint16_t input_shape[4];   ///< N, H, W, C
    uint16_t output_shape[4];  ///< N, H, W, C
    const void* weights;
    const void* biases;
    eif_layer_param_t params;
    eif_quant_param_t quant_params; // Separate from union to coexist with layer params
} eif_layer_t;

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_LAYERS_H
