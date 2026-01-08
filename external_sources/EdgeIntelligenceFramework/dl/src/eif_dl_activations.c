#include "eif_dl_internal.h"

void eif_layer_softmax(const float32_t* input, float32_t* output, int size) {
    float32_t max_val = -INFINITY;
    for (int i = 0; i < size; i++) if (input[i] > max_val) max_val = input[i];
    float32_t sum = 0.0f;
    for (int i = 0; i < size; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }
    for (int i = 0; i < size; i++) output[i] /= sum;
}

void eif_layer_sigmoid(const float32_t* input, float32_t* output, int size) {
    for (int i = 0; i < size; i++) output[i] = 1.0f / (1.0f + expf(-input[i]));
}

void eif_layer_tanh(const float32_t* input, float32_t* output, int size) {
    for (int i = 0; i < size; i++) output[i] = tanhf(input[i]);
}

void eif_layer_relu(const float32_t* input, float32_t* output, int size) {
    for (int i = 0; i < size; i++) output[i] = input[i] > 0 ? input[i] : 0;
}

void eif_layer_relu6(const float32_t* input, float32_t* output, int size) {
    for (int i = 0; i < size; i++) {
        float32_t val = input[i] > 0 ? input[i] : 0;
        output[i] = val > 6.0f ? 6.0f : val;
    }
}


