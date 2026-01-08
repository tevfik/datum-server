/**
 * @file eif_model.h
 * @brief Structured Model API for EIF
 *
 * Enables runtime model construction with a clean C API.
 * Models can be defined declaratively and executed efficiently.
 *
 * Features:
 * - Layer-based model definition
 * - Automatic workspace allocation
 * - Single inference call
 * - Memory-efficient double buffering
 */

#ifndef EIF_MODEL_H
#define EIF_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "eif_rnn.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

#ifndef EIF_MODEL_MAX_LAYERS
#define EIF_MODEL_MAX_LAYERS 32
#endif

#ifndef EIF_MODEL_MAX_WORKSPACE
#define EIF_MODEL_MAX_WORKSPACE (64 * 1024) // 64KB default
#endif

// =============================================================================
// Layer Types
// =============================================================================

typedef enum {
  EIF_LAYER_INPUT = 0,
  EIF_LAYER_CONV2D,
  EIF_LAYER_DWCONV2D,
  EIF_LAYER_DENSE,
  EIF_LAYER_MAXPOOL2D,
  EIF_LAYER_AVGPOOL2D,
  EIF_LAYER_GLOBAL_AVGPOOL,
  EIF_LAYER_FLATTEN,
  EIF_LAYER_RELU,
  EIF_LAYER_LEAKY_RELU,
  EIF_LAYER_SIGMOID,
  EIF_LAYER_TANH,
  EIF_LAYER_SOFTMAX,
  EIF_LAYER_DROPOUT, // No-op in inference
  EIF_LAYER_BATCHNORM,
  EIF_LAYER_ADD,
  EIF_LAYER_CONCAT,
  EIF_LAYER_RNN,
  EIF_LAYER_LSTM,
  EIF_LAYER_GRU
} eif_layer_type_t;

// =============================================================================
// Layer Configuration
// =============================================================================

typedef struct {
  int height;
  int width;
  int channels;
} eif_shape_t;

typedef struct {
  int filters;
  int kernel_h;
  int kernel_w;
  int stride_h;
  int stride_w;
  int padding; // 0=valid, 1=same
  int dilation;
} eif_conv_params_t;

typedef struct {
  int units;
} eif_dense_params_t;

typedef struct {
  int pool_h;
  int pool_w;
  int stride_h;
  int stride_w;
} eif_pool_params_t;

typedef struct {
  int hidden_size;
  bool stateful;
  bool return_sequences;
} eif_rnn_params_t;

typedef struct {
  float alpha; // For LeakyReLU
} eif_activation_params_t;

/**
 * @brief Layer configuration structure
 */
typedef struct {
  eif_layer_type_t type;

  union {
    eif_shape_t input;
    eif_conv_params_t conv;
    eif_dense_params_t dense;
    eif_pool_params_t pool;
    eif_rnn_params_t rnn;
    eif_activation_params_t activation;
  } params;

  // Weights (NULL for layers without weights)
  const int16_t *weights;
  const int16_t *bias;

  // Computed shapes (filled during model creation)
  eif_shape_t input_shape;
  eif_shape_t output_shape;
  int input_size;
  int output_size;
} eif_layer_t;

// =============================================================================
// Layer Macros (Convenience)
// =============================================================================

#define EIF_INPUT(h, w, c)                                                     \
  {.type = EIF_LAYER_INPUT,                                                    \
   .params.input = {h, w, c},                                                  \
   .weights = NULL,                                                            \
   .bias = NULL}

#define EIF_CONV2D(filt, kern, str, w_ptr, b_ptr)                              \
  {.type = EIF_LAYER_CONV2D,                                                   \
   .params.conv = {filt, kern, kern, str, str, 1, 1},                          \
   .weights = w_ptr,                                                           \
   .bias = b_ptr}

#define EIF_CONV2D_FULL(f, kh, kw, sh, sw, pad, dil, w_ptr, b_ptr)             \
  {.type = EIF_LAYER_CONV2D,                                                   \
   .params.conv = {f, kh, kw, sh, sw, pad, dil},                               \
   .weights = w_ptr,                                                           \
   .bias = b_ptr}

