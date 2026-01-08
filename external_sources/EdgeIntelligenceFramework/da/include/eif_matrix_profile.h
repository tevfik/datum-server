/**
 * @file eif_matrix_profile.h
 * @brief Matrix Profile - Time Series Analysis
 * 
 * Implementation of the Matrix Profile algorithm using MASS (Mueen's Algorithm 
 * for Similarity Search) for FFT-accelerated distance computation.
 * 
 * Features:
 * - AB-join (compare two time series)
 * - Self-join (find patterns in single time series)
 * - Motif discovery (find repeating patterns)
 * - Discord detection (find anomalies)
 * - Streaming/incremental updates (STUMPI)
 * 
 * Reference: https://www.cs.ucr.edu/~eamonn/MatrixProfile.html
 */

#ifndef EIF_MATRIX_PROFILE_H
#define EIF_MATRIX_PROFILE_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

// ============================================================================
// Matrix Profile Data Structures
// ============================================================================

/**
 * @brief Matrix Profile result structure
 */
typedef struct {
    float32_t* profile;          // [profile_length] - Distance to nearest neighbor
    int* profile_index;          // [profile_length] - Index of nearest neighbor
    int profile_length;          // Length of the profile
    int window_size;             // Subsequence window size (m)
    int exclusion_zone;          // Exclusion zone size (typically m/4)
} eif_matrix_profile_t;

/**
 * @brief Streaming Matrix Profile state (STUMPI)
 */
typedef struct {
    eif_matrix_profile_t mp;     // Current matrix profile
    float32_t* ts_buffer;        // Circular buffer for time series
    float32_t* qT;               // Running QT product
    float32_t* mean_buffer;      // Running means
    float32_t* std_buffer;       // Running standard deviations
    int buffer_size;             // Size of circular buffer
    int buffer_head;             // Current head position
    int count;                   // Number of samples seen
    int window_size;             // Subsequence window size
} eif_mp_stream_t;

// ============================================================================
// MASS Algorithm (FFT-based distance computation)
// ============================================================================

/**
 * @brief Compute distance profile using MASS algorithm
 * 
 * Computes z-normalized Euclidean distance between a query subsequence
 * and all subsequences of a time series using FFT for O(n log n) complexity.
 * 
 * @param ts Time series data
 * @param ts_length Length of time series
 * @param query Query subsequence  
 * @param query_length Length of query (window size)
 * @param distance_profile Output distance profile [ts_length - query_length + 1]
 * @param pool Memory pool for temporary allocations
 * @return Status code
 */
eif_status_t eif_mass_compute(const float32_t* ts, int ts_length,
                               const float32_t* query, int query_length,
                               float32_t* distance_profile,
                               eif_memory_pool_t* pool);

// ============================================================================
// Matrix Profile Computation
// ============================================================================

/**
 * @brief Initialize matrix profile structure
 * 
 * @param mp Matrix profile structure to initialize
 * @param ts_length Length of the time series
 * @param window_size Subsequence window size
 * @param pool Memory pool for allocations
 * @return Status code
 */
eif_status_t eif_mp_init(eif_matrix_profile_t* mp, int ts_length, 
                          int window_size, eif_memory_pool_t* pool);

/**
 * @brief Compute self-join matrix profile
 * 
 * Finds the nearest neighbor for each subsequence within the same time series.
 * 
 * @param ts Time series data
 * @param ts_length Length of time series
 * @param window_size Subsequence window size
 * @param mp Output matrix profile (must be initialized)
 * @param pool Memory pool
 * @return Status code
 */
eif_status_t eif_mp_compute(const float32_t* ts, int ts_length, int window_size,
                             eif_matrix_profile_t* mp, eif_memory_pool_t* pool);

/**
 * @brief Compute AB-join matrix profile
 * 
 * Finds the nearest neighbor in time series B for each subsequence in A.
 * 
 * @param ts_a Time series A
 * @param len_a Length of time series A
 * @param ts_b Time series B
 * @param len_b Length of time series B
 * @param window_size Subsequence window size
 * @param mp Output matrix profile (profile_length = len_a - window_size + 1)
 * @param pool Memory pool
 * @return Status code
 */
eif_status_t eif_mp_compute_ab(const float32_t* ts_a, int len_a,
                                const float32_t* ts_b, int len_b,
                                int window_size,
                                eif_matrix_profile_t* mp,
                                eif_memory_pool_t* pool);

// ============================================================================
// Motif and Discord Discovery
// ============================================================================

/**
 * @brief Find top-k motifs (repeating patterns)
 * 
 * Motifs are subsequences with the smallest matrix profile values,
 * indicating they have a very similar match elsewhere in the time series.
 * 
 * @param mp Matrix profile
 * @param top_k Number of motifs to find
 * @param motif_indices Output array of motif indices [top_k]
 * @param motif_distances Output array of motif distances [top_k] (optional, can be NULL)
 * @return Status code
 */
eif_status_t eif_mp_find_motifs(const eif_matrix_profile_t* mp, int top_k,
                                 int* motif_indices, float32_t* motif_distances, eif_memory_pool_t* pool);

/**
 * @brief Find top-k discords (anomalies)
 * 
 * Discords are subsequences with the largest matrix profile values,
 * indicating they are most dissimilar from any other subsequence.
 * 
 * @param mp Matrix profile
 * @param top_k Number of discords to find
 * @param discord_indices Output array of discord indices [top_k]
 * @param discord_distances Output array of discord distances [top_k] (optional)
 * @return Status code
 */
eif_status_t eif_mp_find_discords(const eif_matrix_profile_t* mp, int top_k,
                                  int* discord_indices, float32_t* discord_distances, eif_memory_pool_t* pool);

// ============================================================================
// Streaming Matrix Profile (STUMPI)
// ============================================================================

/**
 * @brief Initialize streaming matrix profile
 * 
 * @param stream Streaming state to initialize
 * @param buffer_size Maximum buffer size (should be > 2 * window_size)
 * @param window_size Subsequence window size
 * @param pool Memory pool
 * @return Status code
 */
eif_status_t eif_mp_stream_init(eif_mp_stream_t* stream, int buffer_size,
                                 int window_size, eif_memory_pool_t* pool);

/**
 * @brief Update streaming matrix profile with new data point
 * 
 * @param stream Streaming state
 * @param new_value New time series value
 * @param pool Memory pool for temporary allocations
 * @return Status code
 */
eif_status_t eif_mp_stream_update(eif_mp_stream_t* stream, float32_t new_value,
                                   eif_memory_pool_t* pool);

/**
 * @brief Get current matrix profile from stream
 * 
 * @param stream Streaming state
 * @return Pointer to current matrix profile (read-only)
 */
const eif_matrix_profile_t* eif_mp_stream_get_profile(const eif_mp_stream_t* stream);

#endif // EIF_MATRIX_PROFILE_H
