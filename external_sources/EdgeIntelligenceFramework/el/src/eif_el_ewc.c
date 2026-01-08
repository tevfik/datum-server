/**
 * @file eif_el_ewc.c
 * @brief Elastic Weight Consolidation (EWC) Implementation
 * 
 * Prevents catastrophic forgetting by adding a penalty for changing
 * weights that are important for previously learned tasks.
 */

#include "eif_el.h"
#include <string.h>
#include <math.h>

eif_status_t eif_ewc_init(eif_ewc_t* ewc,
                           int num_weights,
                           float32_t lambda,
                           eif_memory_pool_t* pool) {
    if (!ewc || !pool || num_weights <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    ewc->num_weights = num_weights;
    ewc->lambda = lambda;
    ewc->num_tasks = 0;
    ewc->has_prior_task = false;
    ewc->pool = pool;
    
    ewc->weights = (float32_t*)eif_memory_alloc(pool,
        num_weights * sizeof(float32_t), sizeof(float32_t));
    ewc->star_weights = (float32_t*)eif_memory_alloc(pool,
        num_weights * sizeof(float32_t), sizeof(float32_t));
    ewc->fisher = (float32_t*)eif_memory_alloc(pool,
        num_weights * sizeof(float32_t), sizeof(float32_t));
    
    if (!ewc->weights || !ewc->star_weights || !ewc->fisher) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    memset(ewc->weights, 0, num_weights * sizeof(float32_t));
    memset(ewc->star_weights, 0, num_weights * sizeof(float32_t));
    memset(ewc->fisher, 0, num_weights * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ewc_set_weights(eif_ewc_t* ewc, const float32_t* weights) {
    if (!ewc || !weights) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    memcpy(ewc->weights, weights, ewc->num_weights * sizeof(float32_t));
    return EIF_STATUS_OK;
}

eif_status_t eif_ewc_compute_fisher(eif_ewc_t* ewc,
                                     const float32_t* data,
                                     int num_samples,
                                     int sample_size,
                                     void (*compute_grad)(const float32_t*,
                                                         const float32_t*,
                                                         float32_t*,
                                                         int)) {
    if (!ewc || !data || !compute_grad) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    memset(ewc->fisher, 0, ewc->num_weights * sizeof(float32_t));
    
    float32_t grad[1024];
    
    for (int n = 0; n < num_samples; n++) {
        const float32_t* sample = &data[n * sample_size];
        
        compute_grad(ewc->weights, sample, grad, ewc->num_weights);
        
        for (int i = 0; i < ewc->num_weights; i++) {
            ewc->fisher[i] += grad[i] * grad[i];
        }
    }
    
    float32_t inv_n = 1.0f / num_samples;
    for (int i = 0; i < ewc->num_weights; i++) {
        ewc->fisher[i] *= inv_n;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ewc_consolidate(eif_ewc_t* ewc) {
    if (!ewc) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    memcpy(ewc->star_weights, ewc->weights, 
           ewc->num_weights * sizeof(float32_t));
    
    ewc->num_tasks++;
    ewc->has_prior_task = true;
    
    return EIF_STATUS_OK;
}

float32_t eif_ewc_penalty(const eif_ewc_t* ewc) {
    if (!ewc || !ewc->has_prior_task) {
        return 0.0f;
    }
    
    float32_t penalty = 0.0f;
    for (int i = 0; i < ewc->num_weights; i++) {
        float32_t diff = ewc->weights[i] - ewc->star_weights[i];
        penalty += ewc->fisher[i] * diff * diff;
    }
    
    return 0.5f * ewc->lambda * penalty;
}

eif_status_t eif_ewc_gradient(const eif_ewc_t* ewc, float32_t* grad) {
    if (!ewc || !grad) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    if (!ewc->has_prior_task) {
        memset(grad, 0, ewc->num_weights * sizeof(float32_t));
        return EIF_STATUS_OK;
    }
    
    for (int i = 0; i < ewc->num_weights; i++) {
        float32_t diff = ewc->weights[i] - ewc->star_weights[i];
        grad[i] = ewc->lambda * ewc->fisher[i] * diff;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ewc_update(eif_ewc_t* ewc,
                             const float32_t* task_gradient,
                             float32_t learning_rate) {
    if (!ewc || !task_gradient) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    float32_t ewc_grad[1024];
    eif_ewc_gradient(ewc, ewc_grad);
    
    for (int i = 0; i < ewc->num_weights; i++) {
        ewc->weights[i] -= learning_rate * (task_gradient[i] + ewc_grad[i]);
    }
    
    return EIF_STATUS_OK;
}