#define EIF_DWCONV2D(kern, str, w_ptr, b_ptr)                                  \
  {.type = EIF_LAYER_DWCONV2D,                                                 \
   .params.conv = {0, kern, kern, str, str, 1, 1},                             \
   .weights = w_ptr,                                                           \
   .bias = b_ptr}

#define EIF_DENSE(n_units, w_ptr, b_ptr)                                       \
  {.type = EIF_LAYER_DENSE,                                                    \
   .params.dense = {n_units},                                                  \
   .weights = w_ptr,                                                           \
   .bias = b_ptr}

#define EIF_MAXPOOL(pool_size)                                                 \
  {.type = EIF_LAYER_MAXPOOL2D,                                                \
   .params.pool = {pool_size, pool_size, pool_size, pool_size},                \
   .weights = NULL,                                                            \
   .bias = NULL}

#define EIF_AVGPOOL(pool_size)                                                 \
  {.type = EIF_LAYER_AVGPOOL2D,                                                \
   .params.pool = {pool_size, pool_size, pool_size, pool_size},                \
   .weights = NULL,                                                            \
   .bias = NULL}

#define EIF_GLOBAL_AVGPOOL()                                                   \
  {.type = EIF_LAYER_GLOBAL_AVGPOOL, .weights = NULL, .bias = NULL}

#define EIF_FLATTEN() {.type = EIF_LAYER_FLATTEN, .weights = NULL, .bias = NULL}

#define EIF_RELU() {.type = EIF_LAYER_RELU, .weights = NULL, .bias = NULL}

#define EIF_LEAKY_RELU(a)                                                      \
  {.type = EIF_LAYER_LEAKY_RELU,                                               \
   .params.activation = {a},                                                   \
   .weights = NULL,                                                            \
   .bias = NULL}

#define EIF_SIGMOID() {.type = EIF_LAYER_SIGMOID, .weights = NULL, .bias = NULL}

#define EIF_TANH() {.type = EIF_LAYER_TANH, .weights = NULL, .bias = NULL}

#define EIF_SOFTMAX() {.type = EIF_LAYER_SOFTMAX, .weights = NULL, .bias = NULL}

#define EIF_LSTM(hidden, is_stateful, w_ptr, b_ptr)                            \
  {.type = EIF_LAYER_LSTM,                                                     \
   .params.rnn = {hidden, is_stateful, false},                                 \
   .weights = w_ptr,                                                           \
   .bias = b_ptr}

// =============================================================================
// Model Structure
// =============================================================================

typedef enum {
  EIF_STATUS_OK = 0,
  EIF_STATUS_ERROR = -1,
  EIF_STATUS_NO_MEMORY = -2,
  EIF_STATUS_INVALID_LAYER = -3,
  EIF_STATUS_SHAPE_MISMATCH = -4
} eif_status_t;

typedef struct {
  // Layer configuration
  eif_layer_t *layers;
  int num_layers;

  // Computed properties
  int input_size;
  int output_size;
  int total_params;
  int workspace_size;

  // Workspace buffers (double buffering)
  int16_t *workspace_a;
  int16_t *workspace_b;

  // State for RNN layers
  int16_t *rnn_state;
  int rnn_state_size;
  int state_size_bytes;

  // Status
  bool initialized;
} eif_model_t;

// =============================================================================
// Model API
// =============================================================================

/**
 * @brief Create model from layer array
 * @param model Model structure to initialize
 * @param layers Array of layer configurations
 * @param num_layers Number of layers
 * @return Status code
 */
static inline eif_status_t
eif_model_create(eif_model_t *model, eif_layer_t *layers, int num_layers);

/**
 * @brief Run inference on model
 * @param model Initialized model
 * @param input Input data (model->input_size elements)
 * @param output Output buffer (model->output_size elements)
 * @return Status code
 */
