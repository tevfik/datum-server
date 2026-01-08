/**
 * @file eif_el_utils.c
 * @brief Common utility functions for Edge Learning
 */

#include "eif_el.h"
#include <string.h>
#include <math.h>

float32_t eif_euclidean_distance(const float32_t* a, const float32_t* b, int dim) {
    float32_t sum = 0.0f;
    for (int i = 0; i < dim; i++) {
        float32_t diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

float32_t eif_cosine_similarity(const float32_t* a, const float32_t* b, int dim) {
    float32_t dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot / (sqrtf(norm_a) * sqrtf(norm_b) + 1e-10f);
}

void eif_linear_gradient(const float32_t* weights,
                          const float32_t* inputs,
                          const float32_t* targets,
                          float32_t* gradient,
                          int num_samples,
                          int input_dim,
                          int output_dim) {
    memset(gradient, 0, input_dim * output_dim * sizeof(float32_t));
    
    for (int n = 0; n < num_samples; n++) {
        const float32_t* x = &inputs[n * input_dim];
        const float32_t* y = &targets[n * output_dim];
        
        float32_t y_pred[16];
        for (int o = 0; o < output_dim; o++) {
            y_pred[o] = 0.0f;
            for (int i = 0; i < input_dim; i++) {
                y_pred[o] += x[i] * weights[i * output_dim + o];
            }
        }
        
        float32_t error[16];
        for (int o = 0; o < output_dim; o++) {
            error[o] = y_pred[o] - y[o];
        }
        
        for (int i = 0; i < input_dim; i++) {
            for (int o = 0; o < output_dim; o++) {
                gradient[i * output_dim + o] += x[i] * error[o];
            }
        }
    }
    
    float32_t inv_n = 1.0f / num_samples;
    for (int i = 0; i < input_dim * output_dim; i++) {
        gradient[i] *= inv_n;
    }
}
