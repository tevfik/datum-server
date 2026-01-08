/**
 * @file eif_ml_pca.c
 * @brief Principal Component Analysis Implementation
 */

#include "eif_ml_pca.h"
#include <math.h>
#include <string.h>

// ============================================================================
// Internal Helper Functions
// ============================================================================

// Compute mean of each feature
static void compute_mean(const float32_t *X, uint32_t n_samples,
                        uint16_t n_features, float32_t *mean) {
  memset(mean, 0, n_features * sizeof(float32_t));

  for (uint32_t i = 0; i < n_samples; i++) {
    for (uint16_t j = 0; j < n_features; j++) {
      mean[j] += X[i * n_features + j];
    }
  }

  for (uint16_t j = 0; j < n_features; j++) {
    mean[j] /= (float32_t)n_samples;
  }
}

// Compute standard deviation of each feature
static void compute_std(const float32_t *X, uint32_t n_samples,
                       uint16_t n_features, const float32_t *mean,
                       float32_t *std) {
  memset(std, 0, n_features * sizeof(float32_t));

  for (uint32_t i = 0; i < n_samples; i++) {
    for (uint16_t j = 0; j < n_features; j++) {
      float32_t diff = X[i * n_features + j] - mean[j];
      std[j] += diff * diff;
    }
  }

  for (uint16_t j = 0; j < n_features; j++) {
    std[j] = sqrtf(std[j] / (float32_t)(n_samples - 1));
    if (std[j] < 1e-10f) {
      std[j] = 1.0f; // Avoid division by zero
    }
  }
}

// Standardize data (zero mean, unit variance)
static void standardize_data(float32_t *X_std, const float32_t *X,
                             uint32_t n_samples, uint16_t n_features,
                             const float32_t *mean, const float32_t *std) {
  for (uint32_t i = 0; i < n_samples; i++) {
    for (uint16_t j = 0; j < n_features; j++) {
      X_std[i * n_features + j] =
          (X[i * n_features + j] - mean[j]) / std[j];
    }
  }
}

// Compute covariance matrix: C = (1/n) * X^T * X
static void compute_covariance(const float32_t *X_std, uint32_t n_samples,
                               uint16_t n_features, float32_t *cov,
                               eif_memory_pool_t *pool) {
  memset(cov, 0, n_features * n_features * sizeof(float32_t));

  for (uint16_t i = 0; i < n_features; i++) {
    for (uint16_t j = i; j < n_features; j++) {
      float32_t sum = 0.0f;
      for (uint32_t k = 0; k < n_samples; k++) {
        sum += X_std[k * n_features + i] * X_std[k * n_features + j];
      }
      cov[i * n_features + j] = sum / (float32_t)(n_samples - 1);
      cov[j * n_features + i] = cov[i * n_features + j]; // Symmetric
    }
  }
}

// Power iteration for computing dominant eigenvector
static float32_t power_iteration(const float32_t *A, uint16_t n,
                                 float32_t *eigenvector,
                                 eif_memory_pool_t *pool) {
  // Initialize with random vector
  for (uint16_t i = 0; i < n; i++) {
    eigenvector[i] = 1.0f / sqrtf((float32_t)n);
  }

  float32_t *temp = eif_memory_alloc(pool, n * sizeof(float32_t), 4);
  float32_t eigenvalue = 0.0f;

  for (uint32_t iter = 0; iter < EIF_PCA_MAX_ITERATIONS; iter++) {
    // temp = A * eigenvector
    for (uint16_t i = 0; i < n; i++) {
      temp[i] = 0.0f;
      for (uint16_t j = 0; j < n; j++) {
        temp[i] += A[i * n + j] * eigenvector[j];
      }
    }

    // Compute eigenvalue (Rayleigh quotient)
    float32_t numerator = 0.0f, denominator = 0.0f;
    for (uint16_t i = 0; i < n; i++) {
      numerator += eigenvector[i] * temp[i];
      denominator += eigenvector[i] * eigenvector[i];
    }
    float32_t new_eigenvalue = numerator / denominator;

    // Normalize
    float32_t norm = 0.0f;
    for (uint16_t i = 0; i < n; i++) {
      norm += temp[i] * temp[i];
    }
    norm = sqrtf(norm);

    for (uint16_t i = 0; i < n; i++) {
      eigenvector[i] = temp[i] / norm;
    }

    // Check convergence
    if (fabsf(new_eigenvalue - eigenvalue) < EIF_PCA_CONVERGENCE_TOL) {
      eigenvalue = new_eigenvalue;
      break;
    }
    eigenvalue = new_eigenvalue;
  }

  return eigenvalue;
}

// Deflate matrix by removing influence of found eigenvector
static void deflate_matrix(float32_t *A, uint16_t n,
                          const float32_t *eigenvector, float32_t eigenvalue) {
  for (uint16_t i = 0; i < n; i++) {
    for (uint16_t j = 0; j < n; j++) {
      A[i * n + j] -= eigenvalue * eigenvector[i] * eigenvector[j];
    }
  }
}

// ============================================================================
// Public API Implementation
// ============================================================================

eif_status_t eif_pca_init(eif_pca_t *pca, uint16_t n_features,
                          uint16_t n_components, eif_memory_pool_t *pool) {
  if (!pca || !pool || n_features == 0 || n_components == 0 ||
      n_components > n_features) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  pca->n_features = n_features;
  pca->n_components = n_components;
  pca->is_fitted = 0;
  pca->total_variance = 0.0f;

  pca->mean = eif_memory_alloc(pool, n_features * sizeof(float32_t), 4);
  pca->std = eif_memory_alloc(pool, n_features * sizeof(float32_t), 4);
  pca->components =
      eif_memory_alloc(pool, n_components * n_features * sizeof(float32_t), 4);
  pca->explained_variance =
      eif_memory_alloc(pool, n_components * sizeof(float32_t), 4);

  if (!pca->mean || !pca->std || !pca->components ||
      !pca->explained_variance) {
    return EIF_STATUS_OUT_OF_MEMORY;
  }

  return EIF_STATUS_OK;
}

