/**
 * @file main.c
 * @brief RNN Sequence Classifier Demo
 *
 * Demonstrates using RNN/LSTM for sequence classification.
 * Example: Classify gesture sequences from accelerometer data.
 *
 * Build: make sequence_classifier
 * Run:   ./bin/sequence_classifier --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Include EIF RNN header
#include "eif_rnn.h"

// =============================================================================
// Configuration
// =============================================================================

#define INPUT_SIZE 3   // 3-axis accelerometer
#define HIDDEN_SIZE 16 // LSTM hidden units
#define OUTPUT_SIZE 4  // 4 gesture classes
#define SEQ_LEN 20     // 20 timesteps

// Gesture classes
typedef enum {
  GESTURE_NONE = 0,
  GESTURE_WAVE,
  GESTURE_CIRCLE,
  GESTURE_SWIPE,
  GESTURE_TAP
} gesture_t;

static const char *gesture_names[] = {"None", "Wave", "Circle", "Swipe", "Tap"};

// =============================================================================
// Simulated LSTM Weights (random for demo)
// =============================================================================

// In real use, these would come from trained model
static int16_t w_f[(INPUT_SIZE + HIDDEN_SIZE) * HIDDEN_SIZE];
static int16_t w_i[(INPUT_SIZE + HIDDEN_SIZE) * HIDDEN_SIZE];
static int16_t w_c[(INPUT_SIZE + HIDDEN_SIZE) * HIDDEN_SIZE];
static int16_t w_o[(INPUT_SIZE + HIDDEN_SIZE) * HIDDEN_SIZE];
static int16_t b_f[HIDDEN_SIZE];
static int16_t b_i[HIDDEN_SIZE];
static int16_t b_c[HIDDEN_SIZE];
static int16_t b_o[HIDDEN_SIZE];
static int16_t h_state[HIDDEN_SIZE];
static int16_t c_state[HIDDEN_SIZE];

// Output dense layer
static int16_t w_dense[HIDDEN_SIZE * OUTPUT_SIZE];
static int16_t b_dense[OUTPUT_SIZE];

static void init_random_weights(void) {
  srand(42); // Fixed seed for reproducibility

  for (int i = 0; i < (INPUT_SIZE + HIDDEN_SIZE) * HIDDEN_SIZE; i++) {
    w_f[i] = (int16_t)(rand() % 1000 - 500);
    w_i[i] = (int16_t)(rand() % 1000 - 500);
    w_c[i] = (int16_t)(rand() % 1000 - 500);
    w_o[i] = (int16_t)(rand() % 1000 - 500);
  }

  for (int i = 0; i < HIDDEN_SIZE; i++) {
    b_f[i] = (int16_t)(rand() % 500); // Positive bias for forget gate
    b_i[i] = (int16_t)(rand() % 200 - 100);
    b_c[i] = (int16_t)(rand() % 200 - 100);
    b_o[i] = (int16_t)(rand() % 200 - 100);
  }

  for (int i = 0; i < HIDDEN_SIZE * OUTPUT_SIZE; i++) {
    w_dense[i] = (int16_t)(rand() % 2000 - 1000);
  }
  for (int i = 0; i < OUTPUT_SIZE; i++) {
    b_dense[i] = (int16_t)(rand() % 200 - 100);
  }
}

// =============================================================================
// Sequence Generation
// =============================================================================

static void generate_gesture_sequence(gesture_t type, int16_t *seq) {
  float t;
  float noise;

  for (int i = 0; i < SEQ_LEN; i++) {
    t = (float)i / SEQ_LEN;
    noise = ((float)(rand() % 100) / 500.0f - 0.1f);

    float ax = 0, ay = 0, az = 9.81f;

    switch (type) {
    case GESTURE_WAVE:
      // Side-to-side oscillation
      ax = 8.0f * sinf(2 * 3.14159f * 3 * t);
      ay = 2.0f * sinf(2 * 3.14159f * 2 * t);
      break;

    case GESTURE_CIRCLE:
      // Circular motion
      ax = 6.0f * cosf(2 * 3.14159f * t);
      ay = 6.0f * sinf(2 * 3.14159f * t);
      break;

    case GESTURE_SWIPE:
      // Quick linear motion
      if (i > SEQ_LEN / 4 && i < SEQ_LEN * 3 / 4) {
        ax = 12.0f * sinf(3.14159f * (i - SEQ_LEN / 4) / (SEQ_LEN / 2));
      }
      break;

    case GESTURE_TAP:
      // Sharp spike
      if (i > SEQ_LEN / 3 && i < SEQ_LEN / 3 + 3) {
        az += 15.0f;
      }
      break;

    default:
      break;
    }

    // Convert to Q15 (scale by ~3000 for reasonable range)
    seq[i * INPUT_SIZE + 0] = (int16_t)((ax + noise) * 3000);
    seq[i * INPUT_SIZE + 1] = (int16_t)((ay + noise) * 3000);
    seq[i * INPUT_SIZE + 2] = (int16_t)((az + noise * 0.5f) * 3000);
  }
}

// =============================================================================
// Classification
// =============================================================================

static int dense_layer(const int16_t *input, int16_t *output,
                       const int16_t *weights, const int16_t *bias, int in_size,
                       int out_size) {
  for (int i = 0; i < out_size; i++) {
    int32_t acc = 0;
    for (int j = 0; j < in_size; j++) {
      acc += (int32_t)weights[j * out_size + i] * input[j];
    }
    output[i] = (int16_t)((acc >> 15) + bias[i]);
  }
  return 0;
}

static int softmax_q15(int16_t *x, int len) {
  // Find max for numerical stability
  int16_t max_val = x[0];
  for (int i = 1; i < len; i++) {
    if (x[i] > max_val)
      max_val = x[i];
  }

  // Simple softmax approximation
  int32_t sum = 0;
  for (int i = 0; i < len; i++) {
    x[i] = x[i] - max_val;                         // Subtract max
    x[i] = (x[i] > 0) ? x[i] : (x[i] + 16384) / 2; // Approximate exp
    sum += x[i];
  }

  // Normalize
  if (sum > 0) {
    for (int i = 0; i < len; i++) {
      x[i] = (int16_t)(((int32_t)x[i] * 32767) / sum);
    }
  }

  return 0;
}

static int argmax(const int16_t *x, int len) {
  int max_idx = 0;
  for (int i = 1; i < len; i++) {
    if (x[i] > x[max_idx])
      max_idx = i;
  }
  return max_idx;
}

static gesture_t classify_sequence(const int16_t *seq) {
  // Setup LSTM cell
  eif_lstm_cell_t lstm = {.input_size = INPUT_SIZE,
                          .hidden_size = HIDDEN_SIZE,
                          .W_f = w_f,
                          .W_i = w_i,
                          .W_c = w_c,
                          .W_o = w_o,
                          .b_f = b_f,
                          .b_i = b_i,
                          .b_c = b_c,
                          .b_o = b_o,
                          .h = h_state,
                          .c = c_state,
                          .stateful = false};

  // Process sequence
  int16_t final_h[HIDDEN_SIZE];
  eif_lstm_sequence(&lstm, seq, SEQ_LEN, NULL, false, final_h);

  // Dense output layer
  int16_t logits[OUTPUT_SIZE];
  dense_layer(final_h, logits, w_dense, b_dense, HIDDEN_SIZE, OUTPUT_SIZE);

  // Softmax and classify
  softmax_q15(logits, OUTPUT_SIZE);

  return (gesture_t)(argmax(logits, OUTPUT_SIZE) + 1); // +1 to skip NONE
}

// =============================================================================
// Visualization
// =============================================================================

static void print_sequence_waveform(const int16_t *seq, int axis) {
  const char *blocks[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

  for (int i = 0; i < SEQ_LEN; i++) {
    float val = seq[i * INPUT_SIZE + axis] / 32767.0f;
    val = (val + 1.0f) / 2.0f; // Normalize to [0, 1]
    int idx = (int)(val * 7.99f);
    if (idx < 0)
      idx = 0;
    if (idx > 7)
      idx = 7;
    printf("%s", blocks[idx]);
  }
}

static void print_header(void) {
  printf(
      "\n╔═══════════════════════════════════════════════════════════════╗\n");
  printf(
      "║          🧠 RNN Sequence Classifier Demo                       ║\n");
  printf("╠═══════════════════════════════════════════════════════════════╣\n");
  printf(
      "║  LSTM network classifying accelerometer gesture sequences      ║\n");
  printf(
      "╚═══════════════════════════════════════════════════════════════╝\n\n");
}

// =============================================================================
// Demo Runner
// =============================================================================

static void run_demo(gesture_t expected, bool batch_mode) {
  int16_t seq[SEQ_LEN * INPUT_SIZE];

  generate_gesture_sequence(expected, seq);
  gesture_t predicted = classify_sequence(seq);

  if (batch_mode) {
    printf("  %-8s X:", gesture_names[expected]);
    print_sequence_waveform(seq, 0);
    printf(" Y:");
    print_sequence_waveform(seq, 1);

    if (predicted == expected) {
      printf(" → ✅ %s\n", gesture_names[predicted]);
    } else {
      printf(" → ⚠️  %s\n", gesture_names[predicted]);
    }
  } else {
    printf("Gesture: %s\n", gesture_names[expected]);
    printf("─────────────────────────────────────\n");
    printf("  X-axis: ");
    print_sequence_waveform(seq, 0);
    printf("\n  Y-axis: ");
    print_sequence_waveform(seq, 1);
    printf("\n  Z-axis: ");
    print_sequence_waveform(seq, 2);
    printf("\n\n");
    printf("Predicted: %s %s\n", gesture_names[predicted],
           (predicted == expected) ? "✅" : "❌");
  }
}

static void run_all_demos(bool batch_mode) {
  gesture_t gestures[] = {GESTURE_WAVE, GESTURE_CIRCLE, GESTURE_SWIPE,
                          GESTURE_TAP};
  int n = sizeof(gestures) / sizeof(gestures[0]);

  print_header();

  printf("Testing %d gesture types with LSTM classifier:\n\n", n);

  int correct = 0;
  for (int i = 0; i < n; i++) {
    run_demo(gestures[i], batch_mode);
    // Note: with random weights, accuracy will be random
  }

  printf("\n═══════════════════════════════════════════════════════════════\n");
  printf(
      "Demo complete! Note: Random weights used - train for real accuracy.\n");
  printf("\nLSTM Configuration:\n");
  printf("  Input size:  %d (3-axis accel)\n", INPUT_SIZE);
  printf("  Hidden size: %d\n", HIDDEN_SIZE);
  printf("  Sequence:    %d timesteps\n", SEQ_LEN);
  printf("  Output:      %d classes\n", OUTPUT_SIZE);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char *argv[]) {
  srand((unsigned int)time(NULL));

  bool batch_mode = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--batch") == 0 || strcmp(argv[i], "-b") == 0) {
      batch_mode = true;
    }
  }

  // Initialize weights
  init_random_weights();

  // Run demos
  run_all_demos(batch_mode);

  return 0;
}
