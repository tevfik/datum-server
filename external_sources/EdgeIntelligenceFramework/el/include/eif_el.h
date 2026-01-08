/**
 * @file eif_el.h
 * @brief Edge Learning Module - Main Header
 * 
 * Unified header for all edge learning algorithms:
 * - Federated Learning (FedAvg)
 * - Continual Learning (EWC)
 * - Online Learning
 * - Few-Shot Learning
 * - Reinforcement Learning (Q-Learning, DQN)
 */

#ifndef EIF_EL_H
#define EIF_EL_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Common Types
// =============================================================================

typedef struct {
    float32_t* data;
    int size;
    int count;
} eif_gradient_t;

typedef struct {
    float32_t* weights;
    int num_weights;
} eif_weights_t;

// =============================================================================
// 1. FEDERATED LEARNING
// =============================================================================

typedef struct {
    float32_t* weights;
    int num_weights;
    float32_t* gradients;
    int num_samples;
    float32_t learning_rate;
    int local_epochs;
    int batch_size;
    uint32_t round_id;
    bool has_update;
    eif_memory_pool_t* pool;
} eif_federated_client_t;

eif_status_t eif_federated_init(eif_federated_client_t* client,
                                 int num_weights,
                                 float32_t learning_rate,
                                 eif_memory_pool_t* pool);

eif_status_t eif_federated_set_weights(eif_federated_client_t* client,
                                        const float32_t* global_weights);

eif_status_t eif_federated_train_batch(eif_federated_client_t* client,
                                        const float32_t* inputs,
                                        const float32_t* targets,
                                        int batch_size,
                                        int input_dim,
                                        int output_dim,
                                        void (*gradient_fn)(const float32_t* w, 
                                                           const float32_t* x,
                                                           const float32_t* y,
                                                           float32_t* grad,
                                                           int n, int in_d, int out_d));

eif_status_t eif_federated_get_update(eif_federated_client_t* client,
                                       float32_t* delta);

eif_status_t eif_federated_apply_update(eif_federated_client_t* client,
                                         const float32_t* aggregated_delta);

eif_status_t eif_federated_aggregate(float32_t* global_weights,
                                      const float32_t** client_deltas,
                                      const int* num_samples,
                                      int num_clients,
                                      int num_weights);

// =============================================================================
// 2. CONTINUAL LEARNING (EWC)
// =============================================================================

typedef struct {
    float32_t* weights;
    int num_weights;
    float32_t* star_weights;
    float32_t* fisher;
    float32_t lambda;
    int num_tasks;
    bool has_prior_task;
    eif_memory_pool_t* pool;
} eif_ewc_t;

eif_status_t eif_ewc_init(eif_ewc_t* ewc,
                           int num_weights,
                           float32_t lambda,
                           eif_memory_pool_t* pool);

eif_status_t eif_ewc_set_weights(eif_ewc_t* ewc, const float32_t* weights);

eif_status_t eif_ewc_compute_fisher(eif_ewc_t* ewc,
                                     const float32_t* data,
                                     int num_samples,
                                     int sample_size,
                                     void (*compute_grad)(const float32_t* w,
                                                         const float32_t* x,
                                                         float32_t* grad,
                                                         int n));

eif_status_t eif_ewc_consolidate(eif_ewc_t* ewc);
float32_t eif_ewc_penalty(const eif_ewc_t* ewc);
eif_status_t eif_ewc_gradient(const eif_ewc_t* ewc, float32_t* grad);
eif_status_t eif_ewc_update(eif_ewc_t* ewc,
                             const float32_t* task_gradient,
                             float32_t learning_rate);

// =============================================================================
// 3. ONLINE LEARNING
// =============================================================================

typedef struct {
    float32_t* weights;
    int num_weights;
    float32_t* grad_squared_sum;
    float32_t learning_rate;
    float32_t epsilon;
    float32_t error_rate;
    float32_t error_threshold;
    int window_size;
    int samples_seen;
    float32_t* error_buffer;
    int error_idx;
    eif_memory_pool_t* pool;
} eif_online_learner_t;

eif_status_t eif_online_init(eif_online_learner_t* learner,
                              int num_weights,
                              float32_t learning_rate,
                              int drift_window,
                              eif_memory_pool_t* pool);

eif_status_t eif_online_set_weights(eif_online_learner_t* learner,
                                     const float32_t* weights);

eif_status_t eif_online_update(eif_online_learner_t* learner,
                                const float32_t* gradient,
                                float32_t loss);

float32_t eif_online_predict(const eif_online_learner_t* learner,
                              const float32_t* input,
                              int input_dim);

bool eif_online_drift_detected(const eif_online_learner_t* learner);
void eif_online_reset_drift(eif_online_learner_t* learner);
float32_t eif_online_error_rate(const eif_online_learner_t* learner);

// =============================================================================
// 4. FEW-SHOT LEARNING
// =============================================================================

typedef struct {
    float32_t* embedding;
    int embed_dim;
    int num_samples;
    int class_id;
} eif_prototype_t;

typedef struct {
    eif_prototype_t* prototypes;
    int num_classes;
    int max_classes;
    int embed_dim;
    enum {
        EIF_DISTANCE_EUCLIDEAN,
        EIF_DISTANCE_COSINE
    } distance_type;
    float32_t* embed_buffer;
    eif_memory_pool_t* pool;
} eif_fewshot_t;

eif_status_t eif_fewshot_init(eif_fewshot_t* fs,
                               int max_classes,
                               int embed_dim,
                               eif_memory_pool_t* pool);

eif_status_t eif_fewshot_add_example(eif_fewshot_t* fs,
                                      const float32_t* embedding,
                                      int class_id);

eif_status_t eif_fewshot_update_prototype(eif_fewshot_t* fs,
                                           const float32_t* embedding,
                                           int class_id);

int eif_fewshot_classify(const eif_fewshot_t* fs,
                          const float32_t* embedding,
                          float32_t* distance);

eif_status_t eif_fewshot_predict_proba(const eif_fewshot_t* fs,
                                        const float32_t* embedding,
                                        float32_t* probabilities);

void eif_fewshot_reset(eif_fewshot_t* fs);
int eif_fewshot_num_classes(const eif_fewshot_t* fs);

// =============================================================================
// Utility Functions
// =============================================================================

void eif_linear_gradient(const float32_t* weights,
                          const float32_t* inputs,
                          const float32_t* targets,
                          float32_t* gradient,
                          int num_samples,
                          int input_dim,
                          int output_dim);

float32_t eif_euclidean_distance(const float32_t* a, const float32_t* b, int dim);
float32_t eif_cosine_similarity(const float32_t* a, const float32_t* b, int dim);

#ifdef __cplusplus
}
#endif

#endif // EIF_EL_H
