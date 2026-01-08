/**
 * @file eif_dsp_pid.h
 * @brief PID Controller for Edge Intelligence
 */

#ifndef EIF_DSP_PID_H
#define EIF_DSP_PID_H

#include "eif_status.h"
#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Gains
  float32_t Kp;
  float32_t Ki;
  float32_t Kd;

  // Memory
  float32_t prev_error;
  float32_t integrator;

  // Limits
  float32_t out_min;
  float32_t out_max;
  float32_t int_min; // Anti-windup
  float32_t int_max;
} eif_dsp_pid_t;

/**
 * @brief Initialize PID Controller
 */
void eif_dsp_pid_init(eif_dsp_pid_t *pid, float32_t Kp, float32_t Ki,
                      float32_t Kd, float32_t out_min, float32_t out_max);

/**
 * @brief Reset PID state
 */
void eif_dsp_pid_reset(eif_dsp_pid_t *pid);

/**
 * @brief Compute PID output
 * @param error Setpoint - Measured
 * @param dt Delta time since last update (seconds)
 */
float32_t eif_dsp_pid_update(eif_dsp_pid_t *pid, float32_t error, float32_t dt);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_PID_H