static inline eif_status_t
eif_model_infer(eif_model_t *model, const int16_t *input, int16_t *output);

/**
 * @brief Free model resources
 */
static inline void eif_model_destroy(eif_model_t *model);

/**
 * @brief Print model summary
 */
static inline void eif_model_summary(const eif_model_t *model);

// =============================================================================
// Layer Names
// =============================================================================

static const char *eif_layer_names[] = {
    "Input",     "Conv2D",        "DWConv2D", "Dense",   "MaxPool2D",
    "AvgPool2D", "GlobalAvgPool", "Flatten",  "ReLU",    "LeakyReLU",
    "Sigmoid",   "Tanh",          "Softmax",  "Dropout", "BatchNorm",
    "Add",       "Concat",        "RNN",      "LSTM",    "GRU"};

// =============================================================================
// Shape Inference
// =============================================================================

static inline eif_shape_t eif_infer_conv_shape(eif_shape_t in,
                                               eif_conv_params_t *p) {
  eif_shape_t out;
  if (p->padding == 1) { // Same padding
    out.height = (in.height + p->stride_h - 1) / p->stride_h;
    out.width = (in.width + p->stride_w - 1) / p->stride_w;
  } else { // Valid padding
    out.height = (in.height - p->kernel_h) / p->stride_h + 1;
    out.width = (in.width - p->kernel_w) / p->stride_w + 1;
  }
  out.channels = p->filters;
  return out;
}

static inline eif_shape_t eif_infer_pool_shape(eif_shape_t in,
                                               eif_pool_params_t *p) {
  eif_shape_t out;
  out.height = in.height / p->stride_h;
  out.width = in.width / p->stride_w;
  out.channels = in.channels;
  return out;
}

static inline int eif_shape_size(eif_shape_t s) {
  return s.height * s.width * s.channels;
}

// =============================================================================
// Layer Execution Functions
// =============================================================================

static inline void eif_exec_conv2d(const int16_t *in, int16_t *out,
                                   eif_layer_t *layer) {
  eif_shape_t is = layer->input_shape;
  eif_shape_t os = layer->output_shape;
  eif_conv_params_t *p = &layer->params.conv;

  const int16_t *W = layer->weights;
  const int16_t *B = layer->bias;

  // Simple conv2d implementation
  for (int oy = 0; oy < os.height; oy++) {
    for (int ox = 0; ox < os.width; ox++) {
      for (int oc = 0; oc < os.channels; oc++) {
        int32_t acc = 0;

        for (int ky = 0; ky < p->kernel_h; ky++) {
          for (int kx = 0; kx < p->kernel_w; kx++) {
            int iy = oy * p->stride_h + ky;
            int ix = ox * p->stride_w + kx;

            if (iy >= 0 && iy < is.height && ix >= 0 && ix < is.width) {
              for (int ic = 0; ic < is.channels; ic++) {
                int in_idx = (iy * is.width + ix) * is.channels + ic;
                int w_idx =
                    ((ky * p->kernel_w + kx) * is.channels + ic) * os.channels +
                    oc;
                acc += (int32_t)in[in_idx] * W[w_idx];
              }
            }
          }
        }

        acc = (acc >> 15);
        if (B)
          acc += B[oc];

        // Saturate
        if (acc > 32767)
          acc = 32767;
        if (acc < -32768)
          acc = -32768;

        int out_idx = (oy * os.width + ox) * os.channels + oc;
        out[out_idx] = (int16_t)acc;
      }
    }
  }
}

static inline void eif_exec_dense(const int16_t *in, int16_t *out,
                                  eif_layer_t *layer) {
  int in_size = layer->input_size;
  int out_size = layer->params.dense.units;

  const int16_t *W = layer->weights;
  const int16_t *B = layer->bias;

  for (int o = 0; o < out_size; o++) {
    int32_t acc = 0;
    for (int i = 0; i < in_size; i++) {
      acc += (int32_t)in[i] * W[i * out_size + o];
    }
    acc = (acc >> 15);
    if (B)
      acc += B[o];

    if (acc > 32767)
      acc = 32767;
    if (acc < -32768)
      acc = -32768;

    out[o] = (int16_t)acc;
  }
}

