/**
 * @file eif_rnn.h
 * @brief Recurrent Neural Network (RNN) layers for EIF
 *
 * Provides Simple RNN, LSTM, and GRU cells optimized for MCU.
 * Uses Q15 fixed-point arithmetic.
 *
 * Features:
 * - Simple RNN cell with configurable activation
 * - LSTM cell with forget, input, cell, output gates
 * - GRU cell with reset and update gates
 * - Stateful option for streaming inference
 * - Memory-efficient implementation
 */

#ifndef EIF_RNN_H
#define EIF_RNN_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

#ifndef EIF_RNN_MAX_HIDDEN
#define EIF_RNN_MAX_HIDDEN 128 // Maximum hidden state size
#endif

#ifndef EIF_RNN_MAX_INPUT
#define EIF_RNN_MAX_INPUT 64 // Maximum input size
#endif

// =============================================================================
// Fixed-Point Activation LUTs
// =============================================================================

/**
 * @brief Q15 sigmoid lookup table (256 entries)
 * Maps input range [-8, 8] to sigmoid output [0, 1] in Q15
 */
static const int16_t EIF_SIGMOID_LUT[256] = {
    // Generated for x in [-8, 8], output = 1/(1+exp(-x)) * 32767
    11, 12, 13, 14, 15, 17, 18, 20, 22, 24, 26, 28, 31, 34, 37, 40, 44, 48, 52,
    57, 62, 68, 74, 80, 88, 96, 104, 114, 124, 135, 147, 161, 175, 191, 208,
    227, 247, 269, 293, 320, 348, 379, 413, 450, 490, 534, 581, 633, 689, 750,
    816, 889, 967, 1053, 1146, 1247, 1357, 1477, 1607, 1749, 1903, 2071, 2254,
    2453, 2670, 2906, 3163, 3443, 3747, 4078, 4438, 4828, 5252, 5713, 6213,
    6754, 7341, 7976, 8662, 9403, 10202, 11061, 11983, 12970, 14024, 15147,
    16340, 17604, 18939, 20345, 21820, 23362, 24969, 26636, 28361, 30138, 31963,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    // Symmetric half
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    31963, 30138, 28361, 26636, 24969, 23362, 21820, 20345, 18939, 17604, 16340,
    15147, 14024, 12970, 11983, 11061, 10202, 9403, 8662, 7976, 7341, 6754,
    6213, 5713, 5252, 4828, 4438, 4078, 3747, 3443, 3163, 2906, 2670, 2453,
    2254, 2071, 1903, 1749, 1607, 1477, 1357, 1247, 1146, 1053, 967, 889, 816,
    750, 689, 633, 581, 534, 490, 450, 413, 379, 348, 320, 293, 269, 247, 227,
    208, 191, 175, 161, 147, 135, 124, 114, 104, 96, 88, 80, 74, 68, 62, 57, 52,
    48, 44, 40, 37, 34, 31, 28, 26, 24, 22, 20, 18, 17, 15, 14, 13};

/**
 * @brief Q15 tanh lookup table (256 entries)
 * Maps input range [-4, 4] to tanh output [-1, 1] in Q15
 */
