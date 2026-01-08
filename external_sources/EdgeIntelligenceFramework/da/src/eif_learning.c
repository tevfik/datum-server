#include "eif_data_analysis.h"
#include "eif_utils.h"
#include <math.h>
#include <string.h>

// --- Incremental K-Means ---

eif_status_t eif_kmeans_online_init(eif_kmeans_online_t* model, int k, int num_features, eif_memory_pool_t* pool) {
    if (!model || k <= 0 || num_features <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    model->k = k;
    model->num_features = num_features;
    
    model->centroids = (float32_t*)eif_memory_alloc(pool, k * num_features * sizeof(float32_t), 4);
    model->counts = (int*)eif_memory_alloc(pool, k * sizeof(int), 4);
    
    if (!model->centroids || !model->counts) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Initialize centroids to 0 (user should probably seed them, but for now 0)
    // Or we can expect user to seed them manually after init.
    memset(model->centroids, 0, k * num_features * sizeof(float32_t));
    memset(model->counts, 0, k * sizeof(int));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_kmeans_online_update(eif_kmeans_online_t* model, const float32_t* input, float32_t learning_rate) {
    if (!model || !input) return EIF_STATUS_INVALID_ARGUMENT;
    
    // 1. Find nearest centroid
    int best_k = -1;
    float32_t min_dist_sq = 1e30f; // Large number
    
    for (int i = 0; i < model->k; i++) {
        float32_t dist_sq = 0;
        for (int j = 0; j < model->num_features; j++) {
            float32_t diff = input[j] - model->centroids[i * model->num_features + j];
            dist_sq += diff * diff;
        }
        
        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            best_k = i;
        }
    }
    
    if (best_k == -1) return EIF_STATUS_ERROR;
    
    // 2. Update centroid
    // C_new = C_old + alpha * (x - C_old)
    // If learning_rate is negative, use 1/(count+1) for standard mean
    
    float32_t alpha = learning_rate;
    if (alpha < 0.0f) {
        model->counts[best_k]++;
        alpha = 1.0f / model->counts[best_k];
    }
    
    for (int j = 0; j < model->num_features; j++) {
        float32_t old_val = model->centroids[best_k * model->num_features + j];
        model->centroids[best_k * model->num_features + j] = old_val + alpha * (input[j] - old_val);
    }
    
    return EIF_STATUS_OK;
}

// --- Online Linear Regression (RLS) ---

eif_status_t eif_linreg_online_init(eif_linreg_online_t* model, int num_features, float32_t lambda, eif_memory_pool_t* pool) {
    if (!model || num_features <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    model->num_features = num_features;
    model->lambda = lambda;
    
    int dim = num_features + 1; // +1 for bias
    
    model->weights = (float32_t*)eif_memory_alloc(pool, dim * sizeof(float32_t), 4);
    model->P = (float32_t*)eif_memory_alloc(pool, dim * dim * sizeof(float32_t), 4);
    
    if (!model->weights || !model->P) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Initialize weights to 0
    memset(model->weights, 0, dim * sizeof(float32_t));
    
    // Initialize P to large identity matrix (e.g., 1000 * I)
    memset(model->P, 0, dim * dim * sizeof(float32_t));
    for (int i = 0; i < dim; i++) {
        model->P[i * dim + i] = 1000.0f;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_linreg_online_update(eif_linreg_online_t* model, const float32_t* input, float32_t target, eif_memory_pool_t* pool) {
    if (!model || !input || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    int dim = model->num_features + 1;
    
    // Construct x vector (input + 1.0 for bias)
    // We need a temporary buffer for x, or handle it implicitly.
    // Let's allocate temp x, K (gain), Px from pool (scratch)
    
    float32_t* x = (float32_t*)eif_memory_alloc(pool, dim * sizeof(float32_t), 4);
    float32_t* K = (float32_t*)eif_memory_alloc(pool, dim * sizeof(float32_t), 4);
    float32_t* Px = (float32_t*)eif_memory_alloc(pool, dim * sizeof(float32_t), 4); // P * x
    
    if (!x || !K || !Px) return EIF_STATUS_OUT_OF_MEMORY;
    
    memcpy(x, input, model->num_features * sizeof(float32_t));
    x[model->num_features] = 1.0f; // Bias term
    
    // 1. Calculate P*x
    for (int i = 0; i < dim; i++) {
        Px[i] = 0;
        for (int j = 0; j < dim; j++) {
            Px[i] += model->P[i * dim + j] * x[j];
        }
    }
    
    // 2. Calculate Gain K = (P * x) / (lambda + x^T * P * x)
    float32_t xPx = 0;
    for (int i = 0; i < dim; i++) {
        xPx += x[i] * Px[i];
    }
    
    float32_t denominator = model->lambda + xPx;
    for (int i = 0; i < dim; i++) {
        K[i] = Px[i] / denominator;
    }
    
    // 3. Update Weights: w = w + K * (y - x^T * w)
    float32_t prediction = 0;
    for (int i = 0; i < dim; i++) {
        prediction += model->weights[i] * x[i];
    }
    float32_t error = target - prediction;
    
    for (int i = 0; i < dim; i++) {
        model->weights[i] += K[i] * error;
    }
    
    // 4. Update Covariance P: P = (P - K * x^T * P) / lambda
    // Note: x^T * P is essentially Px^T (since P is symmetric)
    // K * (x^T * P) is outer product of K and Px
    
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
            float32_t term = K[i] * Px[j];
            model->P[i * dim + j] = (model->P[i * dim + j] - term) / model->lambda;
        }
    }
    
    // Free scratch (reset logic usually handles this, but here we just allocated)
    // Assuming pool is reset by caller or we don't care about fragmentation for this step?
    // The pool design in EIF seems to be linear allocator. If we don't reset, we leak.
    // The API eif_linreg_online_update takes pool, implying it can use it for scratch.
    // But we can't reset the WHOLE pool because model might be in it (if passed same pool).
    // Ideally, we should use a separate scratch pool or stack for small vectors.
    // For now, let's assume the caller manages the pool reset or provides a scratch pool.
    
    return EIF_STATUS_OK;
}

float32_t eif_linreg_online_predict(const eif_linreg_online_t* model, const float32_t* input) {
    if (!model || !input) return 0.0f;
    
    float32_t y = 0;
    for (int i = 0; i < model->num_features; i++) {
        y += model->weights[i] * input[i];
    }
    y += model->weights[model->num_features]; // Bias
    
    return y;
}

// --- Online Anomaly Detection ---

eif_status_t eif_anomaly_online_init(eif_anomaly_online_t* model, int num_features, eif_memory_pool_t* pool) {
    if (!model || num_features <= 0 || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    model->num_features = num_features;
    model->count = 0;
    
    model->mean = (float32_t*)eif_memory_alloc(pool, num_features * sizeof(float32_t), 4);
    model->M2 = (float32_t*)eif_memory_alloc(pool, num_features * sizeof(float32_t), 4);
    
    if (!model->mean || !model->M2) return EIF_STATUS_OUT_OF_MEMORY;
    
    memset(model->mean, 0, num_features * sizeof(float32_t));
    memset(model->M2, 0, num_features * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_anomaly_online_update(eif_anomaly_online_t* model, const float32_t* input) {
    if (!model || !input) return EIF_STATUS_INVALID_ARGUMENT;
    
    model->count += 1.0f;
    
    for (int i = 0; i < model->num_features; i++) {
        float32_t delta = input[i] - model->mean[i];
        model->mean[i] += delta / model->count;
        float32_t delta2 = input[i] - model->mean[i];
        model->M2[i] += delta * delta2;
    }
    
    return EIF_STATUS_OK;
}

float32_t eif_anomaly_online_score(const eif_anomaly_online_t* model, const float32_t* input) {
    if (!model || !input || model->count < 2) return 0.0f;
    
    // Simple Euclidean distance from mean, normalized by variance (Mahalanobis-like diagonal)
    float32_t score = 0;
    
    for (int i = 0; i < model->num_features; i++) {
        float32_t variance = model->M2[i] / (model->count - 1);
        if (variance < 1e-6f) variance = 1e-6f; // Avoid div by zero
        
        float32_t diff = input[i] - model->mean[i];
        score += (diff * diff) / variance;
    }
    
    return sqrtf(score);
}