static inline void eif_exec_maxpool2d(const int16_t *in, int16_t *out,
                                      eif_layer_t *layer) {
  eif_shape_t is = layer->input_shape;
  eif_shape_t os = layer->output_shape;
  eif_pool_params_t *p = &layer->params.pool;

  for (int oy = 0; oy < os.height; oy++) {
    for (int ox = 0; ox < os.width; ox++) {
      for (int c = 0; c < os.channels; c++) {
        int16_t max_val = -32768;

        for (int py = 0; py < p->pool_h; py++) {
          for (int px = 0; px < p->pool_w; px++) {
            int iy = oy * p->stride_h + py;
            int ix = ox * p->stride_w + px;

            if (iy < is.height && ix < is.width) {
              int idx = (iy * is.width + ix) * is.channels + c;
              if (in[idx] > max_val)
                max_val = in[idx];
            }
          }
        }

        out[(oy * os.width + ox) * os.channels + c] = max_val;
      }
    }
  }
}

static inline void eif_exec_avgpool2d(const int16_t *in, int16_t *out,
                                      eif_layer_t *layer) {
  eif_shape_t is = layer->input_shape;
  eif_shape_t os = layer->output_shape;
  eif_pool_params_t *p = &layer->params.pool;
  int pool_area = p->pool_h * p->pool_w;

  for (int oy = 0; oy < os.height; oy++) {
    for (int ox = 0; ox < os.width; ox++) {
      for (int c = 0; c < os.channels; c++) {
        int32_t sum = 0;

        for (int py = 0; py < p->pool_h; py++) {
          for (int px = 0; px < p->pool_w; px++) {
            int iy = oy * p->stride_h + py;
            int ix = ox * p->stride_w + px;

            if (iy < is.height && ix < is.width) {
              sum += in[(iy * is.width + ix) * is.channels + c];
            }
          }
        }

        out[(oy * os.width + ox) * os.channels + c] =
            (int16_t)(sum / pool_area);
      }
    }
  }
}

static inline void eif_exec_global_avgpool(const int16_t *in, int16_t *out,
                                           eif_layer_t *layer) {
  eif_shape_t is = layer->input_shape;
  int area = is.height * is.width;

  for (int c = 0; c < is.channels; c++) {
    int32_t sum = 0;
    for (int i = 0; i < area; i++) {
      sum += in[i * is.channels + c];
    }
    out[c] = (int16_t)(sum / area);
  }
}

static inline void eif_exec_relu(int16_t *data, int size) {
  for (int i = 0; i < size; i++) {
    if (data[i] < 0)
      data[i] = 0;
  }
}

static inline void eif_exec_leaky_relu(int16_t *data, int size,
                                       int16_t alpha_q15) {
  for (int i = 0; i < size; i++) {
    if (data[i] < 0) {
      data[i] = (int16_t)(((int32_t)data[i] * alpha_q15) >> 15);
    }
  }
}

static inline void eif_exec_softmax(int16_t *data, int size) {
  // Find max for numerical stability
  int16_t max_val = data[0];
  for (int i = 1; i < size; i++) {
    if (data[i] > max_val)
      max_val = data[i];
  }

  // Approximate exp and sum
  int32_t sum = 0;
  for (int i = 0; i < size; i++) {
    data[i] = data[i] - max_val;
    // Approximate exp: shift up by 8
    int32_t exp_approx = (data[i] > -8192) ? (data[i] + 16384) : 0;
    if (exp_approx < 0)
      exp_approx = 0;
    data[i] = (int16_t)exp_approx;
    sum += exp_approx;
  }

  // Normalize
  if (sum > 0) {
    for (int i = 0; i < size; i++) {
      data[i] = (int16_t)(((int32_t)data[i] * 32767) / sum);
    }
  }
}

