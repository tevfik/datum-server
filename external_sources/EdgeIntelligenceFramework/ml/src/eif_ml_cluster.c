/**
 * @file eif_ml_cluster.c
 * @brief Clustering Algorithms Implementation
 * 
 * Implements:
 * - DBSCAN (Density-Based Spatial Clustering)
 */

#include "eif_ml.h"
#include "eif_data_analysis.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>

// ============================================================================
// DBSCAN Implementation
// ============================================================================
// Note: Uses eif_distance_euclidean from eif_data_analysis

/**
 * @brief Find all neighbors within eps distance
 */
static int find_neighbors(const float32_t* data, int num_samples, int num_features,
                          int point_idx, float32_t eps, int* neighbors) {
    int count = 0;
    const float32_t* p = &data[point_idx * num_features];
    
    for (int i = 0; i < num_samples; i++) {
        const float32_t* q = &data[i * num_features];
        float32_t dist = eif_distance_euclidean(p, q, num_features);
        if (dist <= eps) {
            neighbors[count++] = i;
        }
    }
    
    return count;
}

/**
 * @brief Expand cluster from core point
 */
static void expand_cluster(const float32_t* data, int num_samples, int num_features,
                          int point_idx, int* neighbors, int num_neighbors,
                          int cluster_id, int* labels, float32_t eps, int min_pts,
                          bool* visited, int* temp_neighbors, eif_memory_pool_t* pool) {
    labels[point_idx] = cluster_id;
    
    // Process neighbors queue
    int queue_start = 0;
    int queue_end = num_neighbors;
    
    // Copy initial neighbors to temp buffer as queue
    for (int i = 0; i < num_neighbors; i++) {
        temp_neighbors[i] = neighbors[i];
    }
    
    while (queue_start < queue_end) {
        int neighbor_idx = temp_neighbors[queue_start++];
        
        if (!visited[neighbor_idx]) {
            visited[neighbor_idx] = true;
            
            // Find neighbors of this neighbor
            int* new_neighbors = eif_memory_alloc(pool, num_samples * sizeof(int), 4);
            int new_count = find_neighbors(data, num_samples, num_features, 
                                           neighbor_idx, eps, new_neighbors);
            
            // If core point, add its neighbors to queue
            if (new_count >= min_pts) {
                for (int i = 0; i < new_count; i++) {
                    if (queue_end < num_samples) {
                        temp_neighbors[queue_end++] = new_neighbors[i];
                    }
                }
            }
        }
        
        // If not yet assigned to a cluster, assign to this cluster
        if (labels[neighbor_idx] == EIF_DBSCAN_UNDEFINED || 
            labels[neighbor_idx] == EIF_DBSCAN_NOISE) {
            labels[neighbor_idx] = cluster_id;
        }
    }
}

eif_status_t eif_dbscan_compute(const float32_t* data, int num_samples, int num_features,
                                 float32_t eps, int min_pts, 
                                 eif_dbscan_result_t* result, eif_memory_pool_t* pool) {
    if (!data || !result || !pool || num_samples <= 0 || num_features <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    if (eps <= 0 || min_pts <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Allocate result arrays
    result->labels = eif_memory_alloc(pool, num_samples * sizeof(int), 4);
    result->num_samples = num_samples;
    result->num_clusters = 0;
    result->num_noise = 0;
    
    if (!result->labels) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Initialize labels as undefined
    for (int i = 0; i < num_samples; i++) {
        result->labels[i] = EIF_DBSCAN_UNDEFINED;
    }
    
    // Allocate working arrays
    bool* visited = eif_memory_alloc(pool, num_samples * sizeof(bool), 4);
    int* neighbors = eif_memory_alloc(pool, num_samples * sizeof(int), 4);
    int* temp_neighbors = eif_memory_alloc(pool, num_samples * sizeof(int), 4);
    
    if (!visited || !neighbors || !temp_neighbors) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    memset(visited, 0, num_samples * sizeof(bool));
    
    int cluster_id = 0;
    
    // Process each point
    for (int i = 0; i < num_samples; i++) {
        if (visited[i]) continue;
        
        visited[i] = true;
        
        // Find neighbors
        int num_neighbors = find_neighbors(data, num_samples, num_features, i, eps, neighbors);
        
        if (num_neighbors < min_pts) {
            // Mark as noise (may be changed later if reachable from core point)
            result->labels[i] = EIF_DBSCAN_NOISE;
        } else {
            // Core point - start new cluster
            expand_cluster(data, num_samples, num_features, i, neighbors, num_neighbors,
                          cluster_id, result->labels, eps, min_pts, visited, 
                          temp_neighbors, pool);
            cluster_id++;
        }
    }
    
    result->num_clusters = cluster_id;
    
    // Count noise points
    for (int i = 0; i < num_samples; i++) {
        if (result->labels[i] == EIF_DBSCAN_NOISE) {
            result->num_noise++;
        }
    }
    
    return EIF_STATUS_OK;
}

int eif_dbscan_predict(const float32_t* data, const eif_dbscan_result_t* result,
                        int num_features, float32_t eps, const float32_t* new_point) {
    if (!data || !result || !new_point || num_features <= 0) {
        return EIF_DBSCAN_NOISE;
    }
    
    // Find nearest non-noise point within eps
    float32_t min_dist = eps + 1.0f;
    int best_label = EIF_DBSCAN_NOISE;
    
    for (int i = 0; i < result->num_samples; i++) {
        if (result->labels[i] == EIF_DBSCAN_NOISE) continue;
        
        const float32_t* p = &data[i * num_features];
        float32_t dist = eif_distance_euclidean(new_point, p, num_features);
        
        if (dist <= eps && dist < min_dist) {
            min_dist = dist;
            best_label = result->labels[i];
        }
    }
    
    return best_label;
}
