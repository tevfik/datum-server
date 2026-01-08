/**
 * @file eif_dsp_pid_fixed.c
 * @brief Fixed-Point PID Implementation
 */

#include "eif_dsp_pid_fixed.h"

void eif_pid_q15_init(eif_pid_q15_t *pid, q15_t Kp, q15_t Ki, q15_t Kd,
                      q15_t out_min, q15_t out_max) {
  if (!pid)
    return;
  pid->Kp = Kp;
  pid->Ki = Ki;
  pid->Kd = Kd;
  pid->out_min = out_min;
  pid->out_max = out_max;

  // Set integrator limits to reasonable defaults (e.g. output limits scaled up)
  // We use Q31 for integrator, so these limits map to Q31 range.
  // Q15 range is -32768 to 32767. Q31 is much larger.
  // We'll treat integrator as Q15 accumulating into Q31 (so it has headroom).
  pid->int_min = (q31_t)out_min << 16;
  pid->int_max = (q31_t)out_max << 16;

  eif_pid_q15_reset(pid);
}

void eif_pid_q15_reset(eif_pid_q15_t *pid) {
  if (!pid)
    return;
  pid->prev_error = 0;
  pid->integrator = 0;
}

q15_t eif_pid_q15_update(eif_pid_q15_t *pid, q15_t error) {
  if (!pid)
    return 0;

  // P term: Kp * error
  // Q15 * Q15 = Q30 -> shift >> 15 -> Q15
  q31_t p_term = ((q31_t)pid->Kp * error) >> 15;

  // I term: Integrator += Ki * error
  // We accumulate Ki*error directly into Q31 integrator
  q31_t i_inc = ((q31_t)pid->Ki *
                 error); // Q30 effectively if we consider Ki Q15 and error Q15?
  /**
   * Correction:
   * If integrator is Q31, and we want it to represent huge sum.
   * Let's say Ki is small.
   * Standard discrete integral: I += error * dt.
   * Here we match float logic: integrator += error * dt; i_term = Ki *
   * integrator.
   *
   * Let's restart logic to match float structure:
   * float: integrator += error; i = Ki * integrator;
   */

  // Integrate error directly?
  // If we accumulate just `error` (Q15), we might overflow quickly if we don't
  // use Q31. Let's accumulate error into Q31 integrator.
  pid->integrator += (q31_t)error;

  // Clamp integrator
  // Map bounds to "unscaled" domain?
  // Float impl: int_min/max are same as out_min/max.
  // So if out is -1..1, int is -1..1.
  // If integrator accumulates many errors, it hits limit.
  // In Q31, 1.0 is MAX_INT. But error is Q15 (small).
  // Let's use specific limits.
  // We will clamp integrator to generic Q31 max if not specified,
  // but here we used out_min<<16 which is huge.
  // Actually, simply accumulating `error` is fine.

  if (pid->integrator > pid->int_max)
    pid->integrator = pid->int_max;
  if (pid->integrator < pid->int_min)
    pid->integrator = pid->int_min;

  // I term output
  q31_t i_term = ((q31_t)pid->Ki * (pid->integrator >> 16)) >>
                 0; // Ki(Q15) * Int(High part Q15) -> Q30 -> Q15?
  // Better: (Ki * integrator) >> 15? but integrator is Q31.
  // Let's do: (Ki * (integrator >> 15))? Precision loss.
  // (Ki * integrator) -> Q46. Too big.
  // Let's stick to P-term style:
  // i_term = (Ki * (pid->integrator >> 5)) >> 10 ?
  // Simplest robust way:
  // i_term = (q31_t)(((q63_t)pid->Ki * pid->integrator) >> 31); // Scales
  // Q15*Q31 -> Q15 approx? Q15 (1.0) * Q31 (1.0) = Q46 (1.0). Result >> 31 =
  // Q15 (1.0). Yes.

  q63_t i_mult = (q63_t)pid->Ki * pid->integrator;
  q15_t i_out = (q15_t)(i_mult >> 31);
  // Note: This effectively treats Ki as "gain per huge time".
  // Wait, float code: integrator += error; i = Ki * integrator; (clamped).
  // If I use the logic from P-term:
  // P = Kp * error.
  // If I want I term to be commensurate, Ki should be similar scale.
  // If integrator is just sum of errors, it grows fast.
  // Let's do: integrator += (Ki * error).
  // Then i_term = integrator.
  // This is "velocity form" accumulation usage? No, "position form".
  // Doing `integrator += (Ki * error)` allows precise tracking.
  // Let's change strategy to that.

  // NEW STRATEGY: Accumulate scaled error.
  // integrator += (Ki * error) -> Q15*Q15 = Q30.
  // integrator is Q30.
  // i_out = integrator >> 15.

  pid->integrator += ((q31_t)pid->Ki * error);

  // Clamp Q30 integrator
  q31_t int_lim_max = (q31_t)pid->out_max << 15;
  q31_t int_lim_min = (q31_t)pid->out_min << 15;

  if (pid->integrator > int_lim_max)
    pid->integrator = int_lim_max;
  if (pid->integrator < int_lim_min)
    pid->integrator = int_lim_min;

  q15_t i_out_final = (q15_t)(pid->integrator >> 15);

  // D term: Kd * (error - prev)
  q15_t diff = error - pid->prev_error;
  q31_t d_term = ((q31_t)pid->Kd * diff) >> 15;

  pid->prev_error = error;

  // Sum
  q31_t output = p_term + i_out_final + d_term;

  // Saturate Output
  if (output > pid->out_max)
    output = pid->out_max;
  if (output < pid->out_min)
    output = pid->out_min;

  return (q15_t)output;
}