static inline void eif_exec_rnn(const int16_t *in, int16_t *out,
                                eif_layer_t *layer, int16_t *state_ptr) {
  eif_rnn_cell_t cell;
  int input_size = layer->input_size;
  int hidden_size = layer->params.rnn.hidden_size;

  cell.input_size = input_size;
  cell.hidden_size = hidden_size;
  cell.stateful = layer->params.rnn.stateful;
  cell.use_tanh = true; // Default to tanh for consistency
  cell.h = state_ptr;

  // Converter packs W_ih [I, H] and W_hh [H, H] sequentially
  int w_ih_size = input_size * hidden_size;
  cell.W_ih = layer->weights;
  cell.W_hh = layer->weights + w_ih_size;

  cell.b_ih = layer->bias;
  cell.b_hh = layer->bias + hidden_size;

  // In EIF Model, input is [Batch?, Time, InputSize] or [InputSize]?
  // EIF is mostly designed for frame-based or single-shot.
  // Assume input is [InputSize] (SEQ_LEN=1) unless we update API.
  eif_rnn_sequence(&cell, in, 1, out, layer->params.rnn.return_sequences,
                   state_ptr);
}

static inline void eif_exec_lstm(const int16_t *in, int16_t *out,
                                 eif_layer_t *layer, int16_t *state_ptr) {
  eif_lstm_cell_t cell;
  int input_size = layer->input_size;
  int hidden_size = layer->params.rnn.hidden_size;

  cell.input_size = input_size;
  cell.hidden_size = hidden_size;
  cell.stateful = layer->params.rnn.stateful;

  // State layout: H then C (from converter/runtime calc)
  cell.h = state_ptr;
  cell.c = state_ptr + hidden_size;

  // Weights packed: F, I, C, O
  // Each block size: (input_size + hidden_size) * hidden_size
  int gate_weight_size = (input_size + hidden_size) * hidden_size;

  cell.W_f = layer->weights;
  cell.W_i = layer->weights + gate_weight_size;
  cell.W_c = layer->weights + 2 * gate_weight_size;
  cell.W_o = layer->weights + 3 * gate_weight_size;

  // Biases packed: F, I, C, O. Size: hidden_size
  cell.b_f = layer->bias;
  cell.b_i = layer->bias + hidden_size;
  cell.b_c = layer->bias + 2 * hidden_size;
  cell.b_o = layer->bias + 3 * hidden_size;

  eif_lstm_sequence(&cell, in, 1, out, layer->params.rnn.return_sequences,
                    state_ptr);
}

static inline void eif_exec_gru(const int16_t *in, int16_t *out,
                                eif_layer_t *layer, int16_t *state_ptr) {
  eif_gru_cell_t cell;
  int input_size = layer->input_size;
  int hidden_size = layer->params.rnn.hidden_size;

  cell.input_size = input_size;
  cell.hidden_size = hidden_size;
  cell.stateful = layer->params.rnn.stateful;
  cell.h = state_ptr;

  // Weights packed: R, Z, H
  int gate_weight_size = (input_size + hidden_size) * hidden_size;

  cell.W_r = layer->weights;
  cell.W_z = layer->weights + gate_weight_size;
  cell.W_h = layer->weights + 2 * gate_weight_size;

  cell.b_r = layer->bias;
  cell.b_z = layer->bias + hidden_size;
  cell.b_h = layer->bias + 2 * hidden_size;

  eif_gru_sequence(&cell, in, 1, out, layer->params.rnn.return_sequences,
                   state_ptr);
}

// =============================================================================
// Model Implementation
// =============================================================================

static int16_t _eif_workspace_a[EIF_MODEL_MAX_WORKSPACE / 2];
static int16_t _eif_workspace_b[EIF_MODEL_MAX_WORKSPACE / 2];

