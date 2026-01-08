#include "eif_data_analysis.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

eif_status_t eif_kmeans_compute(const eif_kmeans_config_t* config, 
                                const float32_t* input, 
                                int num_samples, 
                                int num_features, 
                                float32_t* centroids, 
                                int* assignments,
                                eif_memory_pool_t* pool) {
    if (!config || !input || !centroids || !assignments || !pool || num_samples <= 0 || num_features <= 0 || config->k <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int k = config->k;
    float32_t* new_centroids = (float32_t*)eif_memory_alloc(pool, k * num_features * sizeof(float32_t), 4);
    int* counts = (int*)eif_memory_alloc(pool, k * sizeof(int), 4);
    
    if (!new_centroids || !counts) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    for (int iter = 0; iter < config->max_iterations; iter++) {
        int changed = 0;
        
        // Assignment Step
        for (int i = 0; i < num_samples; i++) {
            float32_t min_dist = FLT_MAX;
            int best_cluster = -1;
            
            for (int c = 0; c < k; c++) {
                float32_t dist = 0.0f;
                for (int f = 0; f < num_features; f++) {
                    float32_t diff = input[i * num_features + f] - centroids[c * num_features + f];
                    dist += diff * diff;
                }
                
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = c;
                }
            }
            
            if (assignments[i] != best_cluster) {
                assignments[i] = best_cluster;
                changed++;
            }
        }
        
        // Update Step
        memset(new_centroids, 0, k * num_features * sizeof(float32_t));
        memset(counts, 0, k * sizeof(int));
        
        for (int i = 0; i < num_samples; i++) {
            int c = assignments[i];
            counts[c]++;
            for (int f = 0; f < num_features; f++) {
                new_centroids[c * num_features + f] += input[i * num_features + f];
            }
        }
        
        float32_t max_shift = 0.0f;
        
        for (int c = 0; c < k; c++) {
            if (counts[c] > 0) {
                for (int f = 0; f < num_features; f++) {
                    float32_t new_val = new_centroids[c * num_features + f] / counts[c];
                    float32_t diff = new_val - centroids[c * num_features + f];
                    max_shift += diff * diff; // Squared shift
                    centroids[c * num_features + f] = new_val;
                }
            }
        }
        
        if (max_shift < config->epsilon && iter > 0) {
            break;
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_linreg_fit_simple(const float32_t* x, const float32_t* y, int num_samples, eif_linreg_model_t* model) {
    if (!x || !y || !model || num_samples <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    float32_t sum_x = 0.0f;
    float32_t sum_y = 0.0f;
    float32_t sum_xy = 0.0f;
    float32_t sum_xx = 0.0f;
    
    for (int i = 0; i < num_samples; i++) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
    }
    
    float32_t n = (float32_t)num_samples;
    float32_t denominator = n * sum_xx - sum_x * sum_x;
    
    if (fabsf(denominator) < 1e-6f) {
        return EIF_STATUS_ERROR; // Vertical line or all x same
    }
    
    model->slope = (n * sum_xy - sum_x * sum_y) / denominator;
    model->intercept = (sum_y - model->slope * sum_x) / n;
    
    return EIF_STATUS_OK;
}

float32_t eif_linreg_predict_simple(const eif_linreg_model_t* model, float32_t x) {
    if (!model) return 0.0f;
    return model->slope * x + model->intercept;
}

// Helper: Jacobi Eigenvalue Algorithm for Symmetric Matrix
static void jacobi_eigenvalue(float32_t* A, int n, float32_t* eigenvectors, float32_t* eigenvalues) {
    // Initialize eigenvectors to identity
    memset(eigenvectors, 0, n * n * sizeof(float32_t));
    for (int i = 0; i < n; i++) eigenvectors[i * n + i] = 1.0f;
    
    // Initialize eigenvalues to diagonal of A
    for (int i = 0; i < n; i++) eigenvalues[i] = A[i * n + i];
    
    int max_iter = 100;
    for (int iter = 0; iter < max_iter; iter++) {
        float32_t max_val = 0.0f;
        int p = -1, q = -1;
        
        // Find max off-diagonal element
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (fabsf(A[i * n + j]) > max_val) {
                    max_val = fabsf(A[i * n + j]);
                    p = i; q = j;
                }
            }
        }
        
        if (max_val < 1e-6f) break; // Converged
        
        // Compute rotation
        float32_t app = A[p * n + p];
        float32_t aqq = A[q * n + q];
        float32_t apq = A[p * n + q];
        
        float32_t phi = 0.5f * atan2f(2 * apq, aqq - app);
        float32_t c = cosf(phi);
        float32_t s = sinf(phi);
        
        // Update A
        // A'[p,p] = c^2 App - 2sc Apq + s^2 Aqq
        // A'[q,q] = s^2 App + 2sc Apq + c^2 Aqq
        // A'[p,q] = 0
        // ... and other elements
        
        // We need to update rows/cols p and q
        // Store old values
        float32_t temp_app = app;
        float32_t temp_aqq = aqq;
        
        A[p * n + p] = c*c*temp_app - 2*s*c*apq + s*s*temp_aqq;
        A[q * n + q] = s*s*temp_app + 2*s*c*apq + c*c*temp_aqq;
        A[p * n + q] = 0.0f;
        A[q * n + p] = 0.0f;
        
        for (int i = 0; i < n; i++) {
            if (i != p && i != q) {
                float32_t aip = A[i * n + p];
                float32_t aiq = A[i * n + q];
                A[i * n + p] = c*aip - s*aiq;
                A[p * n + i] = A[i * n + p];
                A[i * n + q] = s*aip + c*aiq;
                A[q * n + i] = A[i * n + q];
            }
        }
        
        // Update Eigenvectors
        for (int i = 0; i < n; i++) {
            float32_t vip = eigenvectors[i * n + p];
            float32_t viq = eigenvectors[i * n + q];
            eigenvectors[i * n + p] = c*vip - s*viq;
            eigenvectors[i * n + q] = s*vip + c*viq;
        }
        
        // Update Eigenvalues array
        eigenvalues[p] = A[p * n + p];
        eigenvalues[q] = A[q * n + q];
    }
}

eif_status_t eif_pca_compute(const eif_pca_config_t* config, 
                             const float32_t* input, 
                             int num_samples, 
                             int num_features, 
                             float32_t* components, 
                             float32_t* explained_variance,
                             eif_memory_pool_t* pool) {
    if (!config || !input || !components || !explained_variance || !pool || num_samples <= 0 || num_features <= 0 || config->num_components <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // 1. Compute Mean
    float32_t* mean = (float32_t*)eif_memory_alloc(pool, num_features * sizeof(float32_t), 4);
    if (!mean) return EIF_STATUS_OUT_OF_MEMORY;
    
    memset(mean, 0, num_features * sizeof(float32_t));
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < num_features; j++) {
            mean[j] += input[i * num_features + j];
        }
    }
    for (int j = 0; j < num_features; j++) mean[j] /= num_samples;
    
    // 2. Compute Covariance Matrix (num_features x num_features)
    float32_t* cov = (float32_t*)eif_memory_alloc(pool, num_features * num_features * sizeof(float32_t), 4);
    if (!cov) return EIF_STATUS_OUT_OF_MEMORY;
    
    memset(cov, 0, num_features * num_features * sizeof(float32_t));
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < num_features; j++) {
            float32_t val_j = input[i * num_features + j] - mean[j];
            for (int k = j; k < num_features; k++) { // Symmetric
                float32_t val_k = input[i * num_features + k] - mean[k];
                cov[j * num_features + k] += val_j * val_k;
            }
        }
    }
    
    // Normalize by N-1
    for (int j = 0; j < num_features; j++) {
        for (int k = j; k < num_features; k++) {
            cov[j * num_features + k] /= (num_samples - 1);
            cov[k * num_features + j] = cov[j * num_features + k];
        }
    }
    
    // 3. Compute Eigenvalues and Eigenvectors
    float32_t* eigenvectors = (float32_t*)eif_memory_alloc(pool, num_features * num_features * sizeof(float32_t), 4);
    float32_t* eigenvalues = (float32_t*)eif_memory_alloc(pool, num_features * sizeof(float32_t), 4);
    
    if (!eigenvectors || !eigenvalues) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    jacobi_eigenvalue(cov, num_features, eigenvectors, eigenvalues);
    
    // 4. Sort Eigenvalues (Bubble sort for simplicity, num_features is small)
    // We need indices to sort eigenvectors too
    int* indices = (int*)eif_memory_alloc(pool, num_features * sizeof(int), 4);
    if (!indices) return EIF_STATUS_OUT_OF_MEMORY;
    
    for(int i=0; i<num_features; i++) indices[i] = i;
    
    for (int i = 0; i < num_features - 1; i++) {
        for (int j = 0; j < num_features - i - 1; j++) {
            if (eigenvalues[j] < eigenvalues[j + 1]) { // Descending
                float32_t temp = eigenvalues[j];
                eigenvalues[j] = eigenvalues[j + 1];
                eigenvalues[j + 1] = temp;
                
                int temp_idx = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temp_idx;
            }
        }
    }
    
    // 5. Output top K components
    int k = config->num_components;
    if (k > num_features) k = num_features;
    
    for (int i = 0; i < k; i++) {
        explained_variance[i] = eigenvalues[i];
        int idx = indices[i];
        // Eigenvectors are columns in 'eigenvectors' array (from Jacobi implementation above? 
        // Wait, Jacobi updates columns. Yes.
        // Copy column 'idx' to row 'i' of output components?
        // Usually components are returned as rows (n_components x n_features).
        for (int j = 0; j < num_features; j++) {
            components[i * num_features + j] = eigenvectors[j * num_features + idx];
        }
    }
    
    return EIF_STATUS_OK;
}

