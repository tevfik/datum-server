/**
 * @file eif_dsp_resample_fixed.h
 * @brief Fixed-Point Resampling
 */

#ifndef EIF_DSP_RESAMPLE_FIXED_H
#define EIF_DSP_RESAMPLE_FIXED_H

#include "eif_fixedpoint.h"
#include "eif_status.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

eif_status_t eif_resample_linear_q15(const q15_t *input, size_t in_len,
                                     q15_t *output, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_RESAMPLE_FIXED_H
