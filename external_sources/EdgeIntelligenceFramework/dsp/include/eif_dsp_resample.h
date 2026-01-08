#ifndef EIF_DSP_RESAMPLE_H
#define EIF_DSP_RESAMPLE_H

#include "eif_types.h"
#include "eif_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Resampling
// ============================================================================

/**
 * @brief Resample configuration
 */
typedef struct {
    uint32_t in_rate;
    uint32_t out_rate;
    float32_t ratio; // in_rate / out_rate
} eif_resample_config_t;

/**
 * @brief Initialize resampler
 */
eif_status_t eif_resample_init(eif_resample_config_t* cfg, uint32_t in_rate, uint32_t out_rate);

/**
 * @brief Process resampling (Linear Interpolation)
 * 
 * @param cfg Configuration
 * @param input Input buffer
 * @param in_len Input length
 * @param output Output buffer
 * @param out_len Pointer to store output length
 * @param max_out_len Maximum capacity of output buffer
 * @return eif_status_t 
 */
eif_status_t eif_resample_process_linear(eif_resample_config_t* cfg, const float32_t* input, size_t in_len, 
                                         float32_t* output, size_t* out_len, size_t max_out_len);

/**
 * @brief Downsample by integer factor (Decimation)
 * Efficient for factors like 2, 4, 8. Includes simple averaging filter.
 */
eif_status_t eif_resample_decimate(const float32_t* input, size_t in_len, int factor, 
                                   float32_t* output, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_RESAMPLE_H