static const int16_t EIF_TANH_LUT[256] = {
    // Generated for x in [-4, 4], output = tanh(x) * 32767
    -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
    -32766, -32765, -32763, -32760, -32755, -32748, -32738, -32724, -32706,
    -32682, -32651, -32612, -32563, -32502, -32427, -32335, -32223, -32090,
    -31930, -31741, -31519, -31260, -30960, -30615, -30222, -29776, -29274,
    -28713, -28091, -27406, -26656, -25841, -24961, -24019, -23017, -21959,
    -20849, -19694, -18498, -17268, -16012, -14737, -13451, -12162, -10877,
    -9604, -8350, -7122, -5926, -4767, -3649, -2576, -1551, -576, 347, 1216,
    2032, 2795, 3507, 4170, 4785, 5356, 5884, 6372, 6822, 7238, 7621, 7974,
    8299, 8599, 8875, 9129, 9363, 9579, 9778, 9962, 10131, 10287, 10431, 10564,
    10687, 10800, 10904, 11001, 11090, 11172, 11248, 11318, 11383, 11443, 11498,
    11550, 11597, 11641, 11682, 11720, 11755, 11787, 11817, 11845, 11871, 11895,
    11917, 11937, 11956, 11974, 11990, 12005, 12019, 12032, 12044, 12055, 12065,
    12075, 12084, 12092, 12100, 12107, 12113,
    // Symmetric positive half
    12113, 12107, 12100, 12092, 12084, 12075, 12065, 12055, 12044, 12032, 12019,
    12005, 11990, 11974, 11956, 11937, 11917, 11895, 11871, 11845, 11817, 11787,
    11755, 11720, 11682, 11641, 11597, 11550, 11498, 11443, 11383, 11318, 11248,
    11172, 11090, 11001, 10904, 10800, 10687, 10564, 10431, 10287, 10131, 9962,
    9778, 9579, 9363, 9129, 8875, 8599, 8299, 7974, 7621, 7238, 6822, 6372,
    5884, 5356, 4785, 4170, 3507, 2795, 2032, 1216, 347, -576, -1551, -2576,
    -3649, -4767, -5926, -7122, -8350, -9604, -10877, -12162, -13451, -14737,
    -16012, -17268, -18498, -19694, -20849, -21959, -23017, -24019, -24961,
    -25841, -26656, -27406, -28091, -28713, -29274, -29776, -30222, -30615,
    -30960, -31260, -31519, -31741, -31930, -32090, -32223, -32335, -32427,
    -32502, -32563, -32612, -32651, -32682, -32706, -32724, -32738, -32748,
    -32755, -32760, -32763, -32765, -32766, -32767, -32767, -32767, -32767,
    -32767, -32767, -32767, -32767, -32767};

/**
 * @brief Q15 sigmoid using LUT
 */
static inline int16_t eif_sigmoid_q15(int32_t x) {
  // Scale input to LUT range [-8, 8] -> [0, 255]
  int32_t idx = ((x >> 10) + 128); // Assumes Q15 input
  if (idx < 0)
    idx = 0;
  if (idx > 255)
    idx = 255;
  return EIF_SIGMOID_LUT[idx];
}

/**
 * @brief Q15 tanh using LUT
 */
static inline int16_t eif_tanh_q15(int32_t x) {
  // Scale input to LUT range [-4, 4] -> [0, 255]
  int32_t idx = ((x >> 9) + 128); // Assumes Q15 input
  if (idx < 0)
    idx = 0;
  if (idx > 255)
    idx = 255;
  return EIF_TANH_LUT[idx];
}

// =============================================================================
// Simple RNN Cell
// =============================================================================

/**
 * @brief Simple RNN cell configuration
 */
typedef struct {
  int input_size;  ///< Input dimension
  int hidden_size; ///< Hidden state dimension

  const int16_t *W_ih; ///< Input-to-hidden weights [input_size x hidden_size]
  const int16_t *W_hh; ///< Hidden-to-hidden weights [hidden_size x hidden_size]
  const int16_t *b_ih; ///< Input bias [hidden_size]
  const int16_t *b_hh; ///< Hidden bias [hidden_size]

  int16_t *h; ///< Hidden state buffer [hidden_size]

  bool use_tanh; ///< Use tanh (true) or ReLU (false)
  bool stateful; ///< Preserve state between sequences
} eif_rnn_cell_t;

/**
 * @brief Initialize RNN cell with zeros
 */
static inline void eif_rnn_init(eif_rnn_cell_t *rnn) {
  if (rnn->h) {
    memset(rnn->h, 0, rnn->hidden_size * sizeof(int16_t));
  }
}

/**
 * @brief Reset RNN hidden state
 */
static inline void eif_rnn_reset(eif_rnn_cell_t *rnn) { eif_rnn_init(rnn); }

/**
 * @brief Process one timestep through RNN cell
 * h_t = activation(W_ih * x_t + b_ih + W_hh * h_{t-1} + b_hh)
 *
 * @param rnn RNN cell configuration
 * @param x Input vector [input_size]
 * @param h_out Output hidden state [hidden_size]
 */
