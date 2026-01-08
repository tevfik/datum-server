/**
 * @file eif_ml_pca.h
 * @brief Principal Component Analysis (PCA) for Embedded Systems
 *
 * PCA implementation for dimensionality reduction optimized for
 * resource-constrained devices using power iteration method.
 */

#ifndef EIF_ML_PCA_H
#define EIF_ML_PCA_H

#include "eif_memory.h"
#include "eif_status.h"
#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PCA Configuration
// ============================================================================

#define EIF_PCA_MAX_ITERATIONS 100
#define EIF_PCA_CONVERGENCE_TOL 1e-6f

// ============================================================================
// PCA Structure
// ============================================================================

typedef struct {
  uint16_t n_features;          // Original number of features
  uint16_t n_components;        // Number of principal components
  float32_t *mean;              // Feature means [n_features]
  float32_t *std;               // Feature standard deviations [n_features]
  float32_t *components;        // Principal components [n_components x n_features]
  float32_t *explained_variance; // Variance explained by each PC [n_components]
  float32_t total_variance;     // Total variance in data
  uint8_t is_fitted;            // Whether PCA has been fitted
} eif_pca_t;

// ============================================================================
// PCA API
// ============================================================================

/**
 * @brief Initialize PCA structure
 *
 * @param pca Pointer to PCA structure
 * @param n_features Number of original features
 * @param n_components Number of principal components to retain
 * @param pool Memory pool for allocations
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_pca_init(eif_pca_t *pca, uint16_t n_features,
                          uint16_t n_components, eif_memory_pool_t *pool);

/**
 * @brief Fit PCA on dataset
 *
 * Computes mean, covariance matrix, and principal components using
 * power iteration for eigenvalue decomposition.
 *
 * @param pca Pointer to PCA structure
 * @param X Training data [n_samples x n_features]
 * @param n_samples Number of training samples
 * @param pool Memory pool for computations
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_pca_fit(eif_pca_t *pca, const float32_t *X, uint32_t n_samples,
                         eif_memory_pool_t *pool);

/**
 * @brief Transform data to principal component space
 *
 * @param pca Pointer to fitted PCA
 * @param X Input data [n_samples x n_features]
 * @param n_samples Number of samples to transform
 * @param X_transformed Output data [n_samples x n_components]
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_pca_transform(const eif_pca_t *pca, const float32_t *X,
                               uint32_t n_samples, float32_t *X_transformed);

/**
 * @brief Transform single sample to PC space
 *
 * @param pca Pointer to fitted PCA
 * @param x Input sample [n_features]
 * @param x_transformed Output sample [n_components]
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_pca_transform_sample(const eif_pca_t *pca, const float32_t *x,
                                      float32_t *x_transformed);

/**
 * @brief Inverse transform from PC space to original space
 *
 * @param pca Pointer to fitted PCA
 * @param X_transformed Data in PC space [n_samples x n_components]
 * @param n_samples Number of samples
 * @param X_reconstructed Output in original space [n_samples x n_features]
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_pca_inverse_transform(const eif_pca_t *pca,
                                       const float32_t *X_transformed,
                                       uint32_t n_samples,
                                       float32_t *X_reconstructed);

/**
 * @brief Get explained variance ratio for each component
 *
 * @param pca Pointer to fitted PCA
 * @param ratios Output array [n_components]
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_pca_get_explained_variance_ratio(const eif_pca_t *pca,
                                                  float32_t *ratios);

/**
 * @brief Get cumulative explained variance
 *
 * @param pca Pointer to fitted PCA
 * @param cumulative Output array [n_components]
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_pca_get_cumulative_variance(const eif_pca_t *pca,
                                             float32_t *cumulative);

/**
 * @brief Clean up PCA resources
 *
 * @param pca Pointer to PCA structure
 */
void eif_pca_cleanup(eif_pca_t *pca);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_PCA_H
