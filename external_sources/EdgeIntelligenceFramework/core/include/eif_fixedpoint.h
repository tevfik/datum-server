#ifndef EIF_FIXEDPOINT_H
#define EIF_FIXEDPOINT_H

#include <stdint.h>
#include <limits.h>

// Fixed-point Types
typedef int8_t  q7_t;
typedef int16_t q15_t;
typedef int32_t q31_t;
typedef int64_t q63_t;

// Ranges
#define EIF_Q7_MAX   ((q7_t)  0x7F)
#define EIF_Q7_MIN   ((q7_t)  0x80)
#define EIF_Q15_MAX  ((q15_t) 0x7FFF)
#define EIF_Q15_MIN  ((q15_t) 0x8000)
#define EIF_Q31_MAX  ((q31_t) 0x7FFFFFFF)
#define EIF_Q31_MIN  ((q31_t) 0x80000000)

// Conversion Macros
#define EIF_FLOAT_TO_Q7(x)   ((q7_t)  ((float)(x) * 128.0f + 0.5f))
#define EIF_Q7_TO_FLOAT(x)   ((float)(x) / 128.0f)

#define EIF_FLOAT_TO_Q15(x)  ((q15_t) ((float)(x) * 32768.0f + 0.5f))
#define EIF_Q15_TO_FLOAT(x)  ((float)(x) / 32768.0f)

#define EIF_FLOAT_TO_Q31(x)  ((q31_t) ((float)(x) * 2147483648.0f + 0.5f))
#define EIF_Q31_TO_FLOAT(x)  ((float)(x) / 2147483648.0f)

// Saturating Arithmetic Functions (Inline for performance)

// Q7 Addition
static inline q7_t eif_q7_add(q7_t a, q7_t b) {
    int16_t val = (int16_t)a + b;
    if (val > EIF_Q7_MAX) return EIF_Q7_MAX;
    if (val < EIF_Q7_MIN) return EIF_Q7_MIN;
    return (q7_t)val;
}

// Q7 Subtraction
static inline q7_t eif_q7_sub(q7_t a, q7_t b) {
    int16_t val = (int16_t)a - b;
    if (val > EIF_Q7_MAX) return EIF_Q7_MAX;
    if (val < EIF_Q7_MIN) return EIF_Q7_MIN;
    return (q7_t)val;
}

// Q15 Addition
static inline q15_t eif_q15_add(q15_t a, q15_t b) {
    int32_t val = (int32_t)a + b;
    if (val > EIF_Q15_MAX) return EIF_Q15_MAX;
    if (val < EIF_Q15_MIN) return EIF_Q15_MIN;
    return (q15_t)val;
}

// Q15 Subtraction
static inline q15_t eif_q15_sub(q15_t a, q15_t b) {
    int32_t val = (int32_t)a - b;
    if (val > EIF_Q15_MAX) return EIF_Q15_MAX;
    if (val < EIF_Q15_MIN) return EIF_Q15_MIN;
    return (q15_t)val;
}

// Q31 Addition
static inline q31_t eif_q31_add(q31_t a, q31_t b) {
    int64_t val = (int64_t)a + b;
    if (val > EIF_Q31_MAX) return EIF_Q31_MAX;
    if (val < EIF_Q31_MIN) return EIF_Q31_MIN;
    return (q31_t)val;
}

// Q31 Subtraction
static inline q31_t eif_q31_sub(q31_t a, q31_t b) {
    int64_t val = (int64_t)a - b;
    if (val > EIF_Q31_MAX) return EIF_Q31_MAX;
    if (val < EIF_Q31_MIN) return EIF_Q31_MIN;
    return (q31_t)val;
}

// Q7 Multiplication (Result is Q7, intermediate is Q15)
// x * y in Q7 = (x * y) >> 7
static inline q7_t eif_q7_mul(q7_t a, q7_t b) {
    int16_t val = ((int16_t)a * b) >> 7;
    // Saturation check usually not needed for mul unless -1 * -1 = 1 (overflow)
    // -128 * -128 = 16384 >> 7 = 128 (Overflows Q7 max 127)
    if (val > EIF_Q7_MAX) return EIF_Q7_MAX;
    return (q7_t)val;
}

// Q15 Multiplication
// x * y in Q15 = (x * y) >> 15
static inline q15_t eif_q15_mul(q15_t a, q15_t b) {
    int32_t val = ((int32_t)a * b) >> 15;
    if (val > EIF_Q15_MAX) return EIF_Q15_MAX;
    return (q15_t)val;
}

// Q31 Multiplication
// x * y in Q31 = (x * y) >> 31
static inline q31_t eif_q31_mul(q31_t a, q31_t b) {
    int64_t val = ((int64_t)a * b) >> 31;
    if (val > EIF_Q31_MAX) return EIF_Q31_MAX;
    return (q31_t)val;
}

