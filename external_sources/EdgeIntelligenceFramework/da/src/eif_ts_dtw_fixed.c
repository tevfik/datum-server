/**
 * @file eif_ts_dtw_fixed.c
 * @brief Dynamic Time Warping (Fixed Point) Implementation
 */

#include "eif_ts_dtw_fixed.h"
#include <stdlib.h>
#include <string.h>

// Helper macros
#define ABS_DIFF(a, b) (abs((int32_t)(a) - (int32_t)(b))) // Result is Q15 (positive)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MIN3(a, b, c) MIN(MIN(a, b), c)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

q31_t eif_ts_dtw_compute_fixed(const q15_t *s1, int len1, 
                               const q15_t *s2, int len2, 
                               int window) {
    if (!s1 || !s2 || len1 <= 0 || len2 <= 0) {
        return EIF_Q31_MAX;
    }

    // Two-row approach to save memory
    // Cost is Q31 to allow accumulation
    int cols = len2 + 1;
    q31_t *prev = (q31_t*)malloc(cols * sizeof(q31_t));
    q31_t *curr = (q31_t*)malloc(cols * sizeof(q31_t));

    if (!prev || !curr) {
        if (prev) free(prev);
        if (curr) free(curr);
        return EIF_Q31_MAX;
    }

    // Initialize prev row
    for (int j = 0; j < cols; j++) {
        prev[j] = EIF_Q31_MAX;
    }
    prev[0] = 0;

    // Iterate
    for (int i = 1; i <= len1; i++) {
        // Initialize current row
        for(int k=0;k<cols;k++) curr[k] = EIF_Q31_MAX;
        
        // Window constraints
        int start = 1;
        int end = len2;
        if (window > 0) {
            start = MAX(1, i - window);
            end = MIN(len2, i + window);
        }

        // Fill cells before start (if any)
        for (int j = 1; j < start; j++) {
            curr[j] = EIF_Q31_MAX;
        }

        for (int j = start; j <= end; j++) {
            // Cost = |s1[i-1] - s2[j-1]|
            // Use q31_t for distance to avoid Q15 overflow (if diff > 32767)
            q31_t d = (q31_t)ABS_DIFF(s1[i-1], s2[j-1]);
            
            // Min of neighbors
            q31_t min_prev = MIN3(prev[j], prev[j-1], curr[j-1]);
            
            if (min_prev == EIF_Q31_MAX) {
                curr[j] = EIF_Q31_MAX;
            } else {
                // Check overflow? Q31 max is 2e9. 
                // d is max 32767. If len is < 65000, unlikely to overflow.
                curr[j] = min_prev + d;
            }
        }

        // Fill cells after end (if any)
        for (int j = end + 1; j <= len2; j++) {
            curr[j] = EIF_Q31_MAX;
        }

        // Swap rows
        q31_t *temp = prev;
        prev = curr;
        curr = temp;
    }

    q31_t result = prev[len2];

    free(prev);
    free(curr);

    return result;
}