// --- Scalers ---

eif_status_t eif_scaler_minmax_fit(const float32_t* input, int num_samples, int num_features, float32_t* min_vals, float32_t* max_vals) {
    if (!input || !min_vals || !max_vals || num_samples <= 0 || num_features <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int j = 0; j < num_features; j++) {
        min_vals[j] = FLT_MAX;
        max_vals[j] = -FLT_MAX;
    }
    
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < num_features; j++) {
            float32_t val = input[i * num_features + j];
            if (val < min_vals[j]) min_vals[j] = val;
            if (val > max_vals[j]) max_vals[j] = val;
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_scaler_minmax_transform(const float32_t* input, int num_samples, int num_features, const float32_t* min_vals, const float32_t* max_vals, float32_t feature_range_min, float32_t feature_range_max, float32_t* output) {
    if (!input || !output || !min_vals || !max_vals || num_samples <= 0 || num_features <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    float32_t range_scale = feature_range_max - feature_range_min;
    
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < num_features; j++) {
            float32_t val = input[i * num_features + j];
            float32_t min = min_vals[j];
            float32_t max = max_vals[j];
            float32_t std = (val - min) / (max - min + 1e-9f); // Avoid div by zero
            output[i * num_features + j] = std * range_scale + feature_range_min;
        }
    }
    return EIF_STATUS_OK;
}

