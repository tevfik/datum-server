#ifndef EIF_ML_KMEANS_FIXED_H
#define EIF_ML_KMEANS_FIXED_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EIF_KMEANS_INIT_RANDOM_FIXED,
} eif_kmeans_init_fixed_t;

typedef struct {
    int k;
    int max_iterations;
    eif_kmeans_init_fixed_t init;
} eif_kmeans_config_fixed_t;

/**
 * @brief K-Means Model (Fixed Point Q15)
 */
typedef struct {
    eif_kmeans_config_fixed_t config;
    q15_t* centroids;       ///< Flat array: [k][features]
    int num_features;
} eif_kmeans_fixed_t;

/**
 * @brief Initialize KMeans Fixed Model
 */
eif_status_t eif_kmeans_init_fixed(eif_kmeans_fixed_t* model, int k, int num_features);

/**
 * @brief Train (Q15 data)
 */
int eif_kmeans_fit_fixed(eif_kmeans_fixed_t* model, const q15_t* data, int num_samples);

/**
 * @brief Predict (Q15 input)
 */
int eif_kmeans_predict_fixed(const eif_kmeans_fixed_t* model, const q15_t* sample);

/**
 * @brief Quantize inputs (Float -> Q15)
 * Scaling assumption: Data is in range [-1.0, 1.0] or assumed to fit in Q15.
 */
void eif_kmeans_quantize_sample(const float* src, q15_t* dst, int dims);

/**
 * @brief Cleanup
 */
void eif_kmeans_free_fixed(eif_kmeans_fixed_t* model);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_KMEANS_FIXED_H
