/**
 * @file eif_edge_inference.h
 * @brief Optimized Edge Inference Engine
 *
 * Unified inference API for edge deployment:
 * - Model profiling and benchmarking
 * - Memory-efficient execution
 * - Batched/streaming inference
 * - Power-aware scheduling
 *
 * Designed for resource-constrained edge devices.
 */

#ifndef EIF_EDGE_INFERENCE_H
#define EIF_EDGE_INFERENCE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_INFERENCE_MAX_LAYERS 32
#define EIF_INFERENCE_MAX_TENSORS 64

// =============================================================================
// Layer Types
// =============================================================================

typedef enum {
  EIF_LAYER_DENSE = 0,
  EIF_LAYER_CONV2D,
  EIF_LAYER_CONV1D,
  EIF_LAYER_DEPTHWISE_CONV2D,
  EIF_LAYER_MAXPOOL2D,
  EIF_LAYER_AVGPOOL2D,
  EIF_LAYER_FLATTEN,
  EIF_LAYER_RELU,
  EIF_LAYER_SIGMOID,
  EIF_LAYER_SOFTMAX,
  EIF_LAYER_BATCHNORM,
  EIF_LAYER_GRU,
  EIF_LAYER_LSTM,
  EIF_LAYER_ATTENTION,
  EIF_LAYER_ADD,
  EIF_LAYER_CONCAT
} eif_layer_type_t;

/**
 * @brief Tensor shape descriptor
 */
typedef struct {
  int dims[4]; ///< N, H, W, C or sequence_len, hidden_dim, etc.
  int rank;    ///< Number of dimensions (1-4)
} eif_tensor_shape_t;

/**
 * @brief Layer profiling statistics
 */
typedef struct {
  uint32_t execution_us; ///< Execution time in microseconds
  uint32_t memory_bytes; ///< Memory required
  uint32_t flops;        ///< Floating point operations
  uint32_t mac_ops;      ///< Multiply-accumulate operations
} eif_layer_profile_t;

/**
 * @brief Layer descriptor
 */
typedef struct {
  eif_layer_type_t type;
  eif_tensor_shape_t input_shape;
  eif_tensor_shape_t output_shape;
  eif_layer_profile_t profile;

  // Layer-specific parameters
  union {
    struct {
      int units;
      bool use_bias;
    } dense;
    struct {
      int filters, kernel_h, kernel_w, stride, padding;
    } conv2d;
    struct {
      int pool_h, pool_w, stride;
    } pool;
    struct {
      int hidden_size;
    } rnn;
  } params;

  // Weights and biases (pointers to external memory)
  float *weights;
  float *bias;
  int32_t weights_size;

  // Quantization info (if quantized)
  bool is_quantized;
  float scale;
  int32_t zero_point;
} eif_layer_t;

// =============================================================================
// Inference Model
// =============================================================================

/**
 * @brief Complete inference model
 */
typedef struct {
  eif_layer_t layers[EIF_INFERENCE_MAX_LAYERS];
  int num_layers;

  eif_tensor_shape_t input_shape;
  eif_tensor_shape_t output_shape;

  // Buffer management
  float *scratch_buffer;
  int scratch_size;

  // Model metadata
  uint32_t total_params;
  uint32_t total_memory;
  uint32_t total_flops;

  // Execution state
  bool is_initialized;
} eif_model_t;

/**
 * @brief Initialize empty model
 */
static inline void eif_model_init(eif_model_t *model) {
  memset(model, 0, sizeof(eif_model_t));
  model->is_initialized = false;
}

/**
 * @brief Add layer to model
 */
static inline int eif_model_add_layer(eif_model_t *model, eif_layer_type_t type,
                                      eif_tensor_shape_t input_shape,
                                      eif_tensor_shape_t output_shape) {
  if (model->num_layers >= EIF_INFERENCE_MAX_LAYERS)
    return -1;

  int idx = model->num_layers;
  model->layers[idx].type = type;
  model->layers[idx].input_shape = input_shape;
  model->layers[idx].output_shape = output_shape;
  model->layers[idx].weights = NULL;
  model->layers[idx].bias = NULL;
  model->layers[idx].is_quantized = false;

  model->num_layers++;
  return idx;
}

/**
 * @brief Set layer weights
 */
static inline void eif_layer_set_weights(eif_layer_t *layer, float *weights,
                                         int weights_size, float *bias) {
  layer->weights = weights;
  layer->weights_size = weights_size;
  layer->bias = bias;
}

// =============================================================================
// Model Profiling
// =============================================================================

/**
 * @brief Calculate tensor size in elements
 */
static inline int eif_tensor_size(eif_tensor_shape_t *shape) {
  int size = 1;
  for (int i = 0; i < shape->rank; i++) {
    size *= shape->dims[i];
  }
  return size;
}

/**
 * @brief Estimate Dense layer operations
 */