eif_status_t eif_scaler_standard_fit(const float32_t* input, int num_samples, int num_features, float32_t* mean_vals, float32_t* std_vals) {
    if (!input || !mean_vals || !std_vals || num_samples <= 0 || num_features <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Compute Mean
    memset(mean_vals, 0, num_features * sizeof(float32_t));
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < num_features; j++) {
            mean_vals[j] += input[i * num_features + j];
        }
    }
    for (int j = 0; j < num_features; j++) mean_vals[j] /= num_samples;
    
    // Compute Std
    memset(std_vals, 0, num_features * sizeof(float32_t));
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < num_features; j++) {
            float32_t diff = input[i * num_features + j] - mean_vals[j];
            std_vals[j] += diff * diff;
        }
    }
    for (int j = 0; j < num_features; j++) {
        std_vals[j] = sqrtf(std_vals[j] / num_samples);
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_scaler_standard_transform(const float32_t* input, int num_samples, int num_features, const float32_t* mean_vals, const float32_t* std_vals, float32_t* output) {
    if (!input || !output || !mean_vals || !std_vals || num_samples <= 0 || num_features <= 0) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < num_features; j++) {
            float32_t val = input[i * num_features + j];
            float32_t mean = mean_vals[j];
            float32_t std = std_vals[j];
            output[i * num_features + j] = (val - mean) / (std + 1e-9f);
        }
    }
    return EIF_STATUS_OK;
}

