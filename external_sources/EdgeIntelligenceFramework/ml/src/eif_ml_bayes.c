/**
 * @file eif_ml_bayes.c
 * @brief Naive Bayes Classifier Implementation
 * 
 * Implements:
 * - Gaussian Naive Bayes (classification)
 */

#include "eif_ml.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ============================================================================
// Gaussian Naive Bayes Implementation
// ============================================================================

eif_status_t eif_nb_init(eif_naive_bayes_t* model, int num_features, 
                          int num_classes, eif_memory_pool_t* pool) {
    if (!model || !pool || num_features <= 0 || num_classes <= 1) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    model->num_features = num_features;
    model->num_classes = num_classes;
    model->total_samples = 0;
    
    // Allocate arrays
    model->class_priors = eif_memory_alloc(pool, num_classes * sizeof(float32_t), 4);
    model->class_counts = eif_memory_alloc(pool, num_classes * sizeof(int), 4);
    model->means = eif_memory_alloc(pool, num_classes * num_features * sizeof(float32_t), 4);
    model->variances = eif_memory_alloc(pool, num_classes * num_features * sizeof(float32_t), 4);
    
    if (!model->class_priors || !model->class_counts || 
        !model->means || !model->variances) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Initialize to zero
    memset(model->class_priors, 0, num_classes * sizeof(float32_t));
    memset(model->class_counts, 0, num_classes * sizeof(int));
    memset(model->means, 0, num_classes * num_features * sizeof(float32_t));
    memset(model->variances, 0, num_classes * num_features * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_nb_fit(eif_naive_bayes_t* model, const float32_t* X, const int* y,
                         int num_samples) {
    if (!model || !X || !y || num_samples <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    model->total_samples = num_samples;
    int num_features = model->num_features;
    int num_classes = model->num_classes;
    
    // Reset counts and means
    memset(model->class_counts, 0, num_classes * sizeof(int));
    memset(model->means, 0, num_classes * num_features * sizeof(float32_t));
    
    // Pass 1: Count samples per class and compute sums (for means)
    for (int i = 0; i < num_samples; i++) {
        int c = y[i];
        if (c < 0 || c >= num_classes) continue;
        
        model->class_counts[c]++;
        
        for (int f = 0; f < num_features; f++) {
            model->means[c * num_features + f] += X[i * num_features + f];
        }
    }
    
    // Compute means and priors
    for (int c = 0; c < num_classes; c++) {
        if (model->class_counts[c] > 0) {
            model->class_priors[c] = (float32_t)model->class_counts[c] / (float32_t)num_samples;
            for (int f = 0; f < num_features; f++) {
                model->means[c * num_features + f] /= (float32_t)model->class_counts[c];
            }
        } else {
            model->class_priors[c] = 0.0f;
        }
    }
    
    // Pass 2: Compute variances
    memset(model->variances, 0, num_classes * num_features * sizeof(float32_t));
    
    for (int i = 0; i < num_samples; i++) {
        int c = y[i];
        if (c < 0 || c >= num_classes) continue;
        
        for (int f = 0; f < num_features; f++) {
            float32_t diff = X[i * num_features + f] - model->means[c * num_features + f];
            model->variances[c * num_features + f] += diff * diff;
        }
    }
    
    // Finalize variances (add small epsilon to avoid division by zero)
    for (int c = 0; c < num_classes; c++) {
        if (model->class_counts[c] > 1) {
            for (int f = 0; f < num_features; f++) {
                model->variances[c * num_features + f] /= (float32_t)(model->class_counts[c] - 1);
                // Add small epsilon for numerical stability
                model->variances[c * num_features + f] += 1e-9f;
            }
        } else {
            // If only one sample, use a default small variance
            for (int f = 0; f < num_features; f++) {
                model->variances[c * num_features + f] = 1e-6f;
            }
        }
    }
    
    return EIF_STATUS_OK;
}

// Compute log-probability for Gaussian distribution
static float32_t gaussian_log_prob(float32_t x, float32_t mean, float32_t var) {
    float32_t diff = x - mean;
    return -0.5f * (logf(2.0f * M_PI * var) + (diff * diff) / var);
}

int eif_nb_predict(const eif_naive_bayes_t* model, const float32_t* x) {
    if (!model || !x) return 0;
    
    int best_class = 0;
    float32_t best_log_prob = -1e30f;
    
    for (int c = 0; c < model->num_classes; c++) {
        if (model->class_priors[c] <= 0) continue;
        
        // Start with log prior
        float32_t log_prob = logf(model->class_priors[c]);
        
        // Add log likelihoods for each feature
        for (int f = 0; f < model->num_features; f++) {
            float32_t mean = model->means[c * model->num_features + f];
            float32_t var = model->variances[c * model->num_features + f];
            log_prob += gaussian_log_prob(x[f], mean, var);
        }
        
        if (log_prob > best_log_prob) {
            best_log_prob = log_prob;
            best_class = c;
        }
    }
    
    return best_class;
}

eif_status_t eif_nb_predict_proba(const eif_naive_bayes_t* model, const float32_t* x,
                                   float32_t* probabilities) {
    if (!model || !x || !probabilities) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Compute log probabilities for each class
    float32_t log_probs[32];
    float32_t max_log_prob = -1e30f;
    
    for (int c = 0; c < model->num_classes && c < 32; c++) {
        if (model->class_priors[c] <= 0) {
            log_probs[c] = -1e30f;
            continue;
        }
        
        log_probs[c] = logf(model->class_priors[c]);
        
        for (int f = 0; f < model->num_features; f++) {
            float32_t mean = model->means[c * model->num_features + f];
            float32_t var = model->variances[c * model->num_features + f];
            log_probs[c] += gaussian_log_prob(x[f], mean, var);
        }
        
        if (log_probs[c] > max_log_prob) {
            max_log_prob = log_probs[c];
        }
    }
    
    // Convert to probabilities using log-sum-exp trick for numerical stability
    float32_t sum_exp = 0.0f;
    for (int c = 0; c < model->num_classes && c < 32; c++) {
        probabilities[c] = expf(log_probs[c] - max_log_prob);
        sum_exp += probabilities[c];
    }
    
    // Normalize
    if (sum_exp > 0) {
        for (int c = 0; c < model->num_classes && c < 32; c++) {
            probabilities[c] /= sum_exp;
        }
    }
    
    return EIF_STATUS_OK;
}