static inline void eif_rnn_step(eif_rnn_cell_t *rnn, const int16_t *x,
                                int16_t *h_out) {
  int32_t acc;

  for (int i = 0; i < rnn->hidden_size; i++) {
    acc = 0;

    // Input contribution: W_ih * x
    for (int j = 0; j < rnn->input_size; j++) {
      acc += (int32_t)rnn->W_ih[j * rnn->hidden_size + i] * x[j];
    }

    // Hidden contribution: W_hh * h
    for (int j = 0; j < rnn->hidden_size; j++) {
      acc += (int32_t)rnn->W_hh[j * rnn->hidden_size + i] * rnn->h[j];
    }

    // Add biases
    acc = (acc >> 15); // Q15 * Q15 -> Q30, shift to Q15
    acc += rnn->b_ih[i] + rnn->b_hh[i];

    // Apply activation
    if (rnn->use_tanh) {
      h_out[i] = eif_tanh_q15(acc);
    } else {
      // ReLU
      h_out[i] = (acc > 0) ? ((acc > 32767) ? 32767 : (int16_t)acc) : 0;
    }
  }

  // Update hidden state
  memcpy(rnn->h, h_out, rnn->hidden_size * sizeof(int16_t));
}

/**
 * @brief Process sequence through RNN
 *
 * @param rnn RNN cell
 * @param x_seq Input sequence [seq_len x input_size]
 * @param seq_len Sequence length
 * @param y_seq Output sequence [seq_len x hidden_size] or NULL
 * @param return_sequences If true, output all timesteps
 * @param final_h Final hidden state output [hidden_size]
 */
static inline void eif_rnn_sequence(eif_rnn_cell_t *rnn, const int16_t *x_seq,
                                    int seq_len, int16_t *y_seq,
                                    bool return_sequences, int16_t *final_h) {
  if (!rnn->stateful) {
    eif_rnn_reset(rnn);
  }

  for (int t = 0; t < seq_len; t++) {
    const int16_t *x_t = &x_seq[t * rnn->input_size];
    int16_t *y_t = return_sequences ? &y_seq[t * rnn->hidden_size] : final_h;

    eif_rnn_step(rnn, x_t, y_t);
  }

  // Copy final state if not returning sequences
  if (!return_sequences && final_h != rnn->h) {
    memcpy(final_h, rnn->h, rnn->hidden_size * sizeof(int16_t));
  }
}

// =============================================================================
// LSTM Cell
// =============================================================================

/**
 * @brief LSTM cell configuration
 *
 * LSTM equations:
 *   f_t = sigmoid(W_f * [h_{t-1}, x_t] + b_f)  // Forget gate
 *   i_t = sigmoid(W_i * [h_{t-1}, x_t] + b_i)  // Input gate
 *   c̃_t = tanh(W_c * [h_{t-1}, x_t] + b_c)    // Candidate
 *   c_t = f_t * c_{t-1} + i_t * c̃_t           // Cell state
 *   o_t = sigmoid(W_o * [h_{t-1}, x_t] + b_o)  // Output gate
 *   h_t = o_t * tanh(c_t)                      // Hidden state
 */
typedef struct {
  int input_size;  ///< Input dimension
  int hidden_size; ///< Hidden/cell state dimension

  // Weights for each gate [input_size + hidden_size, hidden_size]
  const int16_t *W_f; ///< Forget gate weights
  const int16_t *W_i; ///< Input gate weights
  const int16_t *W_c; ///< Cell candidate weights
  const int16_t *W_o; ///< Output gate weights

  // Biases [hidden_size]
  const int16_t *b_f;
  const int16_t *b_i;
  const int16_t *b_c;
  const int16_t *b_o;

  // States
  int16_t *h; ///< Hidden state [hidden_size]
  int16_t *c; ///< Cell state [hidden_size]

  bool stateful; ///< Preserve state between sequences
} eif_lstm_cell_t;

/**
 * @brief Initialize LSTM cell
 */