// --- Distance Metrics ---

float32_t eif_distance_euclidean(const float32_t* a, const float32_t* b, int length) {
    float32_t sum = 0.0f;
    for (int i = 0; i < length; i++) {
        float32_t diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

float32_t eif_distance_manhattan(const float32_t* a, const float32_t* b, int length) {
    float32_t sum = 0.0f;
    for (int i = 0; i < length; i++) {
        sum += fabsf(a[i] - b[i]);
    }
    return sum;
}

float32_t eif_distance_cosine(const float32_t* a, const float32_t* b, int length) {
    float32_t dot = 0.0f;
    float32_t norm_a = 0.0f;
    float32_t norm_b = 0.0f;
    
    for (int i = 0; i < length; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    if (norm_a < 1e-9f || norm_b < 1e-9f) return 0.0f; // Undefined, return 0 or 1? Cosine distance is 1 - similarity.
    // Cosine Similarity = dot / (norm_a * norm_b)
    // Cosine Distance = 1 - Similarity
    return 1.0f - (dot / (sqrtf(norm_a) * sqrtf(norm_b)));
}

// --- KNN ---

typedef struct {
    int index;
    float32_t distance;
} knn_neighbor_t;

int eif_knn_predict(int k, int num_classes, const float32_t* train_data, const int* train_labels, int num_train_samples, int num_features, const float32_t* input, eif_memory_pool_t* pool) {
    if (!train_data || !train_labels || !input || !pool || k <= 0 || num_train_samples <= 0 || num_features <= 0) return -1;
    
    if (k > num_train_samples) k = num_train_samples;
    
    // 1. Compute distances
    knn_neighbor_t* neighbors = (knn_neighbor_t*)eif_memory_alloc(pool, num_train_samples * sizeof(knn_neighbor_t), sizeof(knn_neighbor_t));
    if (!neighbors) return -1;
    
    for (int i = 0; i < num_train_samples; i++) {
        neighbors[i].index = i;
        neighbors[i].distance = eif_distance_euclidean(&train_data[i * num_features], input, num_features);
    }
    
    // 2. Sort (Partial sort top K)
    // Simple bubble sort for K times
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < num_train_samples - i - 1; j++) {
            if (neighbors[j].distance > neighbors[j + 1].distance) {
                knn_neighbor_t temp = neighbors[j];
                neighbors[j] = neighbors[j + 1];
                neighbors[j + 1] = temp;
            }
        }
    }
    // Actually bubble sort pushes largest to end. We want smallest at beginning.
    // Correct logic:
    for (int i = 0; i < num_train_samples - 1; i++) {
        for (int j = 0; j < num_train_samples - i - 1; j++) {
            if (neighbors[j].distance > neighbors[j + 1].distance) {
                knn_neighbor_t temp = neighbors[j];
                neighbors[j] = neighbors[j + 1];
                neighbors[j + 1] = temp;
            }
        }
    }
    // Now sorted ascending.
    
    // 3. Vote
    int* votes = (int*)eif_memory_calloc(pool, num_classes, sizeof(int), sizeof(int));
    if (!votes) return -1;
    
    for (int i = 0; i < k; i++) {
        int label = train_labels[neighbors[i].index];
        if (label >= 0 && label < num_classes) {
            votes[label]++;
        }
    }
    
    // 4. Find max vote
    int max_votes = -1;
    int best_class = -1;
    
    for (int c = 0; c < num_classes; c++) {
        if (votes[c] > max_votes) {
            max_votes = votes[c];
            best_class = c;
        }
    }
    
    return best_class;
}
