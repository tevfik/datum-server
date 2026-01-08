#include "eif_ml_kmeans.h"
#include "eif_hal_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

static float euclidean_dist_sq(const float* a, const float* b, int dims) {
    return eif_simd_dist_sq_f32(a, b, dims);
}

eif_status_t eif_kmeans_init(eif_kmeans_t* model, int k, int num_features) {
    if (!model || k <= 0 || num_features <= 0) return EIF_STATUS_ERROR;
    
    model->config.k = k;
    model->config.max_iterations = 100;
    model->config.min_change = 0.0001f;
    model->config.init = EIF_KMEANS_INIT_RANDOM;
    model->num_features = num_features;
    
    model->centroids = (float*)malloc(k * num_features * sizeof(float));
    if (!model->centroids) return EIF_STATUS_OUT_OF_MEMORY;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_kmeans_fit(eif_kmeans_t* model, const float* data, int num_samples) {
    if (!model || !data || num_samples < model->config.k) return EIF_STATUS_ERROR;
    
    int k = model->config.k;
    int dims = model->num_features;
    
    // 1. Initialize Centroids (Random Forgy)
    // Pick k random points from data as initial centroids
    for (int i = 0; i < k; i++) {
        int idx = rand() % num_samples;
        memcpy(&model->centroids[i * dims], &data[idx * dims], dims * sizeof(float));
    }
    
    int* assignments = (int*)malloc(num_samples * sizeof(int));
    float* new_centroids = (float*)malloc(k * dims * sizeof(float));
    int* counts = (int*)malloc(k * sizeof(int));
    
    if (!assignments || !new_centroids || !counts) {
        free(assignments); free(new_centroids); free(counts);
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    for (int iter = 0; iter < model->config.max_iterations; iter++) {
        // 2. Assignment Step
        float total_change = 0.0f;
        
        for (int i = 0; i < num_samples; i++) {
            float min_dist = FLT_MAX;
            int best_cluster = 0;
            
            for (int c = 0; c < k; c++) {
                float dist = euclidean_dist_sq(&data[i * dims], &model->centroids[c * dims], dims);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = c;
                }
            }
            assignments[i] = best_cluster;
        }
        
        // 3. Update Step
        memset(new_centroids, 0, k * dims * sizeof(float));
        memset(counts, 0, k * sizeof(int));
        
        for (int i = 0; i < num_samples; i++) {
            int cluster = assignments[i];
            counts[cluster]++;
            for (int j = 0; j < dims; j++) {
                new_centroids[cluster * dims + j] += data[i * dims + j];
            }
        }
        
        float max_centroid_move = 0.0f;
        
        for (int c = 0; c < k; c++) {
            if (counts[c] > 0) {
                for (int j = 0; j < dims; j++) {
                    new_centroids[c * dims + j] /= counts[c];
                }
            } else {
                // Handle empty cluster (re-initialize randomly)
                int idx = rand() % num_samples;
                memcpy(&new_centroids[c * dims], &data[idx * dims], dims * sizeof(float));
            }
            
            float move = euclidean_dist_sq(&model->centroids[c * dims], &new_centroids[c * dims], dims);
            if (move > max_centroid_move) max_centroid_move = move;
            
            memcpy(&model->centroids[c * dims], &new_centroids[c * dims], dims * sizeof(float));
        }
        
        if (max_centroid_move < model->config.min_change) {
            break; // Converged
        }
    }
    
    free(assignments);
    free(new_centroids);
    free(counts);
    return EIF_STATUS_OK;
}

int eif_kmeans_predict(const eif_kmeans_t* model, const float* sample) {
    if (!model || !sample) return -1;
    
    float min_dist = FLT_MAX;
    int best_cluster = 0;
    
    for (int c = 0; c < model->config.k; c++) {
        float dist = euclidean_dist_sq(sample, &model->centroids[c * model->num_features], model->num_features);
        if (dist < min_dist) {
            min_dist = dist;
            best_cluster = c;
        }
    }
    return best_cluster;
}

void eif_kmeans_free(eif_kmeans_t* model) {
    if (model) {
        if (model->centroids) free(model->centroids);
        model->centroids = NULL;
    }
}
