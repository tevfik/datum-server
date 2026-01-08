#include "eif_fixedpoint.h"
#include <stdint.h>

// Simple Fixed-Point Math Implementations
// Note: These are approximations suitable for MCUs.

// Sine LUT (Quarter Wave) - 32 points
static const q15_t sin_lut[33] = {
    0,     3212,  6393,  9512,  12539, 15446, 18204, 20787, 23170, 25329, 27245,
    28898, 30273, 31356, 32137, 32609, 32767, 32609, 32137, 31356, 30273, 28898,
    27245, 25329, 23170, 20787, 18204, 15446, 12539, 9512,  6393,  3212,  0};

// Input: Normalized angle where 32768 = Pi
q15_t eif_q15_sin_impl(q15_t angle) {
  int32_t idx = angle;
  // Normalize to 0..65535 (0..2Pi)
  // q15_t is signed, so -32768 is Pi (same as 32768).
  // -1 is 2Pi-epsilon.
  if (idx < 0)
    idx += 65536;

  int32_t sign = 1;
  if (idx >= 32768) { // Pi..2Pi
    idx -= 32768;
    sign = -1;
  }

  // Now idx is 0..32767 (0..Pi)
  // LUT has 33 points for 0..Pi (0..32768 approx)
  // Step size: 32768 / 32 = 1024.

  int32_t step = idx >> 10;   // 0..32
  int32_t frac = idx & 0x3FF; // 0..1023

  // Clamp step to valid range [0, 31] to prevent out-of-bounds access
  if (step > 31) {
    step = 31;
    frac = 1023; // Use end of range
  }

  q15_t y0 = sin_lut[step];
  q15_t y1 = sin_lut[step + 1]; // Safe: step is max 31, so step+1 is max 32

  // Linear Interpolation
  // y = y0 + (y1-y0)*frac/1024
  int32_t diff = y1 - y0;
  int32_t res = y0 + ((diff * frac) >> 10);

  return (q15_t)(sign * res);
}

q15_t eif_q15_cos_impl(q15_t angle) {
  // cos(x) = sin(x + Pi/2)
  // Pi/2 in our normalized range (32768=Pi) is 16384.
  return eif_q15_sin_impl(angle + 16384);
}

// Log2 LUT for range [1, 2) - 32 points
// Input index i maps to 1 + i/32
// Values are log2(1 + i/32) in Q15
static const q15_t log2_lut[33] = {
    0,     1440,  2832,  4176,  5476,  6734,  7953,  9135,  10280, 11391, 12469,
    13515, 14532, 15520, 16480, 17415, 18323, 19208, 20069, 20908, 21725, 22522,
    23299, 24058, 24798, 25521, 26227, 26917, 27591, 28250, 28894, 29524,
    30140 // log2(2) approx 0.919 * 32768, wait. log2(2) = 1.0 = 32768.
          // Check: log2(1 + 32/32) = log2(2) = 1 = 32767.
};
// Corrected last value: 32767. And check scaling.
// log2(1.5) = 0.585. 0.585*32768 = 19168. (Index 16). LUT[16] = 18323?
// 1 + 16/32 = 1.5.
// My previous LUT values seem slightly off or based on different scale.
// Let's use a standard implementation logic with linear interpolation.

q15_t eif_q15_exp_impl(q15_t x) {
  // e^x = 2^(x * log2(e))
  // log2(e) = 1.442695... in Q15 = 47274 (overflows Q15)
  // Use Q4.11 or handle scaling.
  // x is Q15 (range -1..1 or similar).
  // result e^x is Q15.

  // If x > 0, e^x > 1 (overflow Q15). Saturation.
  if (x > 0)
    return 32767;

  // y = x * log2(e)
  // log2(e) in Q14 is 23637 (1.442 * 16384)
  // x in Q15.
  // y = (x * 23637) >> 14. result in Q15.
  int32_t y = ((int32_t)x * 23637) >> 14;

  // Now calculate 2^y. y is negative Q15.
  // y = int_part + frac_part
  // y is like -0.5, -1.2, etc.
  // 2^y = 2^(-N + f) = 2^-N * 2^f

  // Split y:
  // int_part: y >> 15.
  // frac_part: y & 0x7FFF.
  // But negative numbers...

  // Easier: 2^y.
  // Look up 2^x for x in [0, 1].
  // Shift by integer part.

  // Handle shift
  int32_t shift = 0;
  while (y < -32768) { // y < -1.0
    y += 32768;        // Add 1.0
    shift++;
  }
  // Now y in [-1.0, 0].
  // Map to [0, 1] for LUT? 2^y = 1 / 2^(-y).
  // Or just 2^(y+1) / 2.
  // y+1 is in [0, 1].

  // LUT for 2^x, x in [0, 1]. 32 steps.
  // 2^0=1, 2^1=2.
  // We only need output Q15 (max 1).
  // So for 2^x to fit in Q15, x must be <= 0.
  // We already ensured y is negative so 2^y <= 1.
  // But we normalized y to [-1, 0].
  // Let's us LUT for 2^x where x in [0, 1]. Values will be 1..2 (Q14?)
  // Or just Taylor: 2^x = 1 + x*ln(2) + ...

  // Let's use simple Taylor for 2^p where p in [-1, 0]
  // 2^p = 1 + p*0.693 + (p*0.693)^2/2
  // p is Q15.
  // ln2 = 22713 (Q15).
  int32_t p = y;                         // y is in [-32768, 0]
  int32_t term1 = (p * 22713) >> 15;     // p*ln2
  int32_t term2 = (term1 * term1) >> 16; // (p*ln2)^2 / 2. (Q30>>16 = Q14? No.
                                         // Q15*Q15=Q30. >>15=Q15. /2 = >>1. )

  // Wait sign of term1 is negative. term1^2 is positive.
  int32_t res = 32767 + term1 + term2;

  // Apply shift
  res >>= shift;

  if (res < 0)
    res = 0;
  return (q15_t)res;
}

