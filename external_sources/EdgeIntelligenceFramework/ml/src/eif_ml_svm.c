/**
 * @file eif_ml_svm.c
 * @brief Support Vector Machine Implementation
 * 
 * Lightweight Linear SVM for embedded systems using:
 * - Pegasos (Primal Estimated sub-GrAdient SOlver for SVM)
 * - Online/incremental learning support
 * - Binary and One-vs-Rest multiclass
 */

#include "eif_ml.h"
#include <string.h>
#include <math.h>

// =============================================================================
// Linear SVM Implementation (Pegasos Algorithm)
// =============================================================================

eif_status_t eif_svm_init(eif_svm_t* svm, int num_features,
                           float32_t lambda, eif_memory_pool_t* pool) {
    if (!svm || !pool || num_features <= 0 || lambda <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    svm->num_features = num_features;
    svm->lambda = lambda;
    svm->bias = 0.0f;
    svm->t = 1;
    
    svm->weights = (float32_t*)eif_memory_alloc(pool, 
        num_features * sizeof(float32_t), sizeof(float32_t));
    if (!svm->weights) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Initialize weights to zero
    memset(svm->weights, 0, num_features * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_svm_fit(eif_svm_t* svm, const float32_t* X, const int* y,
                          int num_samples, int max_epochs) {
    if (!svm || !X || !y || num_samples <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int n = svm->num_features;
    
    for (int epoch = 0; epoch < max_epochs; epoch++) {
        for (int i = 0; i < num_samples; i++) {
            const float32_t* xi = &X[i * n];
            int yi = (y[i] > 0) ? 1 : -1;  // Ensure ±1 labels
            
            // Pegasos update
            eif_svm_update(svm, xi, yi);
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_svm_update(eif_svm_t* svm, const float32_t* x, int y) {
    if (!svm || !x) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int yi = (y > 0) ? 1 : -1;
    float32_t eta = 1.0f / (svm->lambda * svm->t);  // Learning rate
    
    // Compute decision value: w·x + b
    float32_t decision = svm->bias;
    for (int j = 0; j < svm->num_features; j++) {
        decision += svm->weights[j] * x[j];
    }
    
    // Check margin condition: y * (w·x + b) < 1
    if (yi * decision < 1.0f) {
        // Misclassified or in margin - update with gradient
        for (int j = 0; j < svm->num_features; j++) {
            svm->weights[j] = (1.0f - eta * svm->lambda) * svm->weights[j] 
                             + eta * yi * x[j];
        }
        svm->bias += eta * yi;
    } else {
        // Correctly classified - just regularize
        for (int j = 0; j < svm->num_features; j++) {
            svm->weights[j] = (1.0f - eta * svm->lambda) * svm->weights[j];
        }
    }
    
    svm->t++;
    return EIF_STATUS_OK;
}

float32_t eif_svm_decision(const eif_svm_t* svm, const float32_t* x) {
    if (!svm || !x) return 0.0f;
    
    float32_t decision = svm->bias;
    for (int j = 0; j < svm->num_features; j++) {
        decision += svm->weights[j] * x[j];
    }
    return decision;
}

int eif_svm_predict(const eif_svm_t* svm, const float32_t* x) {
    return (eif_svm_decision(svm, x) >= 0.0f) ? 1 : 0;
}

// =============================================================================
// AdaBoost (Adaptive Boosting) with Decision Stumps
// =============================================================================

/**
 * @brief Find best split for a decision stump
 */
static void find_best_stump(const float32_t* X, const int* y, 
                            const float32_t* weights, int num_samples, 
                            int num_features, eif_stump_t* stump) {
    float32_t best_error = 1.0f;
    
    for (int f = 0; f < num_features; f++) {
        // Try different thresholds for this feature
        for (int i = 0; i < num_samples; i++) {
            float32_t threshold = X[i * num_features + f];
            
            // Try both polarities
            for (int polarity = -1; polarity <= 1; polarity += 2) {
                float32_t error = 0.0f;
                
                for (int j = 0; j < num_samples; j++) {
                    float32_t val = X[j * num_features + f];
                    int pred = (polarity * val < polarity * threshold) ? 0 : 1;
                    if (pred != y[j]) {
                        error += weights[j];
                    }
                }
                
                if (error < best_error) {
                    best_error = error;
                    stump->feature_idx = f;
                    stump->threshold = threshold;
                    stump->polarity = polarity;
                }
            }
        }
    }
}

eif_status_t eif_adaboost_init(eif_adaboost_t* model, int num_estimators,
                                int num_features, eif_memory_pool_t* pool) {
    if (!model || !pool || num_estimators <= 0 || num_features <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    model->num_estimators = num_estimators;
    model->num_features = num_features;
    model->num_fitted = 0;
    
    model->stumps = (eif_stump_t*)eif_memory_alloc(pool,
        num_estimators * sizeof(eif_stump_t), sizeof(void*));
    model->alphas = (float32_t*)eif_memory_alloc(pool,
        num_estimators * sizeof(float32_t), sizeof(float32_t));
    
    if (!model->stumps || !model->alphas) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_adaboost_fit(eif_adaboost_t* model, const float32_t* X,
                               const int* y, int num_samples,
                               eif_memory_pool_t* pool) {
    if (!model || !X || !y || num_samples <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Initialize sample weights
    float32_t* weights = (float32_t*)eif_memory_alloc(pool,
        num_samples * sizeof(float32_t), sizeof(float32_t));
    if (!weights) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    float32_t init_weight = 1.0f / num_samples;
    for (int i = 0; i < num_samples; i++) {
        weights[i] = init_weight;
    }
    
    for (int t = 0; t < model->num_estimators; t++) {
        eif_stump_t* stump = &model->stumps[t];
        
        // Find best weak learner
        find_best_stump(X, y, weights, num_samples, model->num_features, stump);
        
        // Calculate weighted error
        float32_t error = 0.0f;
        for (int i = 0; i < num_samples; i++) {
            float32_t val = X[i * model->num_features + stump->feature_idx];
            int pred = (stump->polarity * val < stump->polarity * stump->threshold) ? 0 : 1;
            if (pred != y[i]) {
                error += weights[i];
            }
        }
        
        // Avoid division by zero
        if (error < 1e-10f) error = 1e-10f;
        if (error > 1.0f - 1e-10f) error = 1.0f - 1e-10f;
        
        // Calculate alpha (classifier weight)
        model->alphas[t] = 0.5f * logf((1.0f - error) / error);
        
        // Update sample weights
        float32_t sum = 0.0f;
        for (int i = 0; i < num_samples; i++) {
            float32_t val = X[i * model->num_features + stump->feature_idx];
            int pred = (stump->polarity * val < stump->polarity * stump->threshold) ? 0 : 1;
            int correct = (pred == y[i]) ? 1 : -1;
            weights[i] *= expf(-model->alphas[t] * correct);
            sum += weights[i];
        }
        
        // Normalize weights
        for (int i = 0; i < num_samples; i++) {
            weights[i] /= sum;
        }
        
        model->num_fitted++;
    }
    
    return EIF_STATUS_OK;
}

int eif_adaboost_predict(const eif_adaboost_t* model, const float32_t* x) {
    if (!model || !x) return 0;
    
    float32_t sum = 0.0f;
    
    for (int t = 0; t < model->num_fitted; t++) {
        const eif_stump_t* stump = &model->stumps[t];
        float32_t val = x[stump->feature_idx];
        int pred = (stump->polarity * val < stump->polarity * stump->threshold) ? 0 : 1;
        sum += model->alphas[t] * (pred * 2 - 1);
    }
    
    return (sum >= 0.0f) ? 1 : 0;
}

// =============================================================================
// Mini-Batch K-Means (for streaming/large datasets)
// =============================================================================

eif_status_t eif_minibatch_kmeans_init(eif_minibatch_kmeans_t* model, int k,
                                        int num_features, int batch_size,
                                        eif_memory_pool_t* pool) {
    if (!model || !pool || k <= 0 || num_features <= 0 || batch_size <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    model->k = k;
    model->num_features = num_features;
    model->batch_size = batch_size;
    model->initialized = false;
    
    model->centroids = (float32_t*)eif_memory_alloc(pool,
        k * num_features * sizeof(float32_t), sizeof(float32_t));
    model->counts = (int*)eif_memory_alloc(pool, k * sizeof(int), sizeof(int));
    
    if (!model->centroids || !model->counts) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    memset(model->counts, 0, k * sizeof(int));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_minibatch_kmeans_partial_fit(eif_minibatch_kmeans_t* model,
                                               const float32_t* X, int num_samples) {
    if (!model || !X || num_samples <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int n = model->num_features;
    int k = model->k;
    
    // Initialize centroids from first batch if not done
    if (!model->initialized) {
        int init_count = (num_samples < k) ? num_samples : k;
        for (int i = 0; i < init_count; i++) {
            memcpy(&model->centroids[i * n], &X[i * n], n * sizeof(float32_t));
            model->counts[i] = 1;
        }
        model->initialized = true;
        return EIF_STATUS_OK;
    }
    
    // Process each sample in the mini-batch
    for (int i = 0; i < num_samples; i++) {
        const float32_t* x = &X[i * n];
        
        // Find nearest centroid
        int best_c = 0;
        float32_t best_dist = 1e30f;
        
        for (int c = 0; c < k; c++) {
            float32_t dist = 0.0f;
            for (int j = 0; j < n; j++) {
                float32_t diff = x[j] - model->centroids[c * n + j];
                dist += diff * diff;
            }
            if (dist < best_dist) {
                best_dist = dist;
                best_c = c;
            }
        }
        
        // Update centroid with running mean
        model->counts[best_c]++;
        float32_t eta = 1.0f / model->counts[best_c];
        
        for (int j = 0; j < n; j++) {
            model->centroids[best_c * n + j] += 
                eta * (x[j] - model->centroids[best_c * n + j]);
        }
    }
    
    return EIF_STATUS_OK;
}

int eif_minibatch_kmeans_predict(const eif_minibatch_kmeans_t* model,
                                  const float32_t* x) {
    if (!model || !x) return -1;
    
    int best_c = 0;
    float32_t best_dist = 1e30f;
    
    for (int c = 0; c < model->k; c++) {
        float32_t dist = 0.0f;
        for (int j = 0; j < model->num_features; j++) {
            float32_t diff = x[j] - model->centroids[c * model->num_features + j];
            dist += diff * diff;
        }
        if (dist < best_dist) {
            best_dist = dist;
            best_c = c;
        }
    }
    
    return best_c;
}
