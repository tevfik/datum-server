/**
 * @file eif_el_online.c
 * @brief Online Learning Implementation
 * 
 * Adaptive SGD with:
 * - AdaGrad-style learning rate adaptation
 * - Concept drift detection
 * - Streaming data support
 */

#include "eif_el.h"
#include <string.h>
#include <math.h>

eif_status_t eif_online_init(eif_online_learner_t* learner,
                              int num_weights,
                              float32_t learning_rate,
                              int drift_window,
                              eif_memory_pool_t* pool) {
    if (!learner || !pool || num_weights <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    learner->num_weights = num_weights;
    learner->learning_rate = learning_rate;
    learner->epsilon = 1e-8f;
    learner->error_rate = 0.0f;
    learner->error_threshold = 0.3f;
    learner->window_size = drift_window;
    learner->samples_seen = 0;
    learner->error_idx = 0;
    learner->pool = pool;
    
    learner->weights = (float32_t*)eif_memory_alloc(pool,
        num_weights * sizeof(float32_t), sizeof(float32_t));
    learner->grad_squared_sum = (float32_t*)eif_memory_alloc(pool,
        num_weights * sizeof(float32_t), sizeof(float32_t));
    learner->error_buffer = (float32_t*)eif_memory_alloc(pool,
        drift_window * sizeof(float32_t), sizeof(float32_t));
    
    if (!learner->weights || !learner->grad_squared_sum || !learner->error_buffer) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    memset(learner->weights, 0, num_weights * sizeof(float32_t));
    memset(learner->grad_squared_sum, 0, num_weights * sizeof(float32_t));
    memset(learner->error_buffer, 0, drift_window * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_online_set_weights(eif_online_learner_t* learner,
                                     const float32_t* weights) {
    if (!learner || !weights) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    memcpy(learner->weights, weights, learner->num_weights * sizeof(float32_t));
    return EIF_STATUS_OK;
}

eif_status_t eif_online_update(eif_online_learner_t* learner,
                                const float32_t* gradient,
                                float32_t loss) {
    if (!learner || !gradient) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // AdaGrad update
    for (int i = 0; i < learner->num_weights; i++) {
        learner->grad_squared_sum[i] += gradient[i] * gradient[i];
        float32_t adaptive_lr = learner->learning_rate / 
            (sqrtf(learner->grad_squared_sum[i]) + learner->epsilon);
        learner->weights[i] -= adaptive_lr * gradient[i];
    }
    
    // Drift detection
    learner->error_buffer[learner->error_idx] = loss;
    learner->error_idx = (learner->error_idx + 1) % learner->window_size;
    learner->samples_seen++;
    
    int n = learner->samples_seen < learner->window_size ? 
            learner->samples_seen : learner->window_size;
    float32_t sum = 0.0f;
    for (int i = 0; i < n; i++) {
        sum += learner->error_buffer[i];
    }
    learner->error_rate = sum / n;
    
    return EIF_STATUS_OK;
}

float32_t eif_online_predict(const eif_online_learner_t* learner,
                              const float32_t* input,
                              int input_dim) {
    if (!learner || !input) return 0.0f;
    
    float32_t result = 0.0f;
    for (int i = 0; i < input_dim && i < learner->num_weights; i++) {
        result += learner->weights[i] * input[i];
    }
    return result;
}

bool eif_online_drift_detected(const eif_online_learner_t* learner) {
    if (!learner) return false;
    return learner->error_rate > learner->error_threshold;
}

void eif_online_reset_drift(eif_online_learner_t* learner) {
    if (!learner) return;
    memset(learner->error_buffer, 0, learner->window_size * sizeof(float32_t));
    learner->error_idx = 0;
    learner->error_rate = 0.0f;
    memset(learner->grad_squared_sum, 0, learner->num_weights * sizeof(float32_t));
}

float32_t eif_online_error_rate(const eif_online_learner_t* learner) {
    return learner ? learner->error_rate : 0.0f;
}
