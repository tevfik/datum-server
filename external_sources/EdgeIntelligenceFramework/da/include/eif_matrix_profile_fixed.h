/**
 * @file eif_matrix_profile_fixed.h
 * @brief Matrix Profile (Fixed Point Q15)
 *
 * Fixed-point implementation of Matrix Profile.
 * Uses integer arithmetic for calculating correlation/distance.
 */

#ifndef EIF_MATRIX_PROFILE_FIXED_H
#define EIF_MATRIX_PROFILE_FIXED_H

#include "eif_fixedpoint.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Fixed-point Matrix Profile result
 */
typedef struct {
    q15_t* profile;              // [profile_length] - Distance (Q15)
    int* profile_index;          // [profile_length] - Index of nearest neighbor
    int profile_length;          // Length of the profile
    int window_size;             // Subsequence window size (m)
    int exclusion_zone;          // Exclusion zone size
} eif_matrix_profile_fixed_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Initialize matrix profile structure (Fixed Point)
 */
eif_status_t eif_mp_init_fixed(eif_matrix_profile_fixed_t* mp, int ts_length, 
                               int window_size, eif_memory_pool_t* pool);

/**
 * @brief Compute self-join matrix profile (Fixed Point)
 * 
 * Computes the Matrix Profile using fixed-point arithmetic.
 * Approximates Pearson Correlation for distance calculation.
 * 
 * @param ts            Time series data (Q15)
 * @param ts_length     Length of time series
 * @param window_size   Subsequence window size
 * @param mp            Output matrix profile (initialized)
 * @param pool          Memory pool
 * @return Status code
 */
eif_status_t eif_mp_compute_fixed(const q15_t* ts, int ts_length, int window_size,
                                   eif_matrix_profile_fixed_t* mp, eif_memory_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif // EIF_MATRIX_PROFILE_FIXED_H
