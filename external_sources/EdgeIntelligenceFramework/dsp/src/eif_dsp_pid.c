/**
 * @file eif_dsp_pid.c
 * @brief PID Controller Implementation
 */

#include "eif_dsp_pid.h"
#include <string.h>

// Helper clamp
static inline float32_t clampf(float32_t v, float32_t min_v, float32_t max_v) {
  if (v < min_v)
    return min_v;
  if (v > max_v)
    return max_v;
  return v;
}

void eif_dsp_pid_init(eif_dsp_pid_t *pid, float32_t Kp, float32_t Ki,
                      float32_t Kd, float32_t out_min, float32_t out_max) {
  if (!pid)
    return;
  pid->Kp = Kp;
  pid->Ki = Ki;
  pid->Kd = Kd;
  pid->out_min = out_min;
  pid->out_max = out_max;
  // Set decent integrator limits (e.g., same as output limits)
  pid->int_min = out_min;
  pid->int_max = out_max;

  eif_dsp_pid_reset(pid);
}

void eif_dsp_pid_reset(eif_dsp_pid_t *pid) {
  if (!pid)
    return;
  pid->prev_error = 0.0f;
  pid->integrator = 0.0f;
}

float32_t eif_dsp_pid_update(eif_dsp_pid_t *pid, float32_t error,
                             float32_t dt) {
  if (!pid)
    return 0.0f;

  // P term
  float32_t p = pid->Kp * error;

  // I term
  pid->integrator += error * dt;
  pid->integrator = clampf(pid->integrator, pid->int_min, pid->int_max);
  float32_t i = pid->Ki * pid->integrator;

  // D term
  float32_t derivative = 0.0f;
  if (dt > 1e-6f) {
    derivative = (error - pid->prev_error) / dt;
  }
  float32_t d = pid->Kd * derivative;

  pid->prev_error = error;

  // Output
  float32_t output = p + i + d;
  return clampf(output, pid->out_min, pid->out_max);
}
