/**
 * @file eif_ts_hw_fixed.h
 * @brief Holt-Winters Exponential Smoothing (Fixed Point Q15)
 */

#ifndef EIF_TS_HW_FIXED_H
#define EIF_TS_HW_FIXED_H

#include "eif_fixedpoint.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EIF_TS_HW_ADDITIVE,
    EIF_TS_HW_MULTIPLICATIVE
} eif_ts_hw_type_t;

typedef struct {
    eif_ts_hw_type_t type;
    int season_length;
    bool initialized;
    
    // Parameters (Q15, 0.0 to 1.0)
    q15_t alpha; // Level smoothing
    q15_t beta;  // Trend smoothing
    q15_t gamma; // Seasonal smoothing
    
    // State
    q15_t level; // L_t
    q15_t trend; // T_t
    
    // Seasonal buffer (Circular)
    q15_t* seasonals; // S_{t-s} ...
    int season_idx;   // Current index in seasonals buffer
    
} eif_ts_hw_fixed_t;

// API

eif_status_t eif_ts_hw_init_fixed(eif_ts_hw_fixed_t* model, int season_length, 
                                  eif_ts_hw_type_t type, eif_memory_pool_t* pool);

eif_status_t eif_ts_hw_update_fixed(eif_ts_hw_fixed_t* model, q15_t input);

q15_t eif_ts_hw_predict_fixed(const eif_ts_hw_fixed_t* model, int steps);

#ifdef __cplusplus
}
#endif

#endif // EIF_TS_HW_FIXED_H