eif_status_t eif_pca_fit(eif_pca_t *pca, const float32_t *X, uint32_t n_samples,
                         eif_memory_pool_t *pool) {
  if (!pca || !X || n_samples < 2) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  // Compute mean and std
  compute_mean(X, n_samples, pca->n_features, pca->mean);
  compute_std(X, n_samples, pca->n_features, pca->mean, pca->std);

  // Standardize data
  float32_t *X_std = eif_memory_alloc(
      pool, n_samples * pca->n_features * sizeof(float32_t), 4);
  if (!X_std) {
    return EIF_STATUS_OUT_OF_MEMORY;
  }
  standardize_data(X_std, X, n_samples, pca->n_features, pca->mean, pca->std);

  // Compute covariance matrix
  float32_t *cov = eif_memory_alloc(
      pool, pca->n_features * pca->n_features * sizeof(float32_t), 4);
  if (!cov) {
    return EIF_STATUS_OUT_OF_MEMORY;
  }
  compute_covariance(X_std, n_samples, pca->n_features, cov, pool);

  // Compute total variance
  pca->total_variance = 0.0f;
  for (uint16_t i = 0; i < pca->n_features; i++) {
    pca->total_variance += cov[i * pca->n_features + i];
  }

  // Find principal components using power iteration
  float32_t *cov_copy = eif_memory_alloc(
      pool, pca->n_features * pca->n_features * sizeof(float32_t), 4);
  memcpy(cov_copy, cov, pca->n_features * pca->n_features * sizeof(float32_t));

  for (uint16_t k = 0; k < pca->n_components; k++) {
    float32_t *eigenvector = &pca->components[k * pca->n_features];

    // Find k-th principal component
    float32_t eigenvalue =
        power_iteration(cov_copy, pca->n_features, eigenvector, pool);

    pca->explained_variance[k] = eigenvalue;

    // Deflate for next component
    if (k < pca->n_components - 1) {
      deflate_matrix(cov_copy, pca->n_features, eigenvector, eigenvalue);
    }
  }

  pca->is_fitted = 1;
  return EIF_STATUS_OK;
}

eif_status_t eif_pca_transform(const eif_pca_t *pca, const float32_t *X,
                               uint32_t n_samples, float32_t *X_transformed) {
  if (!pca || !X || !X_transformed || !pca->is_fitted) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  for (uint32_t i = 0; i < n_samples; i++) {
    eif_status_t status = eif_pca_transform_sample(
        pca, &X[i * pca->n_features], &X_transformed[i * pca->n_components]);
    (void)status; // Always OK given checks above
  }

  return EIF_STATUS_OK;
}

eif_status_t eif_pca_transform_sample(const eif_pca_t *pca, const float32_t *x,
                                      float32_t *x_transformed) {
  if (!pca || !x || !x_transformed || !pca->is_fitted) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  // Standardize input
  float32_t x_std[pca->n_features];
  for (uint16_t j = 0; j < pca->n_features; j++) {
    x_std[j] = (x[j] - pca->mean[j]) / pca->std[j];
  }

  // Project onto principal components
  for (uint16_t k = 0; k < pca->n_components; k++) {
    x_transformed[k] = 0.0f;
    for (uint16_t j = 0; j < pca->n_features; j++) {
      x_transformed[k] += x_std[j] * pca->components[k * pca->n_features + j];
    }
  }

  return EIF_STATUS_OK;
}

eif_status_t eif_pca_inverse_transform(const eif_pca_t *pca,
                                       const float32_t *X_transformed,
                                       uint32_t n_samples,
                                       float32_t *X_reconstructed) {
  if (!pca || !X_transformed || !X_reconstructed || !pca->is_fitted) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  for (uint32_t i = 0; i < n_samples; i++) {
    // Project back to original space
    for (uint16_t j = 0; j < pca->n_features; j++) {
      float32_t sum = 0.0f;
      for (uint16_t k = 0; k < pca->n_components; k++) {
        sum += X_transformed[i * pca->n_components + k] *
               pca->components[k * pca->n_features + j];
      }
      // Unstandardize
      X_reconstructed[i * pca->n_features + j] = sum * pca->std[j] + pca->mean[j];
    }
  }

  return EIF_STATUS_OK;
}

eif_status_t eif_pca_get_explained_variance_ratio(const eif_pca_t *pca,
                                                  float32_t *ratios) {
  if (!pca || !ratios || !pca->is_fitted) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  for (uint16_t k = 0; k < pca->n_components; k++) {
    ratios[k] = pca->explained_variance[k] / pca->total_variance;
  }

  return EIF_STATUS_OK;
}

eif_status_t eif_pca_get_cumulative_variance(const eif_pca_t *pca,
                                             float32_t *cumulative) {
  if (!pca || !cumulative || !pca->is_fitted) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  float32_t sum = 0.0f;
  for (uint16_t k = 0; k < pca->n_components; k++) {
    sum += pca->explained_variance[k];
    cumulative[k] = sum / pca->total_variance;
  }

  return EIF_STATUS_OK;
}

void eif_pca_cleanup(eif_pca_t *pca) {
  if (!pca)
    return;
  // Memory managed by pool
  pca->mean = NULL;
  pca->std = NULL;
  pca->components = NULL;
  pca->explained_variance = NULL;
  pca->is_fitted = 0;
}
