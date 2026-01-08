/**
 * @file eif_anomaly.c
 * @brief Anomaly Detection Pipeline Implementation
 */

#include "eif_anomaly.h"
#include <string.h>
#include <math.h>

// =============================================================================
// Helper Functions
// =============================================================================

static int compare_float(const void* a, const void* b) {
    float32_t fa = *(const float32_t*)a;
    float32_t fb = *(const float32_t*)b;
    return (fa > fb) - (fa < fb);
}

static float32_t compute_median(float32_t* arr, int n) {
    if (n == 0) return 0.0f;
    
    // Simple insertion sort for small arrays (in-place)
    for (int i = 1; i < n; i++) {
        float32_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
    
    if (n % 2 == 0) {
        return (arr[n/2 - 1] + arr[n/2]) / 2.0f;
    }
    return arr[n/2];
}

static float32_t absf(float32_t x) {
    return x < 0 ? -x : x;
}

// =============================================================================
// Statistical Detector
// =============================================================================

eif_status_t eif_stat_detector_init(eif_stat_detector_t* det,
                                     int history_size,
                                     float32_t z_threshold,
                                     eif_memory_pool_t* pool) {
    if (!det || !pool || history_size <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    det->mean = 0.0f;
    det->variance = 0.0f;
    det->m2 = 0.0f;
    det->median = 0.0f;
    det->mad = 0.0f;
    det->count = 0;
    det->history_idx = 0;
    det->history_size = history_size;
    det->z_threshold = z_threshold;
    det->mad_threshold = 3.5f;
    det->pool = pool;
    
    det->history = (float32_t*)eif_memory_alloc(pool, 
        history_size * sizeof(float32_t), sizeof(float32_t));
    if (!det->history) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    return EIF_STATUS_OK;
}

float32_t eif_stat_detector_update(eif_stat_detector_t* det, float32_t value) {
    if (!det) return 0.0f;
    
    // Welford's online algorithm for mean and variance
    det->count++;
    float32_t delta = value - det->mean;
    det->mean += delta / det->count;
    float32_t delta2 = value - det->mean;
    det->m2 += delta * delta2;
    
    if (det->count > 1) {
        det->variance = det->m2 / (det->count - 1);
    }
    
    // Update history buffer
    det->history[det->history_idx] = value;
    det->history_idx = (det->history_idx + 1) % det->history_size;
    
    // Compute anomaly score (Z-score based)
    float32_t stddev = sqrtf(det->variance + 1e-10f);
    float32_t z_score = absf(value - det->mean) / stddev;
    
    // Normalize to 0-1 range
    float32_t score = z_score / (det->z_threshold * 1.5f);
    if (score > 1.0f) score = 1.0f;
    
    return score;
}

bool eif_stat_detector_is_anomaly(const eif_stat_detector_t* det, 
                                   float32_t value) {
    if (!det || det->count < 2) return false;
    
    float32_t stddev = sqrtf(det->variance + 1e-10f);
    float32_t z_score = absf(value - det->mean) / stddev;
    
    return z_score > det->z_threshold;
}

// =============================================================================
// Multivariate Detector
// =============================================================================

eif_status_t eif_mv_detector_init(eif_mv_detector_t* det,
                                   int num_features,
                                   int num_trees,
                                   float32_t threshold,
                                   eif_memory_pool_t* pool) {
    if (!det || !pool || num_features <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    det->num_features = num_features;
    det->threshold = threshold;
    det->fitted = false;
    det->pool = pool;
    
    // Initialize Isolation Forest
    eif_status_t status = eif_iforest_init(&det->forest, num_trees, 256,
                                            10, num_features, pool);
    return status;
}

eif_status_t eif_mv_detector_fit(eif_mv_detector_t* det,
                                  const float32_t* data,
                                  int num_samples) {
    if (!det || !data || num_samples <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_status_t status = eif_iforest_fit(&det->forest, data, num_samples, det->pool);
    if (status == EIF_STATUS_OK) {
        det->fitted = true;
    }
    return status;
}

float32_t eif_mv_detector_score(const eif_mv_detector_t* det,
                                 const float32_t* sample) {
    if (!det || !sample || !det->fitted) return 0.0f;
    
    return eif_iforest_score(&det->forest, sample);
}

bool eif_mv_detector_is_anomaly(const eif_mv_detector_t* det,
                                 const float32_t* sample) {
    if (!det || !sample || !det->fitted) return false;
    
    float32_t score = eif_iforest_score(&det->forest, sample);
    return score > det->threshold;
}

// =============================================================================
// Time Series Detector (EWMA + CUSUM)
// =============================================================================

eif_status_t eif_ts_detector_init(eif_ts_detector_t* det,
                                   float32_t alpha,
                                   float32_t threshold,
                                   eif_memory_pool_t* pool) {
    if (!det || !pool || alpha <= 0 || alpha > 1) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    det->ewma = 0.0f;
    det->ewma_var = 0.0f;
    det->alpha = alpha;
    det->cusum_pos = 0.0f;
    det->cusum_neg = 0.0f;
    det->cusum_threshold = threshold * 2.0f;
    det->count = 0;
    det->threshold = threshold;
    det->pool = pool;
    
    return EIF_STATUS_OK;
}

float32_t eif_ts_detector_update(eif_ts_detector_t* det, float32_t value) {
    if (!det) return 0.0f;
    
    det->count++;
    
    if (det->count == 1) {
        det->ewma = value;
        det->ewma_var = 0.0f;
        return 0.0f;
    }
    
    // EWMA update
    float32_t residual = value - det->ewma;
    det->ewma = det->alpha * value + (1.0f - det->alpha) * det->ewma;
    
    // Variance estimation
    float32_t residual_sq = residual * residual;
    det->ewma_var = det->alpha * residual_sq + (1.0f - det->alpha) * det->ewma_var;
    
    // CUSUM update for change detection
    float32_t norm_residual = residual / (sqrtf(det->ewma_var) + 1e-10f);
    det->cusum_pos = fmaxf(0.0f, det->cusum_pos + norm_residual - 0.5f);
    det->cusum_neg = fmaxf(0.0f, det->cusum_neg - norm_residual - 0.5f);
    
    // Anomaly score based on normalized residual
    float32_t score = absf(norm_residual) / det->threshold;
    if (score > 1.0f) score = 1.0f;
    
    return score;
}

bool eif_ts_detector_changepoint(const eif_ts_detector_t* det) {
    if (!det) return false;
    
    return det->cusum_pos > det->cusum_threshold || 
           det->cusum_neg > det->cusum_threshold;
}

// =============================================================================
// Ensemble Detector
// =============================================================================

eif_status_t eif_ensemble_init(eif_ensemble_detector_t* det,
                                int num_features,
                                int history_size,
                                eif_memory_pool_t* pool) {
    if (!det || !pool || num_features <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    det->num_features = num_features;
    det->threshold = 0.5f;
    det->pool = pool;
    
    // Allocate statistical detectors (one per feature)
    det->stat_det = (eif_stat_detector_t*)eif_memory_alloc(pool,
        num_features * sizeof(eif_stat_detector_t), sizeof(void*));
    if (!det->stat_det) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Initialize statistical detectors
    for (int i = 0; i < num_features; i++) {
        eif_status_t status = eif_stat_detector_init(&det->stat_det[i],
            history_size, 3.0f, pool);
        if (status != EIF_STATUS_OK) return status;
    }
    
    // Initialize multivariate detector
    eif_status_t status = eif_mv_detector_init(&det->mv_det, num_features,
        10, 0.6f, pool);
    if (status != EIF_STATUS_OK) return status;
    
    // Allocate time series detectors
    det->ts_det = (eif_ts_detector_t*)eif_memory_alloc(pool,
        num_features * sizeof(eif_ts_detector_t), sizeof(void*));
    if (!det->ts_det) return EIF_STATUS_OUT_OF_MEMORY;
    
    // Initialize time series detectors
    for (int i = 0; i < num_features; i++) {
        status = eif_ts_detector_init(&det->ts_det[i], 0.1f, 3.0f, pool);
        if (status != EIF_STATUS_OK) return status;
    }
    
    // Weights for fusion (stat, mv, ts)
    det->weights = (float32_t*)eif_memory_alloc(pool,
        3 * sizeof(float32_t), sizeof(float32_t));
    if (!det->weights) return EIF_STATUS_OUT_OF_MEMORY;
    
    det->weights[0] = 0.3f;  // Statistical
    det->weights[1] = 0.4f;  // Multivariate
    det->weights[2] = 0.3f;  // Time series
    
    return EIF_STATUS_OK;
}

eif_status_t eif_ensemble_fit(eif_ensemble_detector_t* det,
                               const float32_t* data,
                               int num_samples) {
    if (!det || !data || num_samples <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Fit multivariate detector
    eif_status_t status = eif_mv_detector_fit(&det->mv_det, data, num_samples);
    if (status != EIF_STATUS_OK) return status;
    
    // Update statistical and time series detectors with training data
    for (int i = 0; i < num_samples; i++) {
        const float32_t* sample = &data[i * det->num_features];
        for (int j = 0; j < det->num_features; j++) {
            eif_stat_detector_update(&det->stat_det[j], sample[j]);
            eif_ts_detector_update(&det->ts_det[j], sample[j]);
        }
    }
    
    return EIF_STATUS_OK;
}

float32_t eif_ensemble_score(eif_ensemble_detector_t* det,
                              const float32_t* sample) {
    if (!det || !sample) return 0.0f;
    
    // Statistical score (max across features)
    float32_t stat_score = 0.0f;
    for (int i = 0; i < det->num_features; i++) {
        float32_t s = eif_stat_detector_update(&det->stat_det[i], sample[i]);
        if (s > stat_score) stat_score = s;
    }
    
    // Multivariate score
    float32_t mv_score = eif_mv_detector_score(&det->mv_det, sample);
    
    // Time series score (max across features)
    float32_t ts_score = 0.0f;
    for (int i = 0; i < det->num_features; i++) {
        float32_t s = eif_ts_detector_update(&det->ts_det[i], sample[i]);
        if (s > ts_score) ts_score = s;
    }
    
    // Weighted fusion
    float32_t score = det->weights[0] * stat_score +
                      det->weights[1] * mv_score +
                      det->weights[2] * ts_score;
    
    return score;
}

bool eif_ensemble_is_anomaly(eif_ensemble_detector_t* det,
                              const float32_t* sample) {
    return eif_ensemble_score(det, sample) > det->threshold;
}

eif_status_t eif_ensemble_scores(eif_ensemble_detector_t* det,
                                  const float32_t* sample,
                                  float32_t* stat_score,
                                  float32_t* mv_score,
                                  float32_t* ts_score) {
    if (!det || !sample) return EIF_STATUS_INVALID_ARGUMENT;
    
    // Statistical score
    if (stat_score) {
        *stat_score = 0.0f;
        for (int i = 0; i < det->num_features; i++) {
            float32_t stddev = sqrtf(det->stat_det[i].variance + 1e-10f);
            float32_t z = absf(sample[i] - det->stat_det[i].mean) / stddev;
            float32_t s = z / (det->stat_det[i].z_threshold * 1.5f);
            if (s > 1.0f) s = 1.0f;
            if (s > *stat_score) *stat_score = s;
        }
    }
    
    // Multivariate score
    if (mv_score) {
        *mv_score = eif_mv_detector_score(&det->mv_det, sample);
    }
    
    // Time series score
    if (ts_score) {
        *ts_score = 0.0f;
        for (int i = 0; i < det->num_features; i++) {
            float32_t residual = sample[i] - det->ts_det[i].ewma;
            float32_t norm = absf(residual) / (sqrtf(det->ts_det[i].ewma_var) + 1e-10f);
            float32_t s = norm / det->ts_det[i].threshold;
            if (s > 1.0f) s = 1.0f;
            if (s > *ts_score) *ts_score = s;
        }
    }
    
    return EIF_STATUS_OK;
}
