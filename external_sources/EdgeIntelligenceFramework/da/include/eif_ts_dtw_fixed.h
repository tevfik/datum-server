/**
 * @file eif_ts_dtw_fixed.h
 * @brief Dynamic Time Warping (Fixed Point Q15)
 */

#ifndef EIF_TS_DTW_FIXED_H
#define EIF_TS_DTW_FIXED_H

#include "eif_fixedpoint.h"
#include "eif_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute DTW distance between two Q15 sequences
 * 
 * Uses Q31 for cost accumulation to prevent overflow.
 * Returns the final accumulated distance.
 * 
 * @param[in] s1        First sequence (Q15)
 * @param[in] len1      Length of first sequence
 * @param[in] s2        Second sequence (Q15)
 * @param[in] len2      Length of second sequence
 * @param[in] window    Sakoe-Chiba window size (0 for no window)
 * @return q31_t        DTW Distance (Q15 scale, but accumulated in 32-bit)
 *                      Returns EIF_Q31_MAX on error.
 */
q31_t eif_ts_dtw_compute_fixed(const q15_t *s1, int len1, 
                               const q15_t *s2, int len2, 
                               int window);

#ifdef __cplusplus
}
#endif

#endif // EIF_TS_DTW_FIXED_H
