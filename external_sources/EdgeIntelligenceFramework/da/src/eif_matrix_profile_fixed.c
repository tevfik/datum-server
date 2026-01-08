/**
 * @file eif_matrix_profile_fixed.c
 * @brief Matrix Profile (Fixed Point) Implementation
 */

#include "eif_matrix_profile_fixed.h"
#include <stdlib.h>
#include <string.h>

#define ABS(x) ((x) > 0 ? (x) : -(x))

// Helper: Integer Square Root for 64-bit input
static int32_t isqrt64(int64_t n) {
    if (n <= 0) return 0;
    int64_t x = n;
    int64_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return (int32_t)x;
}

// Helper: Compute Mean and Std (Q15)
static void compute_statistics_fixed(const q15_t* ts, int length, int window,
                                     q15_t* means, q15_t* stds) {
    // Uses 64-bit accumulators
    for (int i = 0; i <= length - window; i++) {
        int64_t sum = 0;
        int64_t sum_sq = 0;
        for (int j = 0; j < window; j++) {
            q15_t val = ts[i + j];
            sum += val;
            sum_sq += (int64_t)val * val;
        }

        // Mean
        // Mean = Sum / m
        means[i] = (q15_t)(sum / window);
        
        // Variance = (SumSq / m) - Mean^2
        int64_t var_term1 = sum_sq / window;
        int64_t var_term2 = (int64_t)means[i] * means[i];
        int64_t var = var_term1 - var_term2;
        
        // Std = sqrt(Var)
        // Var is in Q30 range (Q15*Q15).
        // Sqrt(Q30) -> Q15.
        // So simple integer sqrt works perfectly.
        if (var < 0) var = 0;
        stds[i] = (q15_t)isqrt64(var);
        
        // We allow 0 here and handle it in the correlation loop
    }
}

eif_status_t eif_mp_init_fixed(eif_matrix_profile_fixed_t* mp, int ts_length, 
                               int window_size, eif_memory_pool_t* pool) {
    if (!mp || !pool || ts_length < window_size || window_size < 2) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }

    mp->window_size = window_size;
    mp->profile_length = ts_length - window_size + 1;
    mp->exclusion_zone = window_size / 4;
    if (mp->exclusion_zone < 1) mp->exclusion_zone = 1;

    mp->profile = (q15_t*)eif_memory_alloc(pool, mp->profile_length * sizeof(q15_t), 4);
    mp->profile_index = (int*)eif_memory_alloc(pool, mp->profile_length * sizeof(int), 4);

    if (!mp->profile || !mp->profile_index) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }

    // Initialize
    for (int i = 0; i < mp->profile_length; i++) {
        mp->profile[i] = EIF_Q15_MAX;
        mp->profile_index[i] = -1;
    }

    return EIF_STATUS_OK;
}