static inline void eif_lstm_init(eif_lstm_cell_t *lstm) {
  if (lstm->h)
    memset(lstm->h, 0, lstm->hidden_size * sizeof(int16_t));
  if (lstm->c)
    memset(lstm->c, 0, lstm->hidden_size * sizeof(int16_t));
}

/**
 * @brief Reset LSTM states
 */
static inline void eif_lstm_reset(eif_lstm_cell_t *lstm) {
  eif_lstm_init(lstm);
}

/**
 * @brief Compute gate output (shared logic for f, i, o gates)
 */
static inline int16_t eif_lstm_gate(const int16_t *W, const int16_t *b,
                                    const int16_t *x, int x_size,
                                    const int16_t *h, int h_size, int idx,
                                    bool use_sigmoid) {
  int32_t acc = 0;
  int total_size = x_size + h_size;

  // Input contribution
  for (int j = 0; j < x_size; j++) {
    acc += (int32_t)W[j * h_size + idx] * x[j];
  }

  // Hidden contribution
  for (int j = 0; j < h_size; j++) {
    acc += (int32_t)W[(x_size + j) * h_size + idx] * h[j];
  }

  acc = (acc >> 15) + b[idx];

  return use_sigmoid ? eif_sigmoid_q15(acc) : eif_tanh_q15(acc);
}

/**
 * @brief Process one timestep through LSTM cell
 */
static inline void eif_lstm_step(eif_lstm_cell_t *lstm, const int16_t *x,
                                 int16_t *h_out) {
  int16_t f, i, c_tilde, o;
  int32_t c_new;

  for (int k = 0; k < lstm->hidden_size; k++) {
    // Forget gate
    f = eif_lstm_gate(lstm->W_f, lstm->b_f, x, lstm->input_size, lstm->h,
                      lstm->hidden_size, k, true);

    // Input gate
    i = eif_lstm_gate(lstm->W_i, lstm->b_i, x, lstm->input_size, lstm->h,
                      lstm->hidden_size, k, true);

    // Cell candidate
    c_tilde = eif_lstm_gate(lstm->W_c, lstm->b_c, x, lstm->input_size, lstm->h,
                            lstm->hidden_size, k, false);

    // Output gate
    o = eif_lstm_gate(lstm->W_o, lstm->b_o, x, lstm->input_size, lstm->h,
                      lstm->hidden_size, k, true);

    // New cell state: c_t = f * c_{t-1} + i * c_tilde
    c_new = ((int32_t)f * lstm->c[k] + (int32_t)i * c_tilde) >> 15;
    lstm->c[k] =
        (int16_t)((c_new > 32767) ? 32767
                                  : ((c_new < -32768) ? -32768 : c_new));

    // New hidden state: h_t = o * tanh(c_t)
    int16_t tanh_c = eif_tanh_q15(lstm->c[k]);
    h_out[k] = (int16_t)(((int32_t)o * tanh_c) >> 15);
  }

  // Update hidden state
  memcpy(lstm->h, h_out, lstm->hidden_size * sizeof(int16_t));
}

/**
 * @brief Process sequence through LSTM
 */
static inline void eif_lstm_sequence(eif_lstm_cell_t *lstm,
                                     const int16_t *x_seq, int seq_len,
                                     int16_t *y_seq, bool return_sequences,
                                     int16_t *final_h) {
  if (!lstm->stateful) {
    eif_lstm_reset(lstm);
  }

  for (int t = 0; t < seq_len; t++) {
    const int16_t *x_t = &x_seq[t * lstm->input_size];
    int16_t *y_t = return_sequences ? &y_seq[t * lstm->hidden_size] : final_h;

    eif_lstm_step(lstm, x_t, y_t);
  }

  if (!return_sequences && final_h != lstm->h) {
    memcpy(final_h, lstm->h, lstm->hidden_size * sizeof(int16_t));
  }
}

// =============================================================================
// GRU Cell
// =============================================================================

/**
 * @brief GRU cell configuration
 *
 * GRU equations:
 *   r_t = sigmoid(W_r * [h_{t-1}, x_t] + b_r)  // Reset gate
 *   z_t = sigmoid(W_z * [h_{t-1}, x_t] + b_z)  // Update gate
 *   h̃_t = tanh(W_h * [r_t * h_{t-1}, x_t] + b_h)  // Candidate
 *   h_t = (1 - z_t) * h_{t-1} + z_t * h̃_t    // Hidden state
 */
