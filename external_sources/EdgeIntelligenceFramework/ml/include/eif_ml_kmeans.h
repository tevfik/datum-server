#ifndef EIF_ML_KMEANS_H
#define EIF_ML_KMEANS_H

#include "eif_types.h"
#include "eif_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EIF_KMEANS_INIT_RANDOM,
    EIF_KMEANS_INIT_KMEANS_PP // K-Means++ (Not implemented yet, placeholder)
} eif_kmeans_init_t;

/**
 * @brief K-Means configuration
 */
typedef struct {
    int k;                  ///< Number of clusters
    int max_iterations;     ///< Maximum iterations
    float min_change;       ///< Minimum centroid change to continue
    eif_kmeans_init_t init; ///< Initialization method
} eif_kmeans_config_t;

/**
 * @brief K-Means Model
 */
typedef struct {
    eif_kmeans_config_t config;
    float* centroids;       ///< Flat array: [k][features]
    int num_features;
} eif_kmeans_t;

/**
 * @brief Initialize K-Means model
 */
eif_status_t eif_kmeans_init(eif_kmeans_t* model, int k, int num_features);

/**
 * @brief Train K-Means model
 * 
 * @param model Initialized model
 * @param data Flat array of input data [num_samples][num_features]
 * @param num_samples Number of samples
 */
eif_status_t eif_kmeans_fit(eif_kmeans_t* model, const float* data, int num_samples);

/**
 * @brief Predict cluster for a single sample
 * 
 * @param model Trained model
 * @param sample Input sample [num_features]
 * @return Cluster index (0 to k-1)
 */
int eif_kmeans_predict(const eif_kmeans_t* model, const float* sample);

/**
 * @brief Free resources
 */
void eif_kmeans_free(eif_kmeans_t* model);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_KMEANS_H
