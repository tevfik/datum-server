/**
 * @file eif_anomaly.h
 * @brief Anomaly Detection Pipeline for Edge AI
 * 
 * Multi-method anomaly detection combining:
 * - Statistical methods (Z-score, MAD)
 * - Machine Learning (Isolation Forest)
 * - Time Series (ARIMA residuals, Matrix Profile)
 * 
 * Use cases:
 * - Predictive maintenance
 * - Sensor drift detection
 * - Manufacturing quality control
 * - Network intrusion detection
 */

#ifndef EIF_ANOMALY_H
#define EIF_ANOMALY_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"
#include "eif_ml.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constants
// =============================================================================

#define EIF_ANOMALY_MAX_FEATURES     16
#define EIF_ANOMALY_HISTORY_SIZE     256
#define EIF_ANOMALY_ENSEMBLE_SIZE    3

// =============================================================================
// Anomaly Detection Methods
// =============================================================================

typedef enum {
    EIF_ANOMALY_ZSCORE,         /**< Z-score based (fast) */
    EIF_ANOMALY_MAD,            /**< Median Absolute Deviation (robust) */
    EIF_ANOMALY_IFOREST,        /**< Isolation Forest (multivariate) */
    EIF_ANOMALY_RESIDUAL,       /**< Time series residual (temporal) */
    EIF_ANOMALY_ENSEMBLE        /**< Combine multiple methods */
} eif_anomaly_method_t;

// =============================================================================
// Statistical Detector (Online)
// =============================================================================

/**
 * @brief Online statistical anomaly detector
 */
typedef struct {
    // Running statistics
    float32_t mean;
    float32_t variance;
    float32_t m2;               /**< For Welford's algorithm */
    float32_t median;
    float32_t mad;              /**< Median Absolute Deviation */
    
    // History buffer for median calculation
    float32_t* history;
    int history_size;
    int history_idx;
    int count;
    
    // Thresholds
    float32_t z_threshold;      /**< Z-score threshold (default: 3.0) */
    float32_t mad_threshold;    /**< MAD threshold (default: 3.5) */
    
    eif_memory_pool_t* pool;
} eif_stat_detector_t;

/**
 * @brief Initialize statistical detector
 */
eif_status_t eif_stat_detector_init(eif_stat_detector_t* det,
                                     int history_size,
                                     float32_t z_threshold,
                                     eif_memory_pool_t* pool);

/**
 * @brief Update detector with new sample and check for anomaly
 * @return Anomaly score (0.0 = normal, 1.0 = definite anomaly)
 */
float32_t eif_stat_detector_update(eif_stat_detector_t* det, float32_t value);

/**
 * @brief Check if value is anomaly without updating
 */
bool eif_stat_detector_is_anomaly(const eif_stat_detector_t* det, 
                                   float32_t value);

// =============================================================================
// Multivariate Anomaly Detector
// =============================================================================

/**
 * @brief Multivariate anomaly detector using Isolation Forest
 */
typedef struct {
    eif_iforest_t forest;
    float32_t threshold;        /**< Anomaly score threshold */
    int num_features;
    bool fitted;
    eif_memory_pool_t* pool;
} eif_mv_detector_t;

/**
 * @brief Initialize multivariate detector
 */
eif_status_t eif_mv_detector_init(eif_mv_detector_t* det,
                                   int num_features,
                                   int num_trees,
                                   float32_t threshold,
                                   eif_memory_pool_t* pool);

/**
 * @brief Fit detector on normal data
 */
eif_status_t eif_mv_detector_fit(eif_mv_detector_t* det,
                                  const float32_t* data,
                                  int num_samples);

/**
 * @brief Score sample for anomaly (higher = more anomalous)
 */
float32_t eif_mv_detector_score(const eif_mv_detector_t* det,
                                 const float32_t* sample);

/**
 * @brief Check if sample is anomaly
 */
bool eif_mv_detector_is_anomaly(const eif_mv_detector_t* det,
                                 const float32_t* sample);

// =============================================================================
// Time Series Anomaly Detector
// =============================================================================

/**
 * @brief Time series anomaly detector using residual analysis
 */
typedef struct {
    // Exponential Weighted Moving Average
    float32_t ewma;
    float32_t ewma_var;
    float32_t alpha;            /**< Smoothing factor (0-1) */
    
    // Change detection (CUSUM)
    float32_t cusum_pos;
    float32_t cusum_neg;
    float32_t cusum_threshold;
    
    // Statistics
    int count;
    float32_t threshold;
    
    eif_memory_pool_t* pool;
} eif_ts_detector_t;

/**
 * @brief Initialize time series detector
 */
eif_status_t eif_ts_detector_init(eif_ts_detector_t* det,
                                   float32_t alpha,
                                   float32_t threshold,
                                   eif_memory_pool_t* pool);

/**
 * @brief Update with new value and return anomaly score
 */
float32_t eif_ts_detector_update(eif_ts_detector_t* det, float32_t value);

/**
 * @brief Check for change point (CUSUM)
 */
bool eif_ts_detector_changepoint(const eif_ts_detector_t* det);

// =============================================================================
// Ensemble Anomaly Detector
// =============================================================================

/**
 * @brief Ensemble combining multiple detection methods
 */
typedef struct {
    eif_stat_detector_t* stat_det;   /**< Per-feature statistical detectors */
    eif_mv_detector_t mv_det;        /**< Multivariate detector */
    eif_ts_detector_t* ts_det;       /**< Per-feature time series detectors */
    
    int num_features;
    float32_t* weights;              /**< Method weights for fusion */
    float32_t threshold;
    
    eif_memory_pool_t* pool;
} eif_ensemble_detector_t;

/**
 * @brief Initialize ensemble detector
 */
eif_status_t eif_ensemble_init(eif_ensemble_detector_t* det,
                                int num_features,
                                int history_size,
                                eif_memory_pool_t* pool);

/**
 * @brief Fit ensemble on normal data
 */
eif_status_t eif_ensemble_fit(eif_ensemble_detector_t* det,
                               const float32_t* data,
                               int num_samples);

/**
 * @brief Score sample using ensemble
 */
float32_t eif_ensemble_score(eif_ensemble_detector_t* det,
                              const float32_t* sample);

/**
 * @brief Check for anomaly
 */
bool eif_ensemble_is_anomaly(eif_ensemble_detector_t* det,
                              const float32_t* sample);

/**
 * @brief Get individual method scores
 */
eif_status_t eif_ensemble_scores(eif_ensemble_detector_t* det,
                                  const float32_t* sample,
                                  float32_t* stat_score,
                                  float32_t* mv_score,
                                  float32_t* ts_score);

#ifdef __cplusplus
}
#endif

#endif // EIF_ANOMALY_H
