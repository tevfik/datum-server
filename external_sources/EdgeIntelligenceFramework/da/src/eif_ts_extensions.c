/**
 * @file eif_ts_extensions.c
 * @brief Time Series Extensions Implementation
 * 
 * Implements:
 * - Moving Average & Exponential Smoothing
 * - Seasonal Decomposition (STL-like)
 * - Change Point Detection (CUSUM)
 * - Autocorrelation (ACF/PACF)
 * - Correlation Analysis (Pearson, Spearman, Cross)
 */

#include "eif_ts.h"
#include <string.h>
#include <math.h>

// ============================================================================
// Moving Average
// ============================================================================

eif_status_t eif_ts_moving_average(const float32_t* data, int length, int window, float32_t* output) {
    if (!data || !output || length <= 0 || window <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    for (int i = 0; i < length; i++) {
        float32_t sum = 0;
        int count = 0;
        int start = i - window / 2;
        int end = i + window / 2;
        
        for (int j = start; j <= end; j++) {
            if (j >= 0 && j < length) {
                sum += data[j];
                count++;
            }
        }
        
        output[i] = count > 0 ? sum / (float32_t)count : data[i];
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Exponential Smoothing
// ============================================================================

eif_status_t eif_ts_exponential_smoothing(const float32_t* data, int length, float32_t alpha, float32_t* output) {
    if (!data || !output || length <= 0 || alpha < 0 || alpha > 1) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    output[0] = data[0];
    for (int i = 1; i < length; i++) {
        output[i] = alpha * data[i] + (1.0f - alpha) * output[i - 1];
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Seasonal Decomposition (Simplified STL)
// ============================================================================

eif_status_t eif_ts_decompose(const float32_t* data, int length, int season_length,
                               eif_ts_decomposition_t* result, eif_memory_pool_t* pool) {
    if (!data || !result || !pool || length <= 0 || season_length <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    result->length = length;
    result->season_length = season_length;
    
    result->trend = eif_memory_alloc(pool, length * sizeof(float32_t), 4);
    result->seasonal = eif_memory_alloc(pool, length * sizeof(float32_t), 4);
    result->residual = eif_memory_alloc(pool, length * sizeof(float32_t), 4);
    
    if (!result->trend || !result->seasonal || !result->residual) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Step 1: Extract trend using moving average
    eif_ts_moving_average(data, length, season_length, result->trend);
    
    // Step 2: Calculate detrended data and average seasonal component
    float32_t* seasonal_avg = eif_memory_alloc(pool, season_length * sizeof(float32_t), 4);
    int* counts = eif_memory_alloc(pool, season_length * sizeof(int), 4);
    memset(seasonal_avg, 0, season_length * sizeof(float32_t));
    memset(counts, 0, season_length * sizeof(int));
    
    for (int i = 0; i < length; i++) {
        float32_t detrended = data[i] - result->trend[i];
        int season_idx = i % season_length;
        seasonal_avg[season_idx] += detrended;
        counts[season_idx]++;
    }
    
    for (int s = 0; s < season_length; s++) {
        if (counts[s] > 0) {
            seasonal_avg[s] /= (float32_t)counts[s];
        }
    }
    
    // Step 3: Apply seasonal component
    for (int i = 0; i < length; i++) {
        result->seasonal[i] = seasonal_avg[i % season_length];
    }
    
    // Step 4: Calculate residual
    for (int i = 0; i < length; i++) {
        result->residual[i] = data[i] - result->trend[i] - result->seasonal[i];
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Change Point Detection (CUSUM)
// ============================================================================

eif_status_t eif_changepoint_init(eif_changepoint_t* detector, float32_t threshold, float32_t drift) {
    if (!detector) return EIF_STATUS_INVALID_ARGUMENT;
    
    detector->threshold = threshold > 0 ? threshold : 5.0f;
    detector->drift = drift >= 0 ? drift : 0.0f;
    detector->mean = 0.0f;
    detector->sum_pos = 0.0f;
    detector->sum_neg = 0.0f;
    detector->count = 0;
    detector->last_change_point = -1;
    
    return EIF_STATUS_OK;
}

int eif_changepoint_update(eif_changepoint_t* detector, float32_t value) {
    if (!detector) return 0;
    
    detector->count++;
    
    float32_t delta = value - detector->mean;
    detector->mean += delta / (float32_t)detector->count;
    
    float32_t deviation = value - detector->mean;
    
    detector->sum_pos = fmaxf(0.0f, detector->sum_pos + deviation - detector->drift);
    detector->sum_neg = fmaxf(0.0f, detector->sum_neg - deviation - detector->drift);
    
    if (detector->sum_pos > detector->threshold || detector->sum_neg > detector->threshold) {
        detector->sum_pos = 0.0f;
        detector->sum_neg = 0.0f;
        detector->last_change_point = detector->count - 1;
        return 1;
    }
    
    return 0;
}

eif_status_t eif_changepoint_detect(const float32_t* data, int length, float32_t threshold,
                                     int* change_points, int* num_changes, int max_changes) {
    if (!data || !change_points || !num_changes || length <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_changepoint_t detector;
    eif_changepoint_init(&detector, threshold, 0.0f);
    
    *num_changes = 0;
    
    for (int i = 0; i < length && *num_changes < max_changes; i++) {
        if (eif_changepoint_update(&detector, data[i])) {
            change_points[(*num_changes)++] = i;
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Autocorrelation Function
// ============================================================================

eif_status_t eif_ts_acf(const float32_t* data, int length, float32_t* acf, int max_lag) {
    if (!data || !acf || length <= 0 || max_lag <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    float32_t mean = 0.0f;
    for (int i = 0; i < length; i++) mean += data[i];
    mean /= (float32_t)length;
    
    float32_t var = 0.0f;
    for (int i = 0; i < length; i++) {
        float32_t diff = data[i] - mean;
        var += diff * diff;
    }
    
    if (var < 1e-10f) {
        for (int k = 0; k <= max_lag; k++) acf[k] = 0.0f;
        acf[0] = 1.0f;
        return EIF_STATUS_OK;
    }
    
    for (int k = 0; k <= max_lag && k < length; k++) {
        float32_t cov = 0.0f;
        for (int i = 0; i < length - k; i++) {
            cov += (data[i] - mean) * (data[i + k] - mean);
        }
        acf[k] = cov / var;
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Partial Autocorrelation (Levinson-Durbin)
// ============================================================================

eif_status_t eif_ts_pacf(const float32_t* data, int length, float32_t* pacf, int max_lag) {
    if (!data || !pacf || length <= 0 || max_lag <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    float32_t acf[64];
    int lag = max_lag < 63 ? max_lag : 63;
    eif_ts_acf(data, length, acf, lag);
    
    float32_t phi[64][64];
    
    pacf[0] = 1.0f;
    if (lag > 0) pacf[1] = acf[1];
    phi[1][1] = acf[1];
    
    for (int k = 2; k <= lag; k++) {
        float32_t num = acf[k];
        float32_t den = 1.0f;
        
        for (int j = 1; j < k; j++) {
            num -= phi[k-1][j] * acf[k - j];
            den -= phi[k-1][j] * acf[j];
        }
        
        phi[k][k] = (fabsf(den) > 1e-10f) ? num / den : 0.0f;
        pacf[k] = phi[k][k];
        
        for (int j = 1; j < k; j++) {
            phi[k][j] = phi[k-1][j] - phi[k][k] * phi[k-1][k-j];
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Correlation Analysis
// ============================================================================

float32_t eif_correlation_pearson(const float32_t* x, const float32_t* y, int length) {
    if (!x || !y || length <= 0) return 0.0f;
    
    float32_t mean_x = 0.0f, mean_y = 0.0f;
    for (int i = 0; i < length; i++) {
        mean_x += x[i];
        mean_y += y[i];
    }
    mean_x /= (float32_t)length;
    mean_y /= (float32_t)length;
    
    float32_t cov = 0.0f, var_x = 0.0f, var_y = 0.0f;
    for (int i = 0; i < length; i++) {
        float32_t dx = x[i] - mean_x;
        float32_t dy = y[i] - mean_y;
        cov += dx * dy;
        var_x += dx * dx;
        var_y += dy * dy;
    }
    
    float32_t denom = sqrtf(var_x * var_y);
    return (denom > 1e-10f) ? cov / denom : 0.0f;
}

float32_t eif_correlation_spearman(const float32_t* x, const float32_t* y, int length, eif_memory_pool_t* pool) {
    if (!x || !y || !pool || length <= 0) return 0.0f;
    
    float32_t* rank_x = eif_memory_alloc(pool, length * sizeof(float32_t), 4);
    float32_t* rank_y = eif_memory_alloc(pool, length * sizeof(float32_t), 4);
    int* idx = eif_memory_alloc(pool, length * sizeof(int), 4);
    
    // Rank x
    for (int i = 0; i < length; i++) idx[i] = i;
    for (int i = 0; i < length - 1; i++) {
        for (int j = 0; j < length - i - 1; j++) {
            if (x[idx[j]] > x[idx[j+1]]) {
                int tmp = idx[j]; idx[j] = idx[j+1]; idx[j+1] = tmp;
            }
        }
    }
    for (int i = 0; i < length; i++) rank_x[idx[i]] = (float32_t)(i + 1);
    
    // Rank y
    for (int i = 0; i < length; i++) idx[i] = i;
    for (int i = 0; i < length - 1; i++) {
        for (int j = 0; j < length - i - 1; j++) {
            if (y[idx[j]] > y[idx[j+1]]) {
                int tmp = idx[j]; idx[j] = idx[j+1]; idx[j+1] = tmp;
            }
        }
    }
    for (int i = 0; i < length; i++) rank_y[idx[i]] = (float32_t)(i + 1);
    
    return eif_correlation_pearson(rank_x, rank_y, length);
}

eif_status_t eif_cross_correlation(const float32_t* x, const float32_t* y, int length,
                                    float32_t* result, int max_lag) {
    if (!x || !y || !result || length <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    float32_t mean_x = 0.0f, mean_y = 0.0f;
    for (int i = 0; i < length; i++) {
        mean_x += x[i];
        mean_y += y[i];
    }
    mean_x /= (float32_t)length;
    mean_y /= (float32_t)length;
    
    float32_t std_x = 0.0f, std_y = 0.0f;
    for (int i = 0; i < length; i++) {
        std_x += (x[i] - mean_x) * (x[i] - mean_x);
        std_y += (y[i] - mean_y) * (y[i] - mean_y);
    }
    std_x = sqrtf(std_x / (float32_t)length);
    std_y = sqrtf(std_y / (float32_t)length);
    
    float32_t norm = (float32_t)length * std_x * std_y;
    if (norm < 1e-10f) norm = 1.0f;
    
    for (int lag = -max_lag; lag <= max_lag; lag++) {
        float32_t cc = 0.0f;
        
        for (int i = 0; i < length; i++) {
            int j = i + lag;
            if (j >= 0 && j < length) {
                cc += (x[i] - mean_x) * (y[j] - mean_y);
            }
        }
        
        result[lag + max_lag] = cc / norm;
    }
    
    return EIF_STATUS_OK;
}
