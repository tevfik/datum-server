/**
 * @file eif_ml_knn_fixed.c
 * @brief k-Nearest Neighbors (k-NN) Classifier (Fixed-Point Q15) Implementation
 */

#include "eif_ml_knn_fixed.h"
#include "eif_hal_simd.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MAX_K 32 // Maximum K value supported

typedef struct {
    uint32_t distance;
    int32_t label;
} Neighbor;

void eif_ml_knn_init_fixed(eif_ml_knn_fixed_t *knn, const q15_t *train_data,
                           const int32_t *train_labels, int num_samples,
                           int num_features, int k) {
    knn->train_data = train_data;
    knn->train_labels = train_labels;
    knn->num_samples = num_samples;
    knn->num_features = num_features;
    knn->k = (k > MAX_K) ? MAX_K : k;
}

int32_t eif_ml_knn_predict_fixed(const eif_ml_knn_fixed_t *knn, const q15_t *input) {
    if (!knn || !input) return -1;

    Neighbor neighbors[MAX_K];
    
    // Initialize neighbors with max distance
    for (int i = 0; i < knn->k; i++) {
        neighbors[i].distance = UINT32_MAX;
        neighbors[i].label = -1;
    }

    // Iterate through all training samples
    for (int i = 0; i < knn->num_samples; i++) {
        // Calculate squared Euclidean distance
        uint32_t dist = eif_simd_dist_sq_q15(
            input, 
            &knn->train_data[i * knn->num_features], 
            knn->num_features
        );

        // Check if this sample is closer than the farthest stored neighbor
        // We maintain the array somewhat sorted or just simply replace the worst one
        
        // Find the worst neighbor currently in our list (largest distance)
        int worst_idx = -1;
        uint32_t max_dist = 0;
        
        // Strategy: Fill the array first
        // If array is full, find max and replace if current is smaller
        
        // More optimal: keep track of max_dist in the set
        // Since K is small, we can just do a linear scan of our K neighbors
        
        int replace_idx = -1;
        uint32_t current_max_in_knn = 0;
        
        for(int j=0; j<knn->k; j++) {
            if (neighbors[j].distance > current_max_in_knn) {
                current_max_in_knn = neighbors[j].distance;
                replace_idx = j;
            }
        }
        
        if (dist < current_max_in_knn) {
            neighbors[replace_idx].distance = dist;
            neighbors[replace_idx].label = knn->train_labels[i];
        }
    }

    // Voting
    // Simple majority vote. 
    // Assuming small number of classes, we can use a small map or just iterate.
    // For general robustness, we'll brute force the frequency count of the K neighbors.
    
    int32_t best_label = -1;
    int max_votes = -1;

    for (int i = 0; i < knn->k; i++) {
        int current_label = neighbors[i].label;
        if (current_label == -1) continue;
        
        int votes = 1;
        for (int j = i + 1; j < knn->k; j++) {
            if (neighbors[j].label == current_label) {
                votes++;
            }
        }
        
        if (votes > max_votes) {
            max_votes = votes;
            best_label = current_label;
        }
    }

    return best_label;
}
