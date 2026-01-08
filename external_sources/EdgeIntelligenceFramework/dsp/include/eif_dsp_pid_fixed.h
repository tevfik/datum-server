/**
 * @file eif_dsp_pid_fixed.h
 * @brief Fixed-Point (Q15) PID Controller
 */

#ifndef EIF_DSP_PID_FIXED_H
#define EIF_DSP_PID_FIXED_H

#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Q15 PID Controller State
 */
typedef struct {
  // Coefficients (Q15)
  q15_t Kp;
  q15_t Ki;
  q15_t Kd;

  // Limits (Q15)
  q15_t out_min;
  q15_t out_max;
  // Integrator limits (Q31 to prevent early wrapping, processed as Q15)
  q31_t int_min;
  q31_t int_max;

  // State
  q15_t prev_error;
  q31_t integrator; // Keep integrator in high precision (Q31)
} eif_pid_q15_t;

/**
 * @brief Initialize Q15 PID
 */
void eif_pid_q15_init(eif_pid_q15_t *pid, q15_t Kp, q15_t Ki, q15_t Kd,
                      q15_t out_min, q15_t out_max);

/**
 * @brief Reset PID state
 */
void eif_pid_q15_reset(eif_pid_q15_t *pid);

/**
 * @brief Update PID Controller
 * @param error Error input (Q15)
 * @return Control output (Q15)
 *
 * Note: dt is assumed to be absorbed into coefficients or constant.
 * If dt is variable, coefficients should be scaled externally.
 */
q15_t eif_pid_q15_update(eif_pid_q15_t *pid, q15_t error);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_PID_FIXED_H
