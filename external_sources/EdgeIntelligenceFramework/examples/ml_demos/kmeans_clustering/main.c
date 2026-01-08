#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "eif_ml_kmeans.h"

// Helper to generate random float in range
float random_float(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

int main(void) {
    printf("=== EIF K-Means Clustering Demo ===\n");
    srand(time(NULL));

    // 1. Setup Data: 3 Clusters in 2D space
    // Cluster 0: Centered at (2, 2)
    // Cluster 1: Centered at (8, 8)
    // Cluster 2: Centered at (2, 8)
    
    int points_per_cluster = 10;
    int total_points = points_per_cluster * 3;
    int dim = 2;
    int k = 3;
    
    float* data = (float*)malloc(total_points * dim * sizeof(float));
    
    printf("Generating %d points...\n", total_points);
    
    for (int i = 0; i < points_per_cluster; i++) {
        // Cluster 0
        data[(i*3 + 0)*dim + 0] = random_float(1.5, 2.5);
        data[(i*3 + 0)*dim + 1] = random_float(1.5, 2.5);
        
        // Cluster 1
        data[(i*3 + 1)*dim + 0] = random_float(7.5, 8.5);
        data[(i*3 + 1)*dim + 1] = random_float(7.5, 8.5);
        
        // Cluster 2
        data[(i*3 + 2)*dim + 0] = random_float(1.5, 2.5);
        data[(i*3 + 2)*dim + 1] = random_float(7.5, 8.5);
    }
    
    // 2. Initialize Model
    eif_kmeans_t km = {0};
    eif_status_t status = eif_kmeans_init(&km, k, dim);
    if (status != EIF_STATUS_OK) {
        printf("Failed to init KMeans\n");
        return -1;
    }
    
    // 3. Train
    printf("Training K-Means (K=%d)...\n", k);
    int iterations = eif_kmeans_fit(&km, data, total_points);
    printf("Converged in %d iterations.\n", iterations);
    
    // 4. Print Centroids
    printf("\nCentroids:\n");
    for (int i = 0; i < k; i++) {
        printf("  Cluster %d: (%.2f, %.2f)\n", i, km.centroids[i*dim], km.centroids[i*dim+1]);
    }
    
    // 5. Predict
    float test_pt[] = {2.1f, 2.1f};
    int cluster = eif_kmeans_predict(&km, test_pt);
    printf("\nPrediction for (2.1, 2.1): Cluster %d\n", cluster);
    
    float test_pt2[] = {8.0f, 8.0f};
    int cluster2 = eif_kmeans_predict(&km, test_pt2);
    printf("Prediction for (8.0, 8.0): Cluster %d\n", cluster2);

    eif_kmeans_free(&km);
    free(data);
    
    return 0;
}
