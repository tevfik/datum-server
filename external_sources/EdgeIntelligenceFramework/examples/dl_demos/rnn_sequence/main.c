/**
 * @file main.c
 * @brief RNN Sequence Prediction Demo
 *
 * Demonstrates GRU/LSTM for sequence tasks:
 * - Sine wave prediction
 * - Pattern generation
 * - Time series forecasting
 *
 * Usage:
 *   ./rnn_sequence_demo --help
 *   ./rnn_sequence_demo --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"
#include "eif_nn_rnn.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SEQ_LEN 50
#define HIDDEN_DIM 8
#define INPUT_DIM 1

static bool json_mode = false;

// Generate sine wave sequence
static void generate_sine_sequence(float *seq, int len, float freq,
                                   float phase) {
  for (int i = 0; i < len; i++) {
    seq[i] = sinf(2.0f * M_PI * freq * i / len + phase);
  }
}

// Simple random weights initialization
static void init_random_weights(float *w, int size, float scale) {
  for (int i = 0; i < size; i++) {
    w[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * scale;
  }
}

// Demo: GRU cell basics
static void demo_gru_basics(void) {
  if (!json_mode) {
    ascii_section("1. GRU Cell Architecture");
    printf("  Gated Recurrent Unit for sequence modeling\n\n");
  }

  if (!json_mode) {
    printf("  GRU Gates:\n");
    printf("    • z (update gate): how much of old state to keep\n");
    printf("    • r (reset gate): how much of old state affects candidate\n");
    printf("    • h_candidate: new state proposal\n\n");

    printf("  Equations:\n");
    printf("    z = sigmoid(Wz*x + Uz*h + bz)\n");
    printf("    r = sigmoid(Wr*x + Ur*h + br)\n");
    printf("    h_cand = tanh(Wh*x + Uh*(r⊙h) + bh)\n");
    printf("    h_new = (1-z)⊙h + z⊙h_cand\n\n");

    printf("  Memory footprint:\n");
    printf("    Weights: 3 × (input × hidden + hidden × hidden)\n");
    printf("    State: hidden_dim floats\n");
  }
}

// Demo: LSTM cell basics
static void demo_lstm_basics(void) {
  if (!json_mode) {
    ascii_section("2. LSTM Cell Architecture");
    printf("  Long Short-Term Memory with cell state\n\n");
  }

  if (!json_mode) {
    printf("  LSTM Gates:\n");
    printf("    • f (forget gate): what to forget from cell state\n");
    printf("    • i (input gate): what new info to add\n");
    printf("    • o (output gate): what to output from cell\n");
    printf("    • c (cell state): long-term memory\n\n");

    printf("  Equations:\n");
    printf("    f = sigmoid(Wf*x + Uf*h + bf)\n");
    printf("    i = sigmoid(Wi*x + Ui*h + bi)\n");
    printf("    o = sigmoid(Wo*x + Uo*h + bo)\n");
    printf("    c_cand = tanh(Wc*x + Uc*h + bc)\n");
    printf("    c_new = f⊙c + i⊙c_cand\n");
    printf("    h_new = o⊙tanh(c_new)\n\n");

    printf("  LSTM vs GRU:\n");
    printf("    • LSTM: 4 gates, separate cell state\n");
    printf("    • GRU: 3 gates, merged cell/hidden state\n");
    printf("    • GRU is ~25%% fewer parameters\n");
  }
}

// Demo: Sequence processing
static void demo_sequence_processing(void) {
  if (!json_mode) {
    ascii_section("3. Sequence Processing");
    printf("  Processing time series data\n\n");
  }

  // Initialize GRU
  eif_gru_t gru;
  eif_gru_init(&gru, INPUT_DIM, HIDDEN_DIM);

  // Allocate weights (simplified - would normally load from trained model)
  float Wz_x[INPUT_DIM * HIDDEN_DIM], Wr_x[INPUT_DIM * HIDDEN_DIM],
      Wh_x[INPUT_DIM * HIDDEN_DIM];
  float Wz_h[HIDDEN_DIM * HIDDEN_DIM], Wr_h[HIDDEN_DIM * HIDDEN_DIM],
      Wh_h[HIDDEN_DIM * HIDDEN_DIM];
  float bz[HIDDEN_DIM], br[HIDDEN_DIM], bh[HIDDEN_DIM];

  srand(42);
  init_random_weights(Wz_x, INPUT_DIM * HIDDEN_DIM, 0.1f);
  init_random_weights(Wr_x, INPUT_DIM * HIDDEN_DIM, 0.1f);
  init_random_weights(Wh_x, INPUT_DIM * HIDDEN_DIM, 0.1f);
  init_random_weights(Wz_h, HIDDEN_DIM * HIDDEN_DIM, 0.1f);
  init_random_weights(Wr_h, HIDDEN_DIM * HIDDEN_DIM, 0.1f);
  init_random_weights(Wh_h, HIDDEN_DIM * HIDDEN_DIM, 0.1f);
  for (int i = 0; i < HIDDEN_DIM; i++) {
    bz[i] = 0;
    br[i] = 0;
    bh[i] = 0;
  }

  gru.Wz_x = Wz_x;
  gru.Wr_x = Wr_x;
  gru.Wh_x = Wh_x;
  gru.Wz_h = Wz_h;
  gru.Wr_h = Wr_h;
  gru.Wh_h = Wh_h;
  gru.bz = bz;
  gru.br = br;
  gru.bh = bh;

  // Generate input sequence
  float input_seq[SEQ_LEN];
  generate_sine_sequence(input_seq, SEQ_LEN, 2.0f, 0.0f);

  // Process sequence
  float hidden_norms[SEQ_LEN];
  float output[HIDDEN_DIM];

  for (int t = 0; t < SEQ_LEN; t++) {
    float x[1] = {input_seq[t]};
    eif_gru_forward(&gru, x, output);

    // Track hidden state norm
    float norm = 0;
    for (int i = 0; i < HIDDEN_DIM; i++) {
      norm += output[i] * output[i];
    }
    hidden_norms[t] = sqrtf(norm);
  }

  if (json_mode) {
    printf("{\"demo\": \"sequence_processing\", \"seq_len\": %d}\n", SEQ_LEN);
  } else {
    printf("  Input sequence (sine wave):\n");
    ascii_plot_waveform("Input", input_seq, SEQ_LEN, 50, 4);

    printf("\n  Hidden state evolution (norm):\n");
    ascii_plot_waveform("H_norm", hidden_norms, SEQ_LEN, 50, 4);

    printf("\n  The hidden state captures the temporal pattern.\n");
  }
}

// Demo: Use cases
static void demo_use_cases(void) {
  if (!json_mode) {
    ascii_section("4. RNN Use Cases for Edge AI");
    printf("  Applications for embedded systems\n\n");
  }

  if (!json_mode) {
    printf("  1. Time Series Forecasting\n");
    printf("     - Sensor prediction\n");
    printf("     - Anomaly detection\n");
    printf("     - Predictive maintenance\n\n");

    printf("  2. Gesture Recognition\n");
    printf("     - IMU sequence classification\n");
    printf("     - Continuous gesture streaming\n\n");

    printf("  3. Speech/Audio\n");
    printf("     - Wake word detection\n");
    printf("     - Keyword spotting\n\n");

    printf("  4. Text Processing\n");
    printf("     - Character-level language model\n");
    printf("     - Sequence-to-sequence tasks\n");
  }
}

int main(int argc, char **argv) {
  demo_cli_result_t result =
      demo_parse_args(argc, argv, "rnn_sequence_demo",
                      "GRU/LSTM sequence modeling demonstration");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF RNN Sequence Demo");
    printf("  Recurrent Neural Networks for time series\n\n");
  }

  demo_gru_basics();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_lstm_basics();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_sequence_processing();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_use_cases();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  RNN capabilities:\n");
    printf("    • GRU: 3 gates, efficient\n");
    printf("    • LSTM: 4 gates, long-term memory\n");
    printf("    • Sequence-to-sequence processing\n");
    printf("    • Temporal pattern recognition\n\n");
  }

  return 0;
}
