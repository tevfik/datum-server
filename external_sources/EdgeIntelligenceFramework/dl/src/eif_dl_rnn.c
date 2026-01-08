#include "eif_dl_internal.h"

// Helper for RNN
// h_t = tanh(W*x + R*h_prev + b)
void eif_layer_rnn(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size, float32_t* state) {
    int units = layer->params.rnn.units;
    // Weights: [W (units x input) | R (units x units)]
    // Biases: [units]
    const float32_t* weights = (const float32_t*)layer->weights;
    const float32_t* biases = (const float32_t*)layer->biases;
    
    const float32_t* W = weights;
    const float32_t* R = weights + (units * input_size);
    
    // Calculate h_t
    for (int i = 0; i < units; i++) {
        float32_t sum = 0.0f;
        if (biases) sum = biases[i];
        
        // W * x
        for (int j = 0; j < input_size; j++) {
            sum += input[j] * W[i * input_size + j];
        }
        
        // R * h_prev
        for (int j = 0; j < units; j++) {
            sum += state[j] * R[i * units + j];
        }
        
        // Activation (tanh)
        output[i] = tanhf(sum);
    }
    
    // Update state
    memcpy(state, output, units * sizeof(float32_t));
}

// Helper for LSTM
void eif_layer_lstm(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size, float32_t* state) {
    int units = layer->params.lstm.units;
    // Weights: [W (4*units x input) | R (4*units x units)]
    // Biases: [4*units] (Input, Forget, Output, Cell)
    // State: [h (units) | c (units)]
    
    const float32_t* weights = (const float32_t*)layer->weights;
    const float32_t* biases = (const float32_t*)layer->biases;
    
    const float32_t* W = weights;
    const float32_t* R = weights + (4 * units * input_size);
    
    float32_t* h_prev = state;
    float32_t* c_prev = state + units;
    
    // Allocate temp buffers for gates (4 * units)
    // Since we are in a constrained environment, we should use the output buffer or scratch.
    // But output is only 'units' size.
    // We can compute the linear combinations first.
    
    // Let's use a small stack buffer if units is small, or assume we have scratch.
    // For this prototype, let's assume units <= 128.
    float32_t gate_activations[4 * 128]; 
    if (units > 128) {
        // Fallback or error
        return; 
    }
    
    // 1. Compute linear combinations
    for (int g = 0; g < 4; g++) {
        for (int i = 0; i < units; i++) {
            int gate_idx = g * units + i;
            float32_t sum = 0.0f;
            if (biases) sum = biases[gate_idx];
            
            for (int j = 0; j < input_size; j++) {
                sum += input[j] * W[gate_idx * input_size + j];
            }
            
            for (int j = 0; j < units; j++) {
                sum += h_prev[j] * R[gate_idx * units + j];
            }
            gate_activations[gate_idx] = sum;
        }
    }
    
    // 2. Apply activations and update state
    for (int i = 0; i < units; i++) {
        float32_t in_gate = 1.0f / (1.0f + expf(-gate_activations[0 * units + i]));
        float32_t out_gate = 1.0f / (1.0f + expf(-gate_activations[1 * units + i]));
        float32_t forget_gate = 1.0f / (1.0f + expf(-gate_activations[2 * units + i]));
        float32_t cell_gate = tanhf(gate_activations[3 * units + i]);
        
        float32_t c_t = forget_gate * c_prev[i] + in_gate * cell_gate;
        float32_t h_t = out_gate * tanhf(c_t);
        
        // Update state
        c_prev[i] = c_t;
        // We can't update h_prev yet if we were doing this in place, but we computed all gates already.
        // So it's safe to update h_prev now? Yes, because R*h_prev is done.
        h_prev[i] = h_t;
        output[i] = h_t;
    }
}

// Helper for GRU
void eif_layer_gru(const eif_layer_t* layer, const float32_t* input, float32_t* output, int input_size, float32_t* state) {
    int units = layer->params.gru.units;
    // Weights: [W (3*units x input) | R (3*units x units)]
    // Biases: [3*units] (Update, Reset, Hidden)
    
    const float32_t* weights = (const float32_t*)layer->weights;
    const float32_t* biases = (const float32_t*)layer->biases;
    
    const float32_t* W = weights;
    const float32_t* R = weights + (3 * units * input_size);
    
    float32_t* h_prev = state;
    
    float32_t gate_activations[3 * 128];
    if (units > 128) return;
    
    // 1. Compute linear combinations
    for (int g = 0; g < 3; g++) {
        for (int i = 0; i < units; i++) {
            int gate_idx = g * units + i;
            float32_t sum = 0.0f;
            if (biases) sum = biases[gate_idx];
            
            for (int j = 0; j < input_size; j++) {
                sum += input[j] * W[gate_idx * input_size + j];
            }
            
            for (int j = 0; j < units; j++) {
                sum += h_prev[j] * R[gate_idx * units + j];
            }
            gate_activations[gate_idx] = sum;
        }
    }
    
    // 2. Apply activations
    for (int i = 0; i < units; i++) {
        float32_t z_t = 1.0f / (1.0f + expf(-gate_activations[0 * units + i])); // Update gate
        float32_t r_t = 1.0f / (1.0f + expf(-gate_activations[1 * units + i])); // Reset gate
        
        // Re-compute h_hat recurrent part
        // We need W_h*x + b_h + R_h*(r_t*h_prev).
        
        // To do this accurately, we need the specific R_h row.
        float32_t r_h_dot_h = 0.0f;
        for(int j=0; j<units; j++) {
            r_h_dot_h += h_prev[j] * R[2*units*units + i*units + j];
        }
        
        float32_t w_x_b = 0.0f; // W*x + b
        if (biases) w_x_b = biases[2*units + i];
        for (int j = 0; j < input_size; j++) {
            w_x_b += input[j] * W[(2*units + i) * input_size + j];
        }
        
        float32_t r_h_rec = 0.0f;
        for (int j = 0; j < units; j++) {
            r_h_rec += (r_t * h_prev[j]) * R[(2*units + i) * units + j];
        }
        
        float32_t h_hat = tanhf(w_x_b + r_h_rec);
        
        // Final h_t
        float32_t h_t = (1.0f - z_t) * h_prev[i] + z_t * h_hat;
        
        output[i] = h_t;
    }
    
    // Update state
    memcpy(h_prev, output, units * sizeof(float32_t));
}
