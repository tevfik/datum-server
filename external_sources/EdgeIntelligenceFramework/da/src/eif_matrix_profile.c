/**
 * @file eif_matrix_profile.c
 * @brief Matrix Profile Implementation
 * 
 * Implements the Matrix Profile algorithm using MASS (Mueen's Algorithm
 * for Similarity Search) for FFT-accelerated distance computation.
 */

#include "eif_matrix_profile.h"
#include "eif_dsp.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Compute running mean and standard deviation
 */
static void compute_statistics(const float32_t* ts, int ts_length, int window_size,
                                float32_t* means, float32_t* stds) {
    int profile_length = ts_length - window_size + 1;
    
    // Compute first window mean and variance
    float32_t sum = 0.0f;
    float32_t sum_sq = 0.0f;
    
    for (int i = 0; i < window_size; i++) {
        sum += ts[i];
        sum_sq += ts[i] * ts[i];
    }
    
    means[0] = sum / window_size;
    float32_t var = (sum_sq / window_size) - (means[0] * means[0]);
    stds[0] = var > 0 ? sqrtf(var) : 1e-10f;
    
    // Sliding window for rest
    for (int i = 1; i < profile_length; i++) {
        // Update sum: add new, remove old
        sum = sum - ts[i - 1] + ts[i + window_size - 1];
        sum_sq = sum_sq - ts[i - 1] * ts[i - 1] + ts[i + window_size - 1] * ts[i + window_size - 1];
        
        means[i] = sum / window_size;
        var = (sum_sq / window_size) - (means[i] * means[i]);
        stds[i] = var > 0 ? sqrtf(var) : 1e-10f;
    }
}

/**
 * @brief Compute sliding dot product directly
 * QT[i] = sum(Q * T[i:i+m]) for all i
 * 
 * Note: For large datasets, an FFT-based implementation would be O(n log n).
 * This direct implementation is O(n*m) but more reliable for embedded use.
 */