// Q15 Division
// a / b in Q15 = (a << 15) / b
static inline q15_t eif_q15_div(q15_t a, q15_t b) {
    if (b == 0) return (a > 0) ? EIF_Q15_MAX : EIF_Q15_MIN;
    int32_t val = ((int32_t)a << 15) / b;
    if (val > EIF_Q15_MAX) return EIF_Q15_MAX;
    if (val < EIF_Q15_MIN) return EIF_Q15_MIN;
    return (q15_t)val;
}

// Q31 Division
// a / b in Q31 = (a << 31) / b
// Note: shifting int32 left by 31 can overflow if not cast to int64
static inline q31_t eif_q31_div(q31_t a, q31_t b) {
    if (b == 0) return (a > 0) ? EIF_Q31_MAX : EIF_Q31_MIN;
    int64_t val = ((int64_t)a << 31) / b;
    if (val > EIF_Q31_MAX) return EIF_Q31_MAX;
    if (val < EIF_Q31_MIN) return EIF_Q31_MIN;
    return (q31_t)val;
}

// Integer Square Root (for Q15/Q31)
// Input is treated as raw integer. For Q15, sqrt(x) in Q15 is sqrt(x * 2^15) ?
// No, if x is Q15 (x = f * 2^15), then sqrt(f) * 2^15 = sqrt(x / 2^15) * 2^15 = sqrt(x) * sqrt(2^15)
// This is complicated.
// Standard approach: sqrt(x) in Q15 format.
// If input is Q15 (0..1.0), output should be Q15 (0..1.0).
// sqrt(f) = y.
// X = f * 2^15. Y = y * 2^15.
// Y = sqrt(X / 2^15) * 2^15 = sqrt(X) * sqrt(2^15).
// So we compute integer sqrt of X, then multiply by sqrt(2^15)? No.
// Let's use a simpler approximation or standard library if available.
// Or just integer sqrt of (x << 15) -> sqrt(f * 2^30) = sqrt(f) * 2^15 = Y.
// So for Q15: Y = sqrt(X << 15).
// For Q31: Y = sqrt(X << 31)? No, 64-bit overflow.
// Let's implement a helper for Q15 sqrt.

static inline q15_t eif_q15_sqrt(q15_t a) {
    if (a <= 0) return 0;
    int32_t val = (int32_t)a << 15;
    // Integer sqrt of val
    // Simple iterative method
    int32_t root = 0;
    int32_t bit = 1 << 30;
    while (bit > val) bit >>= 2;
    while (bit != 0) {
        if (val >= root + bit) {
            val -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (q15_t)root;
}

// --- Fixed-Point Math Functions (Q15) ---

// Sine Approximation (Q15 input/output)
// Input: Scaled angle where [-1, 1) maps to [-Pi, Pi)
// Output: sin(angle) in Q15
static inline q15_t eif_q15_sin(q15_t angle) {
    // Simple Taylor Series or LUT. 
    // For simplicity and no static data, let's use a Bhaskara I approximation or similar.
    // Bhaskara I: sin(x) ~= 16*x*(pi-x) / (5*pi^2 - 4*x*(pi-x)) for x in [0, pi]
    
    // Using a small LUT is better for speed/size trade-off in embedded.
    // But we can't easily put a LUT in a header without static inline issues or multiple definitions.
    // Let's use a polynomial approximation for now.
    // Taylor: x - x^3/6 + x^5/120
    
    // Input angle is normalized: 32768 = Pi.
    // We need to handle range reduction.
    
    // For this task, we'll implement a placeholder that calls float sinf if not optimized,
    // OR implement a real fixed-point approx.
    // Given "math.h should be supported by fixpoint library too", we should avoid math.h dependency if possible.
    
    // Let's use a very simple approximation for now:
    // sin(x) approx x for small x.
    // This is a placeholder. Real implementation requires a lookup table in a .c file.
    // Since we are header-only here, we can't easily add a large LUT.
    // We will declare them here and implement in eif_fixedpoint.c (which exists now).
    return 0; // Placeholder, implemented in .c
}

q15_t eif_q15_sin_impl(q15_t angle); // Defined in eif_fixedpoint.c
q15_t eif_q15_cos_impl(q15_t angle);
q15_t eif_q15_exp_impl(q15_t x);
q15_t eif_q15_log_impl(q15_t x);

#define eif_q15_sin(x) eif_q15_sin_impl(x)
#define eif_q15_cos(x) eif_q15_cos_impl(x)
#define eif_q15_exp(x) eif_q15_exp_impl(x)
#define eif_q15_log(x) eif_q15_log_impl(x)

#endif // EIF_FIXEDPOINT_H