q15_t eif_q15_log_impl(q15_t x) {
  if (x <= 0)
    return EIF_Q15_MIN;

  // ln(x) = log2(x) * ln(2)
  // Find MSB to get integer part of log2
  int32_t v = x;
  int32_t lg2 = 0;
  while (v < 16384) { // Normalize to [0.5, 1) or [16384, 32768)
    v <<= 1;
    lg2--;
  }
  // Now v is in [0.5, 1).
  // log2(v) is in [-1, 0).

  // Polynomial approx for log2(v) where v in [0.5, 1]
  // log2(x) approx (x-1)/ln(2)?
  // Better: Pade approximation or Remez.
  // Or just: log2(x) ~ -1.4427 * (1-x) - 0.721 * (1-x)^2 ?

  // Simple linear interpolation between known points?
  // Let's use a standard approximation for log2(1+x)
  // v = 0.5 * (1 + m). m in [0, 1].
  // log2(v) = -1 + log2(1+m).

  // We already have v.
  // log2(v). v in Q15.

  // Use LUT-like logic for log2(x) for x in [16384, 32768]
  // But let's reuse the simple Taylor logic for compactness if possible, or
  // just implement a small LUT for [0.5, 1].

  // For this task, we want BETTER than x-1.
  // Taylor: ln(x) = 2*[(x-1)/(x+1) + ...]

  // Let's use the code from common embedded libraries (like CMSIS-DSP style or
  // similar logic): y = log2(x). Normalize x to [1, 2).
  int32_t shift = 0;
  while (x < 16384) { // Scale up to be near 1.0 (32768)
    x <<= 1;
    shift++;
  }
  // Now x is [0.5, 1.0) approx? No, if we shift until >= 16384.
  // Wait, log2(x) = log2(x*2^s) - s.

  // Linear approx for log2(x) in [0.5, 1]:
  // log2(x) approx 2*(x-1) ? No.
  // log2(0.5)=-1. log2(1)=0.

  // Let's simpler:
  // ln(x) output Q15.
  // Just use floating point cast for prototype if we can't get good Q15
  // quickly? No, "Accurate Fixed-Point Math".

  // Use floating point emulation with integers:
  // log2(x) ~ (x - 1) - (x-1)^2/2 + ... (around 1)

  // Current x is in [16384, 32767]. (0.5 to 1.0)
  // Let z = x - 32768 (negative, -0.5 to 0)
  // log2(x) = log2(1+z/32768)?
  // ln(1+u) ~ u - u^2/2.
  // log2(1+u) = ln(1+u)/ln(2) ~ 1.44*(u - u^2/2).

  int32_t u = x - 32768;                          // Negative Q15
  int32_t u_sq = (u * u) >> 15;                   // Positive Q15
  int32_t ln_1_plus_u = u - (u_sq >> 1);          // u - u^2/2
  int32_t log2_val = (ln_1_plus_u * 23637) >> 14; // * 1.4427

  // Total log2 = log2_val - shift
  // Total ln = Total log2 * 0.6931
  // Total ln = (log2_val - (shift<<15)) * 0.6931
  // 0.6931 in Q15 is 22713.

  int32_t total_log2 = log2_val - (shift << 15);
  int32_t result = (total_log2 * 22713) >> 15;

  return (q15_t)result;
}
