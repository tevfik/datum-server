# Filter Benchmark Demo

Compare float vs fixed-point filter performance.

## Purpose

Shows that Q15 fixed-point filters are:
- ~1.6x faster than float on x86
- ~10x faster on AVR (no FPU)
- ~2-3x faster on ARM Cortex-M0/M3 (no FPU)

## Running

```bash
./build/bin/filter_benchmark --batch
```

## Sample Output

```
╔═══════════════════════╗
║  Filter Benchmark     ║
╚═══════════════════════╝

--- FIR Filter (16 taps) ---
Float:    0.15 µs/sample
Q15:      0.09 µs/sample
Speedup:  1.67x

--- Biquad Filter ---
Float:    0.04 µs/sample
Q15:      0.03 µs/sample
Speedup:  1.33x

--- Memory Usage ---
Float FIR: 520 bytes
Q15 FIR:   264 bytes
Savings:   49%
```

## When to Use Fixed-Point

| Scenario | Recommendation |
|----------|----------------|
| AVR (Uno, Mega) | Always use Q15 |
| ARM Cortex-M0/M3 | Prefer Q15 |
| ARM Cortex-M4F+ | Float is fine |
| ESP32 | Float is fine |
| Audio (high sample rate) | Consider Q15 |

## API

```c
// Float FIR
eif_fir_t fir;
eif_fir_init(&fir, coeffs, n);
float out = eif_fir_process(&fir, in);

// Q15 FIR
eif_fir_q15_t fir;
eif_fir_q15_init(&fir, coeffs, n);
int16_t out = eif_fir_q15_process(&fir, in);
```
