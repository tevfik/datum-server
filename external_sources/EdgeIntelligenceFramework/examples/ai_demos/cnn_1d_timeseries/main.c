/**
 * @file main.c
 * @brief 1D CNN Time Series Classification Demo
 *
 * Simulates Human Activity Recognition (HAR) using a simple 1D CNN.
 * Input: 3 channels (Accel X, Y, Z), 50 time steps
 * Model: Conv1D -> MaxPool -> FC -> Softmax
 *
 * Demo synthesizes data for "Walking" and "Standing" and classifies it.
 */

#include "eif_nn_layers.h" // Public types
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Extern declarations from eif_dl library (normally internal, verified via
// eif_dl_internal.h)
extern void eif_layer_conv1d(const eif_layer_t *layer, const float32_t *input,
                             float32_t *output, int in_w, int in_c, int *out_w,
                             int *out_c);
extern void eif_layer_maxpool2d(const eif_layer_t *layer,
                                const float32_t *input, float32_t *output,
                                int in_h, int in_w, int in_c, int *out_h,
                                int *out_w);
extern void eif_layer_dense(const eif_layer_t *layer, const float32_t *input,
                            float32_t *output, int input_size);

#define SEQ_LEN 50
#define CHANNELS 3
#define FILTERS 8
#define KERNEL 3
#define STRIDE 1

// Classes
const char *CLASSES[] = {"Standing", "Walking"};

// Mock Weights (Random initialization for demo purpose,
// usually loaded from model.h)
static float32_t conv_weights[FILTERS * KERNEL * CHANNELS];
static float32_t conv_biases[FILTERS];
static float32_t fc_weights[2 * (SEQ_LEN / 2 * FILTERS)]; // After pooling
static float32_t fc_biases[2];

void init_random_weights() {
  // Deterministic random
  srand(42);
  for (int i = 0; i < sizeof(conv_weights) / sizeof(float); i++)
    conv_weights[i] = ((float)(rand() % 100) / 100.0f - 0.5f) * 0.1f;

  // Demo: Manually configure weights to detect "Walking" pattern (high variance/frequency)
  // Walking typically has alternating positive/negative values.
  // We set one filter to be a high-pass filter: [-1, 0, 1]
  for (int c = 0; c < CHANNELS; c++) {
    // Filter 0, Channel c
    int f = 0;
    conv_weights[f * (KERNEL * CHANNELS) + 0 * CHANNELS + c] = -1.0f;
    conv_weights[f * (KERNEL * CHANNELS) + 1 * CHANNELS + c] = 0.0f;
    conv_weights[f * (KERNEL * CHANNELS) + 2 * CHANNELS + c] = 1.0f;
  }

  // FC weights favor "Walking" if Filter 0 activation is high
  int fc_in = SEQ_LEN / 2 * FILTERS; // Assuming stride 2 pool
  for (int i = 0; i < fc_in; i++) {
    // Class 1 (Walking)
    if (i % FILTERS == 0) // Filter 0
      fc_weights[1 * fc_in + i] = 0.5f;
    else
      fc_weights[1 * fc_in + i] = 0.0f;

    // Class 0 (Standing) - small weights
    fc_weights[0 * fc_in + i] = -0.1f;
  }

  // Bias favors Standing slightly
  fc_biases[0] = 0.5f;
  fc_biases[1] = -0.5f;
}

void simple_cnn_inference(const float32_t *input, float32_t *probs) {
  // 1. Conv1D
  // Input: [SEQ_LEN, CHANNELS]
  // Output: [SEQ_LEN, FILTERS] (Simplified, actually slightly smaller due to
  // valid padding)
  int out_w, out_c;

  // Config layer
  eif_layer_t conv_l = {0};
  conv_l.type = EIF_LAYER_CONV1D;
  conv_l.activation = EIF_ACT_RELU;
  conv_l.weights = conv_weights;
  conv_l.biases = conv_biases;
  conv_l.params.conv1d.filters = FILTERS;
  conv_l.params.conv1d.kernel_size = KERNEL;
  conv_l.params.conv1d.stride = 1;
  conv_l.params.conv1d.pad = 1; // Same padding approximation

  // Buffers (Stack allocated for demo)
  float32_t conv_out[SEQ_LEN * FILTERS];
  eif_layer_conv1d(&conv_l, input, conv_out, SEQ_LEN, CHANNELS, &out_w, &out_c);

  // 2. MaxPool1D (Simulated using MaxPool2D with height 1)
  eif_layer_t pool_l = {0};
  pool_l.params.maxpool2d.pool_h = 1;
  pool_l.params.maxpool2d.pool_w = 2; // Stride 2
  pool_l.params.maxpool2d.stride_h = 1;
  pool_l.params.maxpool2d.stride_w = 2;

  int pool_out_w, pool_out_h;
  float32_t pool_out[(SEQ_LEN / 2 + 1) * FILTERS];
  eif_layer_maxpool2d(&pool_l, conv_out, pool_out, 1, out_w, FILTERS,
                      &pool_out_h, &pool_out_w);

  // 3. Dense (FC)
  // Flattened size = pool_out_w * FILTERS
  int fc_input_size = pool_out_w * FILTERS;
  eif_layer_t fc_l = {0};
  fc_l.params.dense.units = 2;
  fc_l.weights = fc_weights;
  fc_l.biases = fc_biases;

  float32_t logits[2];
  eif_layer_dense(&fc_l, pool_out, logits, fc_input_size);

  // 4. Softmax
  float32_t max_val = (logits[0] > logits[1]) ? logits[0] : logits[1];
  float32_t sum = 0.0f;
  for (int i = 0; i < 2; i++) {
    probs[i] = expf(logits[i] - max_val);
    sum += probs[i];
  }
  for (int i = 0; i < 2; i++)
    probs[i] /= sum;
}

int main(int argc, char **argv) {
  int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

  init_random_weights();

  // Generate 'Walking' Data (High frequency sine)
  float32_t input_walking[SEQ_LEN * CHANNELS];
  for (int i = 0; i < SEQ_LEN; i++) {
    input_walking[i * CHANNELS + 0] = sinf(i * 0.8f);
    input_walking[i * CHANNELS + 1] = cosf(i * 0.8f);
    input_walking[i * CHANNELS + 2] = sinf(i * 0.4f) * 0.5f;
  }

  float32_t probs[2];
  simple_cnn_inference(input_walking, probs);
  int pred_class = (probs[1] > probs[0]) ? 1 : 0;

  if (json_mode) {
    printf("{\"type\": \"cnn_1d\", \"input_type\": \"walking\", "
           "\"prediction\": \"%s\", ",
           CLASSES[pred_class]);
    printf("\"probabilities\": {\"standing\": %.2f, \"walking\": %.2f}, ",
           probs[0], probs[1]);
    printf("\"signal_sample\": [%.2f, %.2f, %.2f]}\n", input_walking[0],
           input_walking[1], input_walking[2]);
  } else {
    printf("=== 1D CNN Time Series Classification ===\n\n");
    printf("Scenario: Human Activity Recognition (HAR)\n");
    printf("Input: Synthesized 'Walking' accelerometer data (50 steps)\n");
    printf("Architecture: Conv1D(3x8) -> MaxPool(2) -> FC(2) -> Softmax\n\n");
    printf("Result:\n");
    printf("  Prediction: %s\n", CLASSES[pred_class]);
    printf("  Confidence: %.2f%%\n", probs[pred_class] * 100);
    printf("  (Standing: %.2f, Walking: %.2f)\n", probs[0], probs[1]);
  }

  return 0;
}