static inline eif_status_t
eif_model_create(eif_model_t *model, eif_layer_t *layers, int num_layers) {
  if (!model || !layers || num_layers <= 0) {
    return EIF_STATUS_ERROR;
  }

  if (num_layers > EIF_MODEL_MAX_LAYERS) {
    return EIF_STATUS_ERROR;
  }

  memset(model, 0, sizeof(eif_model_t));
  model->layers = layers;
  model->num_layers = num_layers;

  // Use static workspace
  model->workspace_a = _eif_workspace_a;
  model->workspace_b = _eif_workspace_b;

  // Infer shapes through the network
  eif_shape_t current_shape = {0, 0, 0};
  int max_size = 0;
  int state_size = 0;

  for (int i = 0; i < num_layers; i++) {
    eif_layer_t *layer = &layers[i];

    // Set input shape
    if (i == 0) {
      if (layer->type != EIF_LAYER_INPUT) {
        return EIF_STATUS_INVALID_LAYER;
      }
      current_shape = layer->params.input;
      layer->input_shape = current_shape;
      layer->output_shape = current_shape;
    } else {
      layer->input_shape = current_shape;
    }

    layer->input_size = eif_shape_size(current_shape);
    if (layer->input_size > max_size)
      max_size = layer->input_size;

    // Infer output shape based on layer type
    switch (layer->type) {
    case EIF_LAYER_INPUT:
      // Already handled
      break;

    case EIF_LAYER_CONV2D:
      current_shape = eif_infer_conv_shape(current_shape, &layer->params.conv);
      break;

    case EIF_LAYER_MAXPOOL2D:
    case EIF_LAYER_AVGPOOL2D:
      current_shape = eif_infer_pool_shape(current_shape, &layer->params.pool);
      break;

    case EIF_LAYER_GLOBAL_AVGPOOL:
      current_shape.height = 1;
      current_shape.width = 1;
      break;

    case EIF_LAYER_FLATTEN:
      current_shape.height = 1;
      current_shape.width = 1;
      current_shape.channels = layer->input_size;
      break;

    case EIF_LAYER_DENSE:
      current_shape.height = 1;
      current_shape.width = 1;
      current_shape.channels = layer->params.dense.units;
      break;

    case EIF_LAYER_RNN:
      current_shape.height = 1;
      current_shape.width = 1;
      current_shape.channels = layer->params.rnn.hidden_size;
      state_size += layer->params.rnn.hidden_size;
      break;

    case EIF_LAYER_LSTM:
      current_shape.height = 1;
      current_shape.width = 1;
      current_shape.channels = layer->params.rnn.hidden_size;
      state_size += layer->params.rnn.hidden_size * 2; // H + C
      break;

    case EIF_LAYER_GRU:
      current_shape.height = 1;
      current_shape.width = 1;
      current_shape.channels = layer->params.rnn.hidden_size;
      state_size += layer->params.rnn.hidden_size;
      break;

    case EIF_LAYER_RELU:
    case EIF_LAYER_LEAKY_RELU:
    case EIF_LAYER_SIGMOID:
    case EIF_LAYER_TANH:
    case EIF_LAYER_SOFTMAX:
    case EIF_LAYER_DROPOUT:
      // Shape unchanged
      break;

    default:
      break;
    }

    layer->output_shape = current_shape;
    layer->output_size = eif_shape_size(current_shape);
    if (layer->output_size > max_size)
      max_size = layer->output_size;
  }

  // Set model properties
  model->input_size = layers[0].output_size;
  model->output_size = layers[num_layers - 1].output_size;
  model->workspace_size = max_size;
  model->rnn_state_size = state_size;
  model->state_size_bytes = state_size * sizeof(int16_t);
  model->initialized = true;

  return EIF_STATUS_OK;
}

