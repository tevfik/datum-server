/**
 * @file eif_el_federated.c
 * @brief Federated Learning (FedAvg) Implementation
 */

#include "eif_el.h"
#include <string.h>
#include <math.h>

eif_status_t eif_federated_init(eif_federated_client_t* client,
                                 int num_weights,
                                 float32_t learning_rate,
                                 eif_memory_pool_t* pool) {
    if (!client || !pool || num_weights <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    client->num_weights = num_weights;
    client->learning_rate = learning_rate;
    client->local_epochs = 1;
    client->batch_size = 32;
    client->round_id = 0;
    client->has_update = false;
    client->num_samples = 0;
    client->pool = pool;
    
    client->weights = (float32_t*)eif_memory_alloc(pool,
        num_weights * sizeof(float32_t), sizeof(float32_t));
    client->gradients = (float32_t*)eif_memory_alloc(pool,
        num_weights * sizeof(float32_t), sizeof(float32_t));
    
    if (!client->weights || !client->gradients) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    memset(client->weights, 0, num_weights * sizeof(float32_t));
    memset(client->gradients, 0, num_weights * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_federated_set_weights(eif_federated_client_t* client,
                                        const float32_t* global_weights) {
    if (!client || !global_weights) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    memcpy(client->weights, global_weights, 
           client->num_weights * sizeof(float32_t));
    
    memset(client->gradients, 0, client->num_weights * sizeof(float32_t));
    client->num_samples = 0;
    client->has_update = false;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_federated_train_batch(eif_federated_client_t* client,
                                        const float32_t* inputs,
                                        const float32_t* targets,
                                        int batch_size,
                                        int input_dim,
                                        int output_dim,
                                        void (*gradient_fn)(const float32_t*,
                                                           const float32_t*,
                                                           const float32_t*,
                                                           float32_t*,
                                                           int, int, int)) {
    if (!client || !inputs || !targets || !gradient_fn) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    float32_t batch_grad[1024];
    gradient_fn(client->weights, inputs, targets, batch_grad,
                batch_size, input_dim, output_dim);
    
    for (int i = 0; i < client->num_weights; i++) {
        client->weights[i] -= client->learning_rate * batch_grad[i];
        client->gradients[i] += batch_grad[i];
    }
    
    client->num_samples += batch_size;
    client->has_update = true;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_federated_get_update(eif_federated_client_t* client,
                                       float32_t* delta) {
    if (!client || !delta) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    memcpy(delta, client->gradients, client->num_weights * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_federated_apply_update(eif_federated_client_t* client,
                                         const float32_t* aggregated_delta) {
    if (!client || !aggregated_delta) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    for (int i = 0; i < client->num_weights; i++) {
        client->weights[i] -= client->learning_rate * aggregated_delta[i];
    }
    
    client->round_id++;
    client->has_update = false;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_federated_aggregate(float32_t* global_weights,
                                      const float32_t** client_deltas,
                                      const int* num_samples,
                                      int num_clients,
                                      int num_weights) {
    if (!global_weights || !client_deltas || !num_samples) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int total_samples = 0;
    for (int c = 0; c < num_clients; c++) {
        total_samples += num_samples[c];
    }
    
    if (total_samples == 0) {
        return EIF_STATUS_OK;
    }
    
    float32_t aggregated[1024];
    memset(aggregated, 0, num_weights * sizeof(float32_t));
    
    for (int c = 0; c < num_clients; c++) {
        float32_t weight = (float32_t)num_samples[c] / total_samples;
        for (int i = 0; i < num_weights; i++) {
            aggregated[i] += weight * client_deltas[c][i];
        }
    }
    
    for (int i = 0; i < num_weights; i++) {
        global_weights[i] -= aggregated[i];
    }
    
    return EIF_STATUS_OK;
}
