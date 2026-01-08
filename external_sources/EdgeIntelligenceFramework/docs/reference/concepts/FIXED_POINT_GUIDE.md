# Fixed-Point Arithmetic for Embedded ML

A practical guide to Q15 and fixed-point for MCUs without FPU.

> **For embedded developers**: Your MCU has no FPU or it's slow. This guide
> explains how to do DSP and ML fast using integer math only.

---

## Table of Contents

1. [Why Fixed-Point?](#why-fixed-point)
2. [Q Format Explained](#q-format-explained)
3. [Q15 Operations](#q15-operations)
4. [Conversion Between Float and Q15](#conversion-between-float-and-q15)
5. [Overflow and Saturation](#overflow-and-saturation)
6. [When to Use Fixed-Point](#when-to-use-fixed-point)
7. [Common Mistakes](#common-mistakes)

---

## Why Fixed-Point?

### The Problem

Many MCUs lack a **Floating Point Unit (FPU)**:
- All AVR (Arduino Uno, Mega, Nano)
- ARM Cortex-M0, M0+, M3 (many STM32, nRF)
- Some older ESP8266

Without FPU, float operations are **emulated in software** - 10-100x slower!

### Performance Comparison

| MCU | Float Multiply | Q15 Multiply | Speedup |
|-----|----------------|--------------|---------|
| AVR @ 16MHz | 50-100 cycles | 1-3 cycles | 30-50x |
| Cortex-M0 @ 48MHz | 30-50 cycles | 1 cycle | 30-50x |
| Cortex-M3 @ 72MHz | 20-40 cycles | 1 cycle | 20-40x |
| Cortex-M4F @ 80MHz | 1 cycle | 1 cycle | 1x |

### The Solution: Fixed-Point

Represent fractional numbers as **scaled integers**:

```
Float:  0.5
Q15:    16384  (which is 0.5 × 32768)

The processor sees 16384 (integer math).
You interpret it as 0.5.
```

---

## Q Format Explained

### Notation: Qm.n

- **m**: integer bits (before decimal point)
- **n**: fractional bits (after decimal point)
- Total bits = m + n + 1 (sign bit)

### Q15 Format (Most Common for Audio/DSP)

Q0.15 in a 16-bit signed integer:
- 1 sign bit
- 0 integer bits
- 15 fractional bits

```
Range: -1.0 to +0.99997 (almost +1.0)
Resolution: 1/32768 = 0.00003
```

### Q7 Format (For Neural Networks)

Q0.7 in an 8-bit signed integer:
- 1 sign bit
- 0 integer bits
- 7 fractional bits

```
Range: -1.0 to +0.992
Resolution: 1/128 = 0.0078
```

### Bit Layout

```
Q15 (16-bit signed):
┌────┬────────────────────────────────┐
│ S  │  Fractional bits (15)          │
└────┴────────────────────────────────┘
  ↑
Sign bit

Value = integer_value / 2^15 = integer_value / 32768
```

---

## Q15 Operations

### Addition and Subtraction

**Direct** - just add integers:

```c
int16_t a = 16384;  // 0.5
int16_t b = 8192;   // 0.25
int16_t c = a + b;  // 24576 = 0.75 ✓
```

**Watch for overflow!** Sum can exceed int16 range.

### Multiplication

**Must shift down** after multiplying:

```c
// WRONG - overflows and loses precision
int16_t c = a * b;

// RIGHT - use 32-bit intermediate and shift
int16_t q15_mul(int16_t a, int16_t b) {
    int32_t temp = (int32_t)a * b;  // 32-bit result
    return (int16_t)(temp >> 15);    // Shift back to Q15
}
```

**Example**:
```
a = 16384 (0.5 in Q15)
b = 16384 (0.5 in Q15)

temp = 16384 × 16384 = 268,435,456
result = 268,435,456 >> 15 = 8192 (0.25 in Q15) ✓
```

### Division

**Shift before dividing**:

```c
int16_t q15_div(int16_t a, int16_t b) {
    int32_t temp = (int32_t)a << 15;  // Shift up first
    return (int16_t)(temp / b);
}
```

### Summary of Operations

| Operation | Formula | Notes |
|-----------|---------|-------|
| Add | `a + b` | Check overflow |
| Subtract | `a - b` | Check overflow |
| Multiply | `((int32_t)a * b) >> 15` | Need 32-bit temp |
| Divide | `((int32_t)a << 15) / b` | Need 32-bit temp |
| Negate | `-a` | -32768 can't be negated |

---

## Conversion Between Float and Q15

### Float to Q15

```c
int16_t float_to_q15(float f) {
    // Clamp to valid range
    if (f >= 1.0f) return 32767;
    if (f <= -1.0f) return -32768;
    
    return (int16_t)(f * 32768.0f);
}
```

### Q15 to Float

```c
float q15_to_float(int16_t q) {
    return (float)q / 32768.0f;
}
```

### Common Values

| Float | Q15 | Hex |
|-------|-----|-----|
| 1.0 | 32767 | 0x7FFF |
| 0.5 | 16384 | 0x4000 |
| 0.25 | 8192 | 0x2000 |
| 0.0 | 0 | 0x0000 |
| -0.5 | -16384 | 0xC000 |
| -1.0 | -32768 | 0x8000 |

---

## Overflow and Saturation

### The Overflow Problem

```c
int16_t a = 24576;  // 0.75
int16_t b = 24576;  // 0.75
int16_t c = a + b;  // -16384 ?! (overflow wraparound)

// Expected: 1.5, but Q15 max is ~1.0
```

### Solution 1: Saturated Arithmetic

Clamp results to valid range:

```c
int16_t q15_add_sat(int16_t a, int16_t b) {
    int32_t sum = (int32_t)a + b;
    
    if (sum > 32767) return 32767;
    if (sum < -32768) return -32768;
    
    return (int16_t)sum;
}
```

### Solution 2: Scale Down Inputs

Before adding, divide by 2:

```c
int16_t c = (a >> 1) + (b >> 1);  // Each scaled by 0.5
// Result also scaled by 0.5
```

### When to Use Which

| Situation | Solution |
|-----------|----------|
| Filter accumulator | Saturation |
| Mixing audio signals | Scale down |
| Neural network accumulator | Use 32-bit accumulator |
| Final output | Saturation |

---

## When to Use Fixed-Point

### Use Fixed-Point When:

✅ MCU has no FPU  
✅ MCU has slow FPU (<10 MIPS float)  
✅ Processing audio at high sample rates  
✅ Running neural networks on M0/M3  
✅ Memory is critical (16-bit vs 32-bit)

### Use Float When:

✅ MCU has fast FPU (Cortex-M4F, M7)  
✅ Developing/prototyping (easier debugging)  
✅ Wide dynamic range needed (sensors)  
✅ Low sample rate (temperature @ 1 Hz)  

### EIF Platform Recommendations

| Platform | Recommendation |
|----------|----------------|
| Arduino Uno/Mega (AVR) | Q15 only |
| Arduino Nano 33 (nRF52) | Float OK |
| ESP32 | Float OK |
| STM32F0/F1/L0 (M0/M3) | Q15 preferred |
| STM32F4/L4 (M4F) | Float OK |
| Raspberry Pi Pico (M0+) | Q15 for DSP |

---

## Common Mistakes

### Mistake 1: Forgetting to Shift After Multiply

```c
// WRONG
int16_t y = a * b;  // Wrong scale!

// RIGHT
int16_t y = (int16_t)(((int32_t)a * b) >> 15);
```

### Mistake 2: Integer Overflow in Accumulator

```c
// WRONG - overflows after 2 iterations
int16_t sum = 0;
for (int i = 0; i < 16; i++) {
    sum += samples[i];
}

// RIGHT - use 32-bit accumulator
int32_t sum = 0;
for (int i = 0; i < 16; i++) {
    sum += samples[i];
}
int16_t avg = (int16_t)(sum / 16);
```

### Mistake 3: Comparing Q15 to Float Constants

```c
// WRONG
if (q15_value > 0.5) { ... }  // Comparing int16 to float!

// RIGHT
if (q15_value > 16384) { ... }  // Both are integers

// Or define constants
#define Q15_HALF 16384
if (q15_value > Q15_HALF) { ... }
```

### Mistake 4: Not Handling -32768

```c
// WRONG - undefined behavior!
int16_t neg = -value;  // If value == -32768, -(-32768) overflows!

// RIGHT
int16_t q15_negate(int16_t x) {
    if (x == -32768) return 32767;  // Saturate
    return -x;
}
```

### Mistake 5: Using Wrong Q Format

```c
// Mixing Q15 and Q7 without scaling!
int16_t q15_val = 16384;  // 0.5 in Q15
int8_t q7_val = 64;       // 0.5 in Q7

// WRONG
int16_t result = q15_val + q7_val;  // Mixing formats!

// RIGHT - convert first
int16_t q7_as_q15 = (int16_t)q7_val << 8;  // Shift Q7 to Q15
int16_t result = q15_val + q7_as_q15;
```

---

## EIF Fixed-Point Functions

### Q15 Filter Functions

```c
#include "eif_dsp_fir_fixed.h"

// Initialize
eif_fir_q15_t fir;
int16_t coeffs[16];
eif_fir_q15_design_ma(coeffs, 16);
eif_fir_q15_init(&fir, coeffs, 16);

// Process
int16_t output = eif_fir_q15_process(&fir, input);
```

### Conversion Utilities

```c
#include "eif_dsp_fir_fixed.h"

// Float to Q15
int16_t q = eif_float_to_q15(0.5f);

// Q15 to float
float f = eif_q15_to_float(q);

// Q15 multiply
int16_t y = eif_q15_mul(a, b);
```

---

## Quick Reference

### Q15 Constants

```c
#define Q15_ONE     32767   // Maximum positive (~1.0)
#define Q15_HALF    16384   // 0.5
#define Q15_QUARTER 8192    // 0.25
#define Q15_ZERO    0       // 0.0
#define Q15_NEG_ONE -32768  // -1.0
```

### Q15 Operations Macro

```c
#define Q15_MUL(a, b) ((int16_t)(((int32_t)(a) * (b)) >> 15))
#define Q15_ADD_SAT(a, b) q15_add_sat(a, b)
#define Q15_FROM_FLOAT(f) ((int16_t)((f) * 32768.0f))
#define Q15_TO_FLOAT(q) ((float)(q) / 32768.0f)
```

---

## Next Steps

1. **Try the demos**: `./bin/filter_benchmark --batch`
2. **Compare**: Float vs Q15 filter performance
3. **Practice**: Convert a float filter to Q15
4. **Read**: [DSP_FUNDAMENTALS.md](DSP_FUNDAMENTALS.md) for filter theory
