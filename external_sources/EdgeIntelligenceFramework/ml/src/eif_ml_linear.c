/**
 * @file eif_ml_linear.c
 * @brief Linear ML Models Implementation
 * 
 * Implements:
 * - Logistic Regression (Binary & Multiclass)
 */

#include "eif_ml.h"
#include <math.h>
#include <stdlib.h>

// ============================================================================
// Utility Functions
// ============================================================================

static float32_t sigmoid(float32_t x) {
    if (x > 20.0f) return 1.0f;
    if (x < -20.0f) return 0.0f;
    return 1.0f / (1.0f + expf(-x));
}

static float32_t randf(void) {
    return (float32_t)rand() / (float32_t)RAND_MAX;
}

// ============================================================================
// Logistic Regression Implementation
// ============================================================================

eif_status_t eif_logreg_init(eif_logreg_t* model, int num_features,
                              float32_t learning_rate, float32_t regularization,
                              eif_memory_pool_t* pool) {
    if (!model || !pool || num_features <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    model->num_features = num_features;
    model->learning_rate = learning_rate > 0 ? learning_rate : 0.01f;
    model->regularization = regularization >= 0 ? regularization : 0.0f;
    
    model->weights = eif_memory_alloc(pool, (num_features + 1) * sizeof(float32_t), 4);
    if (!model->weights) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    for (int i = 0; i <= num_features; i++) {
        model->weights[i] = (randf() - 0.5f) * 0.1f;
    }
    
    return EIF_STATUS_OK;
}

float32_t eif_logreg_predict_proba(const eif_logreg_t* model, const float32_t* x) {
    if (!model || !x) return 0.5f;
    
    float32_t z = model->weights[0];
    for (int i = 0; i < model->num_features; i++) {
        z += model->weights[i + 1] * x[i];
    }
    
    return sigmoid(z);
}

int eif_logreg_predict(const eif_logreg_t* model, const float32_t* x) {
    return eif_logreg_predict_proba(model, x) >= 0.5f ? 1 : 0;
}

eif_status_t eif_logreg_update(eif_logreg_t* model, const float32_t* x, int y) {
    if (!model || !x) return EIF_STATUS_INVALID_ARGUMENT;
    
    float32_t pred = eif_logreg_predict_proba(model, x);
    float32_t error = pred - (float32_t)y;
    
    model->weights[0] -= model->learning_rate * error;
    
    for (int i = 0; i < model->num_features; i++) {
        float32_t grad = error * x[i] + model->regularization * model->weights[i + 1];
        model->weights[i + 1] -= model->learning_rate * grad;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_logreg_fit(eif_logreg_t* model, const float32_t* X, const int* y,
                             int num_samples, int max_epochs) {
    if (!model || !X || !y || num_samples <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    for (int epoch = 0; epoch < max_epochs; epoch++) {
        for (int i = 0; i < num_samples; i++) {
            const float32_t* x = &X[i * model->num_features];
            eif_logreg_update(model, x, y[i]);
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Multi-class Logistic Regression (One-vs-Rest)
// ============================================================================

eif_status_t eif_logreg_multiclass_init(eif_logreg_multiclass_t* model, int num_features,
                                         int num_classes, float32_t learning_rate,
                                         float32_t regularization, eif_memory_pool_t* pool) {
    if (!model || !pool || num_features <= 0 || num_classes <= 1) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    model->num_features = num_features;
    model->num_classes = num_classes;
    
    model->classifiers = eif_memory_alloc(pool, num_classes * sizeof(eif_logreg_t), 4);
    if (!model->classifiers) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    for (int c = 0; c < num_classes; c++) {
        eif_status_t status = eif_logreg_init(&model->classifiers[c], num_features,
                                               learning_rate, regularization, pool);
        if (status != EIF_STATUS_OK) return status;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_logreg_multiclass_fit(eif_logreg_multiclass_t* model, const float32_t* X,
                                        const int* y, int num_samples, int max_epochs) {
    if (!model || !X || !y || num_samples <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    for (int c = 0; c < model->num_classes; c++) {
        for (int epoch = 0; epoch < max_epochs; epoch++) {
            for (int i = 0; i < num_samples; i++) {
                const float32_t* x = &X[i * model->num_features];
                int binary_y = (y[i] == c) ? 1 : 0;
                eif_logreg_update(&model->classifiers[c], x, binary_y);
            }
        }
    }
    
    return EIF_STATUS_OK;
}

int eif_logreg_multiclass_predict(const eif_logreg_multiclass_t* model, const float32_t* x) {
    if (!model || !x) return 0;
    
    int best_class = 0;
    float32_t best_prob = eif_logreg_predict_proba(&model->classifiers[0], x);
    
    for (int c = 1; c < model->num_classes; c++) {
        float32_t prob = eif_logreg_predict_proba(&model->classifiers[c], x);
        if (prob > best_prob) {
            best_prob = prob;
            best_class = c;
        }
    }
    
    return best_class;
}