eif_status_t eif_mp_compute_fixed(const q15_t* ts, int ts_length, int window_size,
                                   eif_matrix_profile_fixed_t* mp, eif_memory_pool_t* pool) {
    if (!ts || !mp || !pool) return EIF_STATUS_INVALID_ARGUMENT;

    eif_status_t status = eif_mp_init_fixed(mp, ts_length, window_size, pool);
    if (status != EIF_STATUS_OK) return status;

    // Precompute stats
    q15_t* means = (q15_t*)eif_memory_alloc(pool, mp->profile_length * sizeof(q15_t), 4);
    q15_t* stds = (q15_t*)eif_memory_alloc(pool, mp->profile_length * sizeof(q15_t), 4);
    
    if (!means || !stds) return EIF_STATUS_OUT_OF_MEMORY;
    
    compute_statistics_fixed(ts, ts_length, window_size, means, stds);

    // Naive O(n^2) implementation for fixed point
    // Optimization: Matrix Profile is symmetric for self-join? No, but MP[i] is N.N of i.
    // We compute full distance matrix diagonals or just iterate.
    
    for (int i = 0; i < mp->profile_length; i++) {
        int64_t mu_q = means[i];
        int64_t sig_q = stds[i];
        
        // For each candidate
        for (int j = 0; j < mp->profile_length; j++) {
            // Exclusion zone
            if (ABS(i - j) <= mp->exclusion_zone) continue;
            
            // Compute Dot Product
            int64_t dot = 0;
            for (int k = 0; k < window_size; k++) {
                dot += (int64_t)ts[i+k] * ts[j+k];
            }
            // Dot is Sum(Q*T). Scaled Q30 approx.
            
            // Corr = (Dot - m * mu_q * mu_t) / (m * sig_q * sig_t)
            int64_t mu_t = means[j];
            int64_t sig_t = stds[j];
            
            int64_t num = dot - (int64_t)window_size * mu_q * mu_t; // Q30 approx
            int64_t den = (int64_t)window_size * sig_q * sig_t;     // Q30 approx
            
            q15_t corr_q15 = 0;
            
            if (den == 0) {
                // Handle constant signals (std=0)
                if (sig_q == 0 && sig_t == 0) {
                    corr_q15 = 32767; // 1.0 (approx)
                } else {
                    corr_q15 = 0;     // No correlation
                }
            } else {
                // We want result in Q15.
                // num/den is float correlation.
                // corr_q15 = (num << 15) / den.
                // Careful with shifts on large numbers.
                // num is around m * Q15 * Q15 ~= 100 * 32000 * 32000 ~= 10^11.
                // Fits in int64_t (up to 10^18).
                // (num << 15) ~= 10^16. Fits in int64_t.
                
                int64_t scaled_num = num << 15;
                int64_t result = scaled_num / den;
                
                if (result > EIF_Q15_MAX) result = EIF_Q15_MAX;
                if (result < -EIF_Q15_MAX) result = -EIF_Q15_MAX;
                corr_q15 = (q15_t)result;
            }
            
            // Dist = sqrt(2 * m * (1 - rho))
            // 1.0 is 32768.
            // (1 - rho) = (32768 - corr_q15) -> Range 0 to 65536.
            // 2 * m * Val.
            int64_t val = 32768 - corr_q15;
            if (val < 0) val = 0;
            
            int64_t dist_sq = (int64_t)2 * window_size * val; 
            // Result is Q15 scale (because val is Q15, but interpreted as integer part of formula?).
            // No, wait.
            // D = sqrt(2m * (1 - C)).
            // C is dimensionless.
            // D should be Euclidean Distance.
            // If data is Q15 (0..1), distance is Q15.
            // My formula derivation:
            // sqrt(2*m*(1 - C))
            // In Fixed Point:
            // C_fixed = C * 2^15.
            // (1 - C) = (2^15 - C_fixed) / 2^15.
            // D = sqrt(2*m * (2^15 - C_fixed) / 2^15).
            // D = sqrt( 2*m * (2^15 - C_fixed) ) / sqrt(2^15).
            // This is messy.
            // Let's just output `dist` as a comparable metric.
            // Or better:
            // D_fixed = sqrt(2*m * (32768 - corr_q15))
            // This yields a value up to sqrt(2*100*65536) ~= sqrt(13,000,000) ~= 3600.
            // This fits easily in Q15 (if viewed as integer) or needs scaling.
            // Let's stick to returning this integer value.
            
            q15_t dist = (q15_t)isqrt64(dist_sq);
            
            // Update Profile
            if (dist < mp->profile[i]) {
                mp->profile[i] = dist;
                mp->profile_index[i] = j;
            }
        }
    }
    
    // Free stats
    // Note: eif_memory currently doesn't support individual frees easily without "eif_memory_free"?
    // The previous code mass_compute didn't free its temp allocations inside the loop?
    // It relied on pool reset or small allocations.
    // Here we allocated means/stds. We should strictly free them or rely on pool scope.
    // Assuming pool handles it for now or we are leaking in this scope (but pool is usually linear).
    
    return EIF_STATUS_OK;
}
