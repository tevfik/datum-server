/**
 * @file eif_data_analysis.h
 * @brief EIF Data Analysis Module - Umbrella Header
 * 
 * This is the main include for data analysis functionality.
 * Includes all submodule headers for convenience.
 * 
 * Submodules:
 * - eif_ml.h         : ML algorithms (trees, linear, cluster)
 * - eif_timeseries.h : ARIMA, Holt-Winters, Decomposition, ACF
 */

#ifndef EIF_DATA_ANALYSIS_H
#define EIF_DATA_ANALYSIS_H

// Core dependencies
#include "eif_core.h"
#include "eif_matrix.h"
#include "eif_status.h"
#include "eif_types.h"

// Include all submodule headers
#include "eif_ml.h"
#include "eif_ts.h"

// ============================================================================
// PCA (Principal Component Analysis)
// ============================================================================

typedef struct {
    int num_components;
} eif_pca_config_t;

eif_status_t eif_pca_compute(const eif_pca_config_t* config, 
                             const float32_t* input, 
                             int num_samples, 
                             int num_features, 
                             float32_t* components, 
                             float32_t* explained_variance,
                             eif_memory_pool_t* pool);

// ============================================================================
// Scalers (Feature Normalization)
// ============================================================================

// MinMax: Scale to [min, max]
eif_status_t eif_scaler_minmax_fit(const float32_t* input, int num_samples, 
                                    int num_features, float32_t* min_vals, 
                                    float32_t* max_vals);
eif_status_t eif_scaler_minmax_transform(const float32_t* input, int num_samples, 
                                          int num_features, const float32_t* min_vals, 
                                          const float32_t* max_vals, 
                                          float32_t feature_range_min, 
                                          float32_t feature_range_max, 
                                          float32_t* output);

// Standard: Scale to mean=0, std=1
eif_status_t eif_scaler_standard_fit(const float32_t* input, int num_samples, 
                                      int num_features, float32_t* mean_vals, 
                                      float32_t* std_vals);
eif_status_t eif_scaler_standard_transform(const float32_t* input, int num_samples, 
                                            int num_features, const float32_t* mean_vals, 
                                            const float32_t* std_vals, float32_t* output);

// ============================================================================
// KNN (K-Nearest Neighbors)
// ============================================================================

int eif_knn_predict(int k, int num_classes, 
                    const float32_t* train_data, const int* train_labels, 
                    int num_train_samples, int num_features, 
                    const float32_t* input, eif_memory_pool_t* pool);

// ============================================================================
// Online Anomaly Detection (Running Statistics)
// ============================================================================

typedef struct {
    int num_features;
    float32_t* mean;
    float32_t* M2;       // Sum of squares of differences from mean
    float32_t count;
} eif_anomaly_online_t;

eif_status_t eif_anomaly_online_init(eif_anomaly_online_t* model, int num_features, 
                                      eif_memory_pool_t* pool);
eif_status_t eif_anomaly_online_update(eif_anomaly_online_t* model, const float32_t* input);
float32_t eif_anomaly_online_score(const eif_anomaly_online_t* model, const float32_t* input);

#endif // EIF_DATA_ANALYSIS_H