typedef struct {
  int input_size;
  int hidden_size;

  const int16_t *W_r; ///< Reset gate weights
  const int16_t *W_z; ///< Update gate weights
  const int16_t *W_h; ///< Candidate weights

  const int16_t *b_r;
  const int16_t *b_z;
  const int16_t *b_h;

  int16_t *h; ///< Hidden state

  bool stateful;
} eif_gru_cell_t;

/**
 * @brief Initialize GRU cell
 */
static inline void eif_gru_init(eif_gru_cell_t *gru) {
  if (gru->h)
    memset(gru->h, 0, gru->hidden_size * sizeof(int16_t));
}

/**
 * @brief Reset GRU state
 */
static inline void eif_gru_reset(eif_gru_cell_t *gru) { eif_gru_init(gru); }

/**
 * @brief Process one timestep through GRU cell
 */
static inline void eif_gru_step(eif_gru_cell_t *gru, const int16_t *x,
                                int16_t *h_out) {
  int16_t r, z, h_tilde;
  int32_t acc, h_new;

  for (int k = 0; k < gru->hidden_size; k++) {
    // Reset gate
    acc = 0;
    for (int j = 0; j < gru->input_size; j++) {
      acc += (int32_t)gru->W_r[j * gru->hidden_size + k] * x[j];
    }
    for (int j = 0; j < gru->hidden_size; j++) {
      acc += (int32_t)gru->W_r[(gru->input_size + j) * gru->hidden_size + k] *
             gru->h[j];
    }
    r = eif_sigmoid_q15((acc >> 15) + gru->b_r[k]);

    // Update gate
    acc = 0;
    for (int j = 0; j < gru->input_size; j++) {
      acc += (int32_t)gru->W_z[j * gru->hidden_size + k] * x[j];
    }
    for (int j = 0; j < gru->hidden_size; j++) {
      acc += (int32_t)gru->W_z[(gru->input_size + j) * gru->hidden_size + k] *
             gru->h[j];
    }
    z = eif_sigmoid_q15((acc >> 15) + gru->b_z[k]);

    // Candidate hidden state
    acc = 0;
    for (int j = 0; j < gru->input_size; j++) {
      acc += (int32_t)gru->W_h[j * gru->hidden_size + k] * x[j];
    }
    for (int j = 0; j < gru->hidden_size; j++) {
      int16_t r_h = (int16_t)(((int32_t)r * gru->h[j]) >> 15);
      acc +=
          (int32_t)gru->W_h[(gru->input_size + j) * gru->hidden_size + k] * r_h;
    }
    h_tilde = eif_tanh_q15((acc >> 15) + gru->b_h[k]);

    // New hidden state: h_t = (1-z) * h_{t-1} + z * h_tilde
    int16_t one_minus_z = 32767 - z;
    h_new = ((int32_t)one_minus_z * gru->h[k] + (int32_t)z * h_tilde) >> 15;
    h_out[k] = (int16_t)((h_new > 32767) ? 32767
                                         : ((h_new < -32768) ? -32768 : h_new));
  }

  memcpy(gru->h, h_out, gru->hidden_size * sizeof(int16_t));
}

/**
 * @brief Process sequence through GRU
 */
static inline void eif_gru_sequence(eif_gru_cell_t *gru, const int16_t *x_seq,
                                    int seq_len, int16_t *y_seq,
                                    bool return_sequences, int16_t *final_h) {
  if (!gru->stateful) {
    eif_gru_reset(gru);
  }

  for (int t = 0; t < seq_len; t++) {
    const int16_t *x_t = &x_seq[t * gru->input_size];
    int16_t *y_t = return_sequences ? &y_seq[t * gru->hidden_size] : final_h;

    eif_gru_step(gru, x_t, y_t);
  }

  if (!return_sequences && final_h != gru->h) {
    memcpy(final_h, gru->h, gru->hidden_size * sizeof(int16_t));
  }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_RNN_H
