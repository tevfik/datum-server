/**
 * @file eif_nn_rnn.h
 * @brief Recurrent Neural Network Cells (GRU, LSTM)
 *
 * Lightweight RNN implementations for embedded systems:
 * - GRU (Gated Recurrent Unit)
 * - LSTM (Long Short-Term Memory)
 *
 * Optimized for sequence processing on resource-constrained devices.
 */

#ifndef EIF_NN_RNN_H
#define EIF_NN_RNN_H

#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum hidden units (memory constraint)
#define EIF_RNN_MAX_HIDDEN 64
#define EIF_RNN_MAX_INPUT 64

// =============================================================================
// GRU Cell
// =============================================================================

/**
 * @brief GRU cell state
 *
 * Gated Recurrent Unit with reset and update gates.
 * Equations:
 *   z = sigmoid(Wz * [h, x])   // Update gate
 *   r = sigmoid(Wr * [h, x])   // Reset gate
 *   h_new = tanh(W * [r*h, x])
 *   h = (1-z)*h + z*h_new
 */
typedef struct {
  int input_size;
  int hidden_size;

  // Weights for input [hidden_size x input_size]
  float *Wz_x, *Wr_x, *Wh_x;
  // Weights for hidden [hidden_size x hidden_size]
  float *Wz_h, *Wr_h, *Wh_h;
  // Biases [hidden_size]
  float *bz, *br, *bh;

  // Hidden state
  float h[EIF_RNN_MAX_HIDDEN];
} eif_gru_t;

/**
 * @brief Sigmoid activation
 */
static inline float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }

/**
 * @brief Tanh activation
 */
static inline float tanh_act(float x) { return tanhf(x); }

/**
 * @brief Initialize GRU cell (weights should be set externally)
 */
static inline void eif_gru_init(eif_gru_t *gru, int input_size,
                                int hidden_size) {
  gru->input_size = input_size;
  gru->hidden_size = hidden_size;
  memset(gru->h, 0, sizeof(gru->h));
}

/**
 * @brief Reset GRU hidden state
 */
static inline void eif_gru_reset(eif_gru_t *gru) {
  memset(gru->h, 0, sizeof(gru->h));
}

/**
 * @brief Forward pass through GRU cell
 * @param input Input vector [input_size]
 * @param output Output vector [hidden_size]
 */
static inline void eif_gru_forward(eif_gru_t *gru, const float *input,
                                   float *output) {
  float z[EIF_RNN_MAX_HIDDEN], r[EIF_RNN_MAX_HIDDEN], h_new[EIF_RNN_MAX_HIDDEN];

  for (int i = 0; i < gru->hidden_size; i++) {
    // Compute gates
    float z_val = gru->bz[i];
    float r_val = gru->br[i];
    float h_val = gru->bh[i];

    // Input contribution
    for (int j = 0; j < gru->input_size; j++) {
      z_val += gru->Wz_x[i * gru->input_size + j] * input[j];
      r_val += gru->Wr_x[i * gru->input_size + j] * input[j];
      h_val += gru->Wh_x[i * gru->input_size + j] * input[j];
    }

    // Hidden contribution
    for (int j = 0; j < gru->hidden_size; j++) {
      z_val += gru->Wz_h[i * gru->hidden_size + j] * gru->h[j];
      r_val += gru->Wr_h[i * gru->hidden_size + j] * gru->h[j];
    }

    z[i] = sigmoid(z_val);
    r[i] = sigmoid(r_val);

    // Reset gate applied to hidden
    float reset_h = 0.0f;
    for (int j = 0; j < gru->hidden_size; j++) {
      reset_h += gru->Wh_h[i * gru->hidden_size + j] * (r[i] * gru->h[j]);
    }
    h_new[i] = tanh_act(h_val + reset_h);
  }

  // Update hidden state
  for (int i = 0; i < gru->hidden_size; i++) {
    gru->h[i] = (1.0f - z[i]) * gru->h[i] + z[i] * h_new[i];
    output[i] = gru->h[i];
  }
}

// =============================================================================
// LSTM Cell
// =============================================================================

/**
 * @brief LSTM cell state
 *
 * Long Short-Term Memory with input, forget, output gates.
 */
typedef struct {
  int input_size;
  int hidden_size;

  // Weights [hidden_size x (input_size + hidden_size)]
  float *Wi, *Wf, *Wc, *Wo; // Input, Forget, Cell, Output
  // Biases
  float *bi, *bf, *bc, *bo;

  // States
  float h[EIF_RNN_MAX_HIDDEN]; // Hidden state
  float c[EIF_RNN_MAX_HIDDEN]; // Cell state
} eif_lstm_t;

/**
 * @brief Initialize LSTM cell
 */
static inline void eif_lstm_init(eif_lstm_t *lstm, int input_size,
                                 int hidden_size) {
  lstm->input_size = input_size;
  lstm->hidden_size = hidden_size;
  memset(lstm->h, 0, sizeof(lstm->h));
  memset(lstm->c, 0, sizeof(lstm->c));
}

/**
 * @brief Reset LSTM states
 */
static inline void eif_lstm_reset(eif_lstm_t *lstm) {
  memset(lstm->h, 0, sizeof(lstm->h));
  memset(lstm->c, 0, sizeof(lstm->c));
}

/**
 * @brief Forward pass through LSTM cell
 */
static inline void eif_lstm_forward(eif_lstm_t *lstm, const float *input,
                                    float *output) {
  float i_gate[EIF_RNN_MAX_HIDDEN], f_gate[EIF_RNN_MAX_HIDDEN];
  float c_tilde[EIF_RNN_MAX_HIDDEN], o_gate[EIF_RNN_MAX_HIDDEN];

  int in_sz = lstm->input_size;
  int hid_sz = lstm->hidden_size;

  for (int i = 0; i < hid_sz; i++) {
    float i_val = lstm->bi[i];
    float f_val = lstm->bf[i];
    float c_val = lstm->bc[i];
    float o_val = lstm->bo[i];

    // Input contribution
    for (int j = 0; j < in_sz; j++) {
      i_val += lstm->Wi[i * (in_sz + hid_sz) + j] * input[j];
      f_val += lstm->Wf[i * (in_sz + hid_sz) + j] * input[j];
      c_val += lstm->Wc[i * (in_sz + hid_sz) + j] * input[j];
      o_val += lstm->Wo[i * (in_sz + hid_sz) + j] * input[j];
    }

    // Hidden contribution
    for (int j = 0; j < hid_sz; j++) {
      i_val += lstm->Wi[i * (in_sz + hid_sz) + in_sz + j] * lstm->h[j];
      f_val += lstm->Wf[i * (in_sz + hid_sz) + in_sz + j] * lstm->h[j];
      c_val += lstm->Wc[i * (in_sz + hid_sz) + in_sz + j] * lstm->h[j];
      o_val += lstm->Wo[i * (in_sz + hid_sz) + in_sz + j] * lstm->h[j];
    }

    i_gate[i] = sigmoid(i_val);
    f_gate[i] = sigmoid(f_val);
    c_tilde[i] = tanh_act(c_val);
    o_gate[i] = sigmoid(o_val);
  }

  // Update cell and hidden state
  for (int i = 0; i < hid_sz; i++) {
    lstm->c[i] = f_gate[i] * lstm->c[i] + i_gate[i] * c_tilde[i];
    lstm->h[i] = o_gate[i] * tanh_act(lstm->c[i]);
    output[i] = lstm->h[i];
  }
}

#ifdef __cplusplus
}
#endif

#endif // EIF_NN_RNN_H