static eif_status_t sliding_dot_product(const float32_t* query, int query_len,
                                         const float32_t* ts, int ts_len,
                                         float32_t* dot_product) {
    int output_len = ts_len - query_len + 1;
    
    for (int i = 0; i < output_len; i++) {
        float32_t sum = 0.0f;
        for (int j = 0; j < query_len; j++) {
            sum += query[j] * ts[i + j];
        }
        dot_product[i] = sum;
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// MASS Algorithm
// ============================================================================

eif_status_t eif_mass_compute(const float32_t* ts, int ts_length,
                               const float32_t* query, int query_length,
                               float32_t* distance_profile,
                               eif_memory_pool_t* pool) {
    if (!ts || !query || !distance_profile || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    if (ts_length < query_length || query_length < 2) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    int profile_length = ts_length - query_length + 1;
    
    // Compute query mean and std
    float32_t q_mean = 0.0f;
    float32_t q_std = 0.0f;
    for (int i = 0; i < query_length; i++) {
        q_mean += query[i];
    }
    q_mean /= query_length;
    
    for (int i = 0; i < query_length; i++) {
        float32_t diff = query[i] - q_mean;
        q_std += diff * diff;
    }
    q_std = sqrtf(q_std / query_length);
    if (q_std < 1e-10f) q_std = 1e-10f;
    
    // Compute means and stds for all subsequences
    float32_t* ts_means = eif_memory_alloc(pool, profile_length * sizeof(float32_t), 4);
    float32_t* ts_stds = eif_memory_alloc(pool, profile_length * sizeof(float32_t), 4);
    float32_t* dot_product = eif_memory_alloc(pool, profile_length * sizeof(float32_t), 4);
    
    if (!ts_means || !ts_stds || !dot_product) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    compute_statistics(ts, ts_length, query_length, ts_means, ts_stds);
    
    // Compute sliding dot product directly
    sliding_dot_product(query, query_length, ts, ts_length, dot_product);
    
    // Convert to z-normalized Euclidean distance
    // dist = sqrt(2 * m * (1 - (QT - m * mu_q * mu_t) / (m * sigma_q * sigma_t)))
    for (int i = 0; i < profile_length; i++) {
        float32_t pearson = (dot_product[i] - query_length * q_mean * ts_means[i]) / 
                            (query_length * q_std * ts_stds[i]);
        
        // Clamp pearson to [-1, 1] for numerical stability
        if (pearson > 1.0f) pearson = 1.0f;
        if (pearson < -1.0f) pearson = -1.0f;
        
        distance_profile[i] = sqrtf(2.0f * query_length * (1.0f - pearson));
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Matrix Profile Computation
// ============================================================================

eif_status_t eif_mp_init(eif_matrix_profile_t* mp, int ts_length, 
                          int window_size, eif_memory_pool_t* pool) {
    if (!mp || !pool || ts_length < window_size || window_size < 2) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    mp->window_size = window_size;
    mp->profile_length = ts_length - window_size + 1;
    mp->exclusion_zone = window_size / 4;
    if (mp->exclusion_zone < 1) mp->exclusion_zone = 1;
    
    mp->profile = eif_memory_alloc(pool, mp->profile_length * sizeof(float32_t), 4);
    mp->profile_index = eif_memory_alloc(pool, mp->profile_length * sizeof(int), 4);
    
    if (!mp->profile || !mp->profile_index) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Initialize to infinity / -1
    for (int i = 0; i < mp->profile_length; i++) {
        mp->profile[i] = FLT_MAX;
        mp->profile_index[i] = -1;
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_mp_compute(const float32_t* ts, int ts_length, int window_size,
                             eif_matrix_profile_t* mp, eif_memory_pool_t* pool) {
    if (!ts || !mp || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_status_t status = eif_mp_init(mp, ts_length, window_size, pool);
    if (status != EIF_STATUS_OK) return status;
    
    float32_t* distance_profile = eif_memory_alloc(pool, mp->profile_length * sizeof(float32_t), 4);
    if (!distance_profile) return EIF_STATUS_OUT_OF_MEMORY;
    
    // For each subsequence, compute distance profile and update matrix profile
    for (int i = 0; i < mp->profile_length; i++) {
        // Compute distance profile for query at position i
        status = eif_mass_compute(ts, ts_length, &ts[i], window_size, distance_profile, pool);
        if (status != EIF_STATUS_OK) return status;
        
        // Update matrix profile with exclusion zone
        for (int j = 0; j < mp->profile_length; j++) {
            // Skip exclusion zone around diagonal
            if (abs(i - j) <= mp->exclusion_zone) continue;
            
            if (distance_profile[j] < mp->profile[i]) {
                mp->profile[i] = distance_profile[j];
                mp->profile_index[i] = j;
            }
            if (distance_profile[j] < mp->profile[j]) {
                mp->profile[j] = distance_profile[j];
                mp->profile_index[j] = i;
            }
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_mp_compute_ab(const float32_t* ts_a, int len_a,
                                const float32_t* ts_b, int len_b,
                                int window_size,
                                eif_matrix_profile_t* mp,
                                eif_memory_pool_t* pool) {
    if (!ts_a || !ts_b || !mp || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    if (len_a < window_size || len_b < window_size || window_size < 2) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Initialize MP for series A
    eif_status_t status = eif_mp_init(mp, len_a, window_size, pool);
    if (status != EIF_STATUS_OK) return status;
    
    int profile_b_length = len_b - window_size + 1;
    float32_t* distance_profile = eif_memory_alloc(pool, profile_b_length * sizeof(float32_t), 4);
    if (!distance_profile) return EIF_STATUS_OUT_OF_MEMORY;
    
    // For each subsequence in A, find nearest neighbor in B
    for (int i = 0; i < mp->profile_length; i++) {
        // Compute distance to all subsequences in B
        status = eif_mass_compute(ts_b, len_b, &ts_a[i], window_size, distance_profile, pool);
        if (status != EIF_STATUS_OK) return status;
        
        // Find minimum distance
        float32_t min_dist = FLT_MAX;
        int min_idx = -1;
        for (int j = 0; j < profile_b_length; j++) {
            if (distance_profile[j] < min_dist) {
                min_dist = distance_profile[j];
                min_idx = j;
            }
        }
        
        mp->profile[i] = min_dist;
        mp->profile_index[i] = min_idx;
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Motif and Discord Discovery
// ============================================================================

eif_status_t eif_mp_find_motifs(const eif_matrix_profile_t* mp, int top_k,
                                 int* motif_indices, float32_t* motif_distances, eif_memory_pool_t* pool) {
    if (!mp || !motif_indices || top_k <= 0 || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Simple selection: find top_k smallest values
    // Using a simple O(k*n) approach for embedded systems
    bool* used = (bool*)eif_memory_calloc(pool, mp->profile_length, sizeof(bool), sizeof(bool));
    if (!used) return EIF_STATUS_OUT_OF_MEMORY;
    
    for (int k = 0; k < top_k && k < mp->profile_length; k++) {
        float32_t min_val = FLT_MAX;
        int min_idx = -1;
        
        for (int i = 0; i < mp->profile_length; i++) {
            if (!used[i] && mp->profile[i] < min_val) {
                min_val = mp->profile[i];
                min_idx = i;
            }
        }
        
        if (min_idx >= 0) {
            motif_indices[k] = min_idx;
            if (motif_distances) motif_distances[k] = min_val;
            
            // Mark exclusion zone as used
            for (int i = min_idx - mp->exclusion_zone; i <= min_idx + mp->exclusion_zone; i++) {
                if (i >= 0 && i < mp->profile_length) used[i] = true;
            }
        }
    }
    
    return EIF_STATUS_OK;
}

eif_status_t eif_mp_find_discords(const eif_matrix_profile_t* mp, int top_k,
                                   int* discord_indices, float32_t* discord_distances, eif_memory_pool_t* pool) {
    if (!mp || !discord_indices || top_k <= 0 || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    bool* used = (bool*)eif_memory_calloc(pool, mp->profile_length, sizeof(bool), sizeof(bool));
    if (!used) return EIF_STATUS_OUT_OF_MEMORY;
    
    for (int k = 0; k < top_k && k < mp->profile_length; k++) {
        float32_t max_val = -FLT_MAX;
        int max_idx = -1;
        
        for (int i = 0; i < mp->profile_length; i++) {
            if (!used[i] && mp->profile[i] > max_val && mp->profile[i] < FLT_MAX) {
                max_val = mp->profile[i];
                max_idx = i;
            }
        }
        
        if (max_idx >= 0) {
            discord_indices[k] = max_idx;
            if (discord_distances) discord_distances[k] = max_val;
            
            for (int i = max_idx - mp->exclusion_zone; i <= max_idx + mp->exclusion_zone; i++) {
                if (i >= 0 && i < mp->profile_length) used[i] = true;
            }
        }
    }
    
    return EIF_STATUS_OK;
}

// ============================================================================
// Streaming Matrix Profile (STUMPI)
// ============================================================================

eif_status_t eif_mp_stream_init(eif_mp_stream_t* stream, int buffer_size,
                                 int window_size, eif_memory_pool_t* pool) {
    if (!stream || !pool || buffer_size < 2 * window_size || window_size < 2) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    stream->window_size = window_size;
    stream->buffer_size = buffer_size;
    stream->buffer_head = 0;
    stream->count = 0;
    
    // Allocate buffers
    stream->ts_buffer = eif_memory_alloc(pool, buffer_size * sizeof(float32_t), 4);
    stream->qT = eif_memory_alloc(pool, buffer_size * sizeof(float32_t), 4);
    stream->mean_buffer = eif_memory_alloc(pool, buffer_size * sizeof(float32_t), 4);
    stream->std_buffer = eif_memory_alloc(pool, buffer_size * sizeof(float32_t), 4);
    
    if (!stream->ts_buffer || !stream->qT || !stream->mean_buffer || !stream->std_buffer) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    // Initialize matrix profile
    eif_status_t status = eif_mp_init(&stream->mp, buffer_size, window_size, pool);
    if (status != EIF_STATUS_OK) return status;
    
    memset(stream->ts_buffer, 0, buffer_size * sizeof(float32_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_mp_stream_update(eif_mp_stream_t* stream, float32_t new_value,
                                   eif_memory_pool_t* pool) {
    if (!stream || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Add to circular buffer
    stream->ts_buffer[stream->buffer_head] = new_value;
    stream->buffer_head = (stream->buffer_head + 1) % stream->buffer_size;
    stream->count++;
    
    // Need at least 2*window_size samples to compute meaningful profile
    if (stream->count < 2 * stream->window_size) {
        return EIF_STATUS_OK;
    }
    
    // Recompute matrix profile on current buffer
    // For true O(1) update, need more complex STUMPI implementation
    // This is a simplified version that recomputes periodically
    if (stream->count % stream->window_size == 0) {
        int current_len = stream->count < stream->buffer_size ? 
                          stream->count : stream->buffer_size;
        
        // Linearize circular buffer
        float32_t* linear = eif_memory_alloc(pool, current_len * sizeof(float32_t), 4);
        if (!linear) return EIF_STATUS_OUT_OF_MEMORY;
        
        int start = (stream->buffer_head - current_len + stream->buffer_size) % stream->buffer_size;
        for (int i = 0; i < current_len; i++) {
            linear[i] = stream->ts_buffer[(start + i) % stream->buffer_size];
        }
        
        // Recompute profile
        eif_mp_compute(linear, current_len, stream->window_size, &stream->mp, pool);
    }
    
    return EIF_STATUS_OK;
}

const eif_matrix_profile_t* eif_mp_stream_get_profile(const eif_mp_stream_t* stream) {
    if (!stream) return NULL;
    return &stream->mp;
}
