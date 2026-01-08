#include "eif_ml_kmeans_fixed.h"
#include "eif_hal_simd.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int32_t euclidean_dist_sq_fixed(const q15_t* a, const q15_t* b, int dims) {
    return eif_simd_dist_sq_q15(a, b, dims);
}

eif_status_t eif_kmeans_init_fixed(eif_kmeans_fixed_t* model, int k, int num_features) {
    if (!model || k <= 0 || num_features <= 0) return EIF_STATUS_ERROR;
    
    model->config.k = k;
    model->config.max_iterations = 100;
    model->config.init = EIF_KMEANS_INIT_RANDOM_FIXED;
    model->num_features = num_features;
    
    model->centroids = (q15_t*)malloc(k * num_features * sizeof(q15_t));
    if (!model->centroids) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Initialize to 0
    memset(model->centroids, 0, k * num_features * sizeof(q15_t));
    
    return EIF_STATUS_OK;
}

int eif_kmeans_predict_fixed(const eif_kmeans_fixed_t* model, const q15_t* sample) {
    if (!model || !sample) return -1;
    
    int best_cluster = -1;
    int32_t min_dist = INT32_MAX;
    
    for (int i = 0; i < model->config.k; i++) {
        const q15_t* centroid = &model->centroids[i * model->num_features];
        int32_t dist = euclidean_dist_sq_fixed(sample, centroid, model->num_features);
        if (dist < min_dist) {
            min_dist = dist;
            best_cluster = i;
        }
    }
    return best_cluster;
}

int eif_kmeans_fit_fixed(eif_kmeans_fixed_t* model, const q15_t* data, int num_samples) {
    int k = model->config.k;
    int dims = model->num_features;
    
    // 1. Init Centroids (Random pick from data)
    for (int i = 0; i < k; i++) {
        // Pseudo-random index
        int idx = (i * 12345) % num_samples;
        memcpy(&model->centroids[i * dims], &data[idx * dims], dims * sizeof(q15_t));
    }
    
    // Alloc assignments and counts
    int* assignments = (int*)malloc(num_samples * sizeof(int));
    int* counts = (int*)malloc(k * sizeof(int));
    // Accumulators for centroids (int32 to avoid overflow accumulation)
    int32_t* acc_centroids = (int32_t*)malloc(k * dims * sizeof(int32_t));
    
    int iter = 0; 
    for (; iter < model->config.max_iterations; iter++) {
        int changes = 0;
        
        // E-Step: Assign
        for (int i = 0; i < num_samples; i++) {
            int old_cluster = (iter == 0) ? -1 : assignments[i];
            int new_cluster = eif_kmeans_predict_fixed(model, &data[i * dims]);
            assignments[i] = new_cluster;
            if (new_cluster != old_cluster) changes++;
        }
        
        if (iter > 0 && changes == 0) break;
        
        // M-Step: Update
        memset(counts, 0, k * sizeof(int));
        memset(acc_centroids, 0, k * dims * sizeof(int32_t));
        
        for (int i = 0; i < num_samples; i++) {
            int c = assignments[i];
            counts[c]++;
            for (int d = 0; d < dims; d++) {
                acc_centroids[c * dims + d] += data[i * dims + d];
            }
        }
        
        for (int i = 0; i < k; i++) {
            if (counts[i] > 0) {
                for (int d = 0; d < dims; d++) {
                   model->centroids[i * dims + d] = (q15_t)(acc_centroids[i * dims + d] / counts[i]);
                }
            }
        }
    }
    
    free(assignments);
    free(counts);
    free(acc_centroids);
    return iter;
}

void eif_kmeans_quantize_sample(const float* src, q15_t* dst, int dims) {
    for (int i = 0; i < dims; i++) {
        dst[i] = EIF_FLOAT_TO_Q15(src[i]);
    }
}

void eif_kmeans_free_fixed(eif_kmeans_fixed_t* model) {
    if (model) {
        if (model->centroids) free(model->centroids);
        model->centroids = NULL;
    }
}