static inline void eif_profile_dense(eif_layer_t *layer) {
  int input_size = eif_tensor_size(&layer->input_shape);
  int output_size = eif_tensor_size(&layer->output_shape);

  layer->profile.mac_ops = input_size * output_size;
  layer->profile.flops = layer->profile.mac_ops * 2; // mul + add
  layer->profile.memory_bytes = (input_size * output_size + output_size) * 4;
}

/**
 * @brief Estimate Conv2D layer operations
 */
static inline void eif_profile_conv2d(eif_layer_t *layer) {
  int in_h = layer->input_shape.dims[1];
  int in_w = layer->input_shape.dims[2];
  int in_c = layer->input_shape.dims[3];
  int out_h = layer->output_shape.dims[1];
  int out_w = layer->output_shape.dims[2];
  int out_c = layer->output_shape.dims[3];
  int k_h = layer->params.conv2d.kernel_h;
  int k_w = layer->params.conv2d.kernel_w;

  layer->profile.mac_ops = out_h * out_w * out_c * k_h * k_w * in_c;
  layer->profile.flops = layer->profile.mac_ops * 2;
  layer->profile.memory_bytes = (k_h * k_w * in_c * out_c + out_c) * 4;
}

/**
 * @brief Profile entire model
 */
static inline void eif_model_profile(eif_model_t *model) {
  model->total_params = 0;
  model->total_memory = 0;
  model->total_flops = 0;

  for (int i = 0; i < model->num_layers; i++) {
    eif_layer_t *layer = &model->layers[i];

    switch (layer->type) {
    case EIF_LAYER_DENSE:
      eif_profile_dense(layer);
      break;
    case EIF_LAYER_CONV2D:
    case EIF_LAYER_CONV1D:
      eif_profile_conv2d(layer);
      break;
    default:
      // Non-weight layers
      layer->profile.mac_ops = 0;
      layer->profile.flops = eif_tensor_size(&layer->input_shape);
      layer->profile.memory_bytes = 0;
      break;
    }

    model->total_params += layer->weights_size;
    model->total_memory += layer->profile.memory_bytes;
    model->total_flops += layer->profile.flops;
  }
}

/**
 * @brief Estimate scratch buffer size needed
 */
static inline int eif_model_scratch_size(eif_model_t *model) {
  int max_size = 0;

  for (int i = 0; i < model->num_layers; i++) {
    int in_size = eif_tensor_size(&model->layers[i].input_shape) * 4;
    int out_size = eif_tensor_size(&model->layers[i].output_shape) * 4;
    int layer_size = in_size + out_size; // Double buffer
    if (layer_size > max_size)
      max_size = layer_size;
  }

  return max_size;
}

// =============================================================================
// Memory Estimation
// =============================================================================

/**
 * @brief Memory breakdown for edge deployment
 */
typedef struct {
  uint32_t weights_bytes;     ///< Model weights
  uint32_t activations_bytes; ///< Peak activation memory
  uint32_t scratch_bytes;     ///< Working buffer
  uint32_t total_bytes;       ///< Total memory required
  float weight_compression;   ///< Ratio if quantized
} eif_memory_estimate_t;

/**
 * @brief Estimate memory requirements
 */
static inline void eif_estimate_memory(eif_model_t *model,
                                       eif_memory_estimate_t *mem) {
  mem->weights_bytes = model->total_memory;
  mem->scratch_bytes = eif_model_scratch_size(model);

  // Find peak activation size
  int peak_activation = 0;
  for (int i = 0; i < model->num_layers; i++) {
    int size = eif_tensor_size(&model->layers[i].output_shape) * 4;
    if (size > peak_activation)
      peak_activation = size;
  }
  mem->activations_bytes = peak_activation * 2; // Input + output

  mem->total_bytes =
      mem->weights_bytes + mem->activations_bytes + mem->scratch_bytes;

  // Check for quantization savings
  bool any_quantized = false;
  for (int i = 0; i < model->num_layers; i++) {
    if (model->layers[i].is_quantized) {
      any_quantized = true;
      break;
    }
  }
  mem->weight_compression = any_quantized ? 4.0f : 1.0f; // INT8 = 4x
}

/**
 * @brief Check if model fits in memory
 */
static inline bool eif_model_fits_memory(eif_model_t *model,
                                         uint32_t available_ram) {
  eif_memory_estimate_t mem;
  eif_estimate_memory(model, &mem);
  return mem.total_bytes <= available_ram;
}

// =============================================================================
// Inference Configuration
// =============================================================================

typedef struct {
  bool enable_profiling;      ///< Collect timing info
  bool power_save_mode;       ///< Reduce speed for power savings
  int batch_size;             ///< Batch size (1 for streaming)
  float confidence_threshold; ///< Min confidence for valid output
} eif_inference_config_t;

/**
 * @brief Default inference configuration
 */
static inline void eif_inference_config_default(eif_inference_config_t *cfg) {
  cfg->enable_profiling = false;
  cfg->power_save_mode = false;
  cfg->batch_size = 1;
  cfg->confidence_threshold = 0.5f;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_EDGE_INFERENCE_H