static inline eif_status_t
eif_model_infer(eif_model_t *model, const int16_t *input, int16_t *output) {
  if (!model || !model->initialized || !input || !output) {
    return EIF_STATUS_ERROR;
  }

  // Setup double buffering
  const int16_t *in = input;
  int16_t *out = model->workspace_a;

  // State pointer tracking
  int16_t *current_state_ptr = model->rnn_state;
  if (model->rnn_state_size > 0 && !model->rnn_state) {
    // If state required but no memory provided, error?
    // Or fail gracefully
    return EIF_STATUS_ERROR;
  }

  for (int i = 1; i < model->num_layers; i++) { // Skip input layer
    eif_layer_t *layer = &model->layers[i];

    // Use output buffer for last layer
    if (i == model->num_layers - 1) {
      out = output;
    }

    switch (layer->type) {
    case EIF_LAYER_CONV2D:
      eif_exec_conv2d(in, out, layer);
      break;

    case EIF_LAYER_DENSE:
      eif_exec_dense(in, out, layer);
      break;

    case EIF_LAYER_MAXPOOL2D:
      eif_exec_maxpool2d(in, out, layer);
      break;

    case EIF_LAYER_AVGPOOL2D:
      eif_exec_avgpool2d(in, out, layer);
      break;

    case EIF_LAYER_GLOBAL_AVGPOOL:
      eif_exec_global_avgpool(in, out, layer);
      break;

    case EIF_LAYER_FLATTEN:
      // Just copy if different buffers
      if (in != out) {
        memcpy(out, in, layer->input_size * sizeof(int16_t));
      }
      break;

    case EIF_LAYER_RELU:
      if (in != out)
        memcpy(out, in, layer->input_size * sizeof(int16_t));
      eif_exec_relu(out, layer->output_size);
      break;

    case EIF_LAYER_LEAKY_RELU: {
      if (in != out)
        memcpy(out, in, layer->input_size * sizeof(int16_t));
      int16_t alpha = (int16_t)(layer->params.activation.alpha * 32767);
      eif_exec_leaky_relu(out, layer->output_size, alpha);
      break;
    }

    case EIF_LAYER_SOFTMAX:
      if (in != out)
        memcpy(out, in, layer->input_size * sizeof(int16_t));
      eif_exec_softmax(out, layer->output_size);
      break;

    case EIF_LAYER_DROPOUT:
      // No-op in inference
      if (in != out)
        memcpy(out, in, layer->input_size * sizeof(int16_t));
      break;

    case EIF_LAYER_RNN:
      eif_exec_rnn(in, out, layer, current_state_ptr);
      current_state_ptr += layer->params.rnn.hidden_size;
      break;

    case EIF_LAYER_LSTM:
      eif_exec_lstm(in, out, layer, current_state_ptr);
      current_state_ptr += layer->params.rnn.hidden_size * 2;
      break;

    case EIF_LAYER_GRU:
      eif_exec_gru(in, out, layer, current_state_ptr);
      current_state_ptr += layer->params.rnn.hidden_size;
      break;

    default:
      // Unknown layer, copy through
      if (in != out)
        memcpy(out, in, layer->input_size * sizeof(int16_t));
      break;
    }

    // Swap buffers
    if (i < model->num_layers - 1) {
      in = out;
      out =
          (out == model->workspace_a) ? model->workspace_b : model->workspace_a;
    }
  }

  return EIF_STATUS_OK;
}

static inline void eif_model_destroy(eif_model_t *model) {
  if (model) {
    // Static workspace, nothing to free
    model->initialized = false;
  }
}

static inline void eif_model_summary(const eif_model_t *model) {
  if (!model || !model->initialized)
    return;

// Note: printf may not be available on all embedded targets
#ifdef EIF_HAS_PRINTF
  printf("Model Summary\n");
  printf("═══════════════════════════════════════════════════\n");
  printf("%-20s %-15s %10s\n", "Layer", "Type", "Output Size");
  printf("───────────────────────────────────────────────────\n");

  for (int i = 0; i < model->num_layers; i++) {
    eif_layer_t *l = &model->layers[i];
    printf("%-20s %-15s %10d\n", "", eif_layer_names[l->type], l->output_size);
  }

  printf("═══════════════════════════════════════════════════\n");
  printf("Total layers: %d\n", model->num_layers);
  printf("Input size: %d, Output size: %d\n", model->input_size,
         model->output_size);
#endif
}

#ifdef __cplusplus
}
#endif

#endif // EIF_MODEL_H
