# RNN Fundamentals for Embedded Systems

Understanding Recurrent Neural Networks for sequence modeling on MCUs.

---

## Table of Contents

1. [What are RNNs?](#what-are-rnns)
2. [Simple RNN](#simple-rnn)
3. [LSTM](#lstm)
4. [GRU](#gru)
5. [Fixed-Point Implementation](#fixed-point-implementation)
6. [Memory Considerations](#memory-considerations)
7. [EIF API](#eif-api)

---

## What are RNNs?

Recurrent Neural Networks process **sequential data** by maintaining a hidden state that captures information from previous timesteps.

```
Input:  x₁ → x₂ → x₃ → ... → xₜ
         ↓    ↓    ↓           ↓
Hidden: h₁ → h₂ → h₃ → ... → hₜ
         ↓    ↓    ↓           ↓
Output: y₁   y₂   y₃         yₜ
```

### Use Cases on MCUs

| Application | Input | Output |
|-------------|-------|--------|
| Gesture Recognition | IMU sequence | Gesture class |
| Wake Word Detection | Audio frames | Wake/Not wake |
| Predictive Maintenance | Sensor readings | Anomaly score |
| Text Generation | Characters | Next character |

---

## Simple RNN

The simplest recurrent architecture:

```
h_t = tanh(W_ih * x_t + W_hh * h_{t-1} + b)
```

### Math Breakdown

1. **Input transformation**: W_ih × x_t
2. **Recurrent transformation**: W_hh × h_{t-1}
3. **Bias addition**: + b
4. **Non-linearity**: tanh(...)

### Code Example

```c
#include "eif_rnn.h"

// Define buffers
int16_t h_state[16];

// Configure cell
eif_rnn_cell_t rnn = {
    .input_size = 3,
    .hidden_size = 16,
    .W_ih = weights_ih,
    .W_hh = weights_hh,
    .b_ih = bias_ih,
    .b_hh = bias_hh,
    .h = h_state,
    .use_tanh = true,
    .stateful = false
};

// Reset and process
eif_rnn_reset(&rnn);
for (int t = 0; t < seq_len; t++) {
    eif_rnn_step(&rnn, &input[t * 3], output);
}
```

### Pros & Cons

| Pros | Cons |
|------|------|
| Simple, fast | Vanishing gradients |
| Low memory | Short-term memory only |
| Easy to train | Poor long sequences |

---

## LSTM

Long Short-Term Memory solves the vanishing gradient problem with **gates**:

```
        ┌──────────────────────────────────────┐
        │                Cell State            │
        │    c_{t-1} ─────×────────+──── c_t   │
        │                 ↑        ↑           │
        │              (forget)  (input)       │
        │                 f_t     i_t × c̃_t   │
        └──────────────────↑───────↑───────────┘
                           │       │
        ┌──────────────────┴───────┴───────────┐
        │          Gate Computations           │
        │                                      │
        │  f_t = σ(W_f · [h_{t-1}, x_t] + b_f) │
        │  i_t = σ(W_i · [h_{t-1}, x_t] + b_i) │
        │  c̃_t = tanh(W_c · [h,x] + b_c)      │
        │  o_t = σ(W_o · [h_{t-1}, x_t] + b_o) │
        │                                      │
        │  h_t = o_t × tanh(c_t)               │
        └──────────────────────────────────────┘
```

### Gates Explained

| Gate | Symbol | Function |
|------|--------|----------|
| **Forget** | f_t | What to forget from cell state |
| **Input** | i_t | What new information to add |
| **Output** | o_t | What to output from cell state |

### Code Example

```c
// LSTM buffers
int16_t h_state[32];
int16_t c_state[32];

eif_lstm_cell_t lstm = {
    .input_size = 13,    // MFCC features
    .hidden_size = 32,
    .W_f = w_forget, .W_i = w_input,
    .W_c = w_cell,   .W_o = w_output,
    .b_f = b_forget, .b_i = b_input,
    .b_c = b_cell,   .b_o = b_output,
    .h = h_state,
    .c = c_state,
    .stateful = true  // Keep state for streaming
};

// Process streaming audio
while (get_audio_frame(frame)) {
    eif_lstm_step(&lstm, frame, output);
    if (is_wake_word(output)) {
        trigger_assistant();
    }
}
```

---

## GRU

Gated Recurrent Unit - simpler than LSTM with similar performance:

```
r_t = σ(W_r · [h_{t-1}, x_t] + b_r)     // Reset gate
z_t = σ(W_z · [h_{t-1}, x_t] + b_z)     // Update gate
h̃_t = tanh(W_h · [r_t × h_{t-1}, x_t] + b_h)  // Candidate
h_t = (1 - z_t) × h_{t-1} + z_t × h̃_t  // New hidden
```

### GRU vs LSTM

| Feature | LSTM | GRU |
|---------|------|-----|
| Gates | 4 | 2 |
| States | h + c | h only |
| Parameters | More | ~25% fewer |
| Memory | Higher | Lower |
| Performance | Similar | Similar |

**Recommendation**: Use GRU for memory-constrained MCUs.

---

## Fixed-Point Implementation

### Q15 Format

All RNN operations use Q1.15 fixed-point:
- Range: -1.0 to +0.99997
- Resolution: 1/32768 ≈ 0.00003

### Activation LUTs

Sigmoid and tanh use 256-entry lookup tables:

```c
// Q15 sigmoid: input [-8,8] → output [0,1]
int16_t sigmoid_q15(int32_t x) {
    int idx = (x >> 10) + 128;
    if (idx < 0) idx = 0;
    if (idx > 255) idx = 255;
    return SIGMOID_LUT[idx];
}
```

### Overflow Prevention

```c
// Safe Q15 multiply with saturation
int32_t acc = (int32_t)a * b;
acc = acc >> 15;  // Q30 → Q15
if (acc > 32767) acc = 32767;
if (acc < -32768) acc = -32768;
```

---

## Memory Considerations

### Memory Usage Formula

```
Simple RNN:
  Weights: (input + hidden) × hidden × 2 bytes
  State:   hidden × 2 bytes
  
LSTM:
  Weights: 4 × (input + hidden) × hidden × 2 bytes
  State:   hidden × 4 bytes (h + c)
  
GRU:
  Weights: 3 × (input + hidden) × hidden × 2 bytes
  State:   hidden × 2 bytes
```

### Example: Wake Word LSTM

| Parameter | Value | Memory |
|-----------|-------|--------|
| Input size | 13 (MFCC) | - |
| Hidden size | 32 | - |
| Weights | 4 × 45 × 32 | 11.5 KB |
| States | 32 × 2 | 128 B |
| **Total** | - | **~12 KB** |

---

## EIF API

### Simple RNN

```c
void eif_rnn_init(eif_rnn_cell_t* rnn);
void eif_rnn_reset(eif_rnn_cell_t* rnn);
void eif_rnn_step(eif_rnn_cell_t* rnn, const int16_t* x, int16_t* h_out);
void eif_rnn_sequence(eif_rnn_cell_t* rnn, const int16_t* seq, int len,
                      int16_t* out, bool return_sequences, int16_t* final_h);
```

### LSTM

```c
void eif_lstm_init(eif_lstm_cell_t* lstm);
void eif_lstm_reset(eif_lstm_cell_t* lstm);
void eif_lstm_step(eif_lstm_cell_t* lstm, const int16_t* x, int16_t* h_out);
void eif_lstm_sequence(eif_lstm_cell_t* lstm, const int16_t* seq, int len,
                       int16_t* out, bool return_sequences, int16_t* final_h);
```

### GRU

```c
void eif_gru_init(eif_gru_cell_t* gru);
void eif_gru_reset(eif_gru_cell_t* gru);
void eif_gru_step(eif_gru_cell_t* gru, const int16_t* x, int16_t* h_out);
void eif_gru_sequence(eif_gru_cell_t* gru, const int16_t* seq, int len,
                      int16_t* out, bool return_sequences, int16_t* final_h);
```

---

## Next Steps

1. See [examples/rnn_demos/sequence_classifier](../../examples/rnn_demos/sequence_classifier) for working demo
2. Train your model with Keras and convert with eif_convert
3. Profile on target MCU
