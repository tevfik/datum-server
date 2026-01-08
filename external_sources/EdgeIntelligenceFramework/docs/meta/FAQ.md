# Frequently Asked Questions

Common questions about the Edge Intelligence Framework.

---

## General

### What platforms does EIF support?

**Tested**:
- Linux (x86_64, ARM)
- macOS (Intel, Apple Silicon)
- Arduino (Uno, Mega, Nano 33, ESP32)
- STM32 (F4, L4, H7)
- Raspberry Pi Pico (RP2040)
- ESP32 / ESP32-S3

**Should work** (untested):
- Windows (MinGW)
- Any C99 compiler

---

### What's the minimum MCU requirement?

| Use Case | Minimum | Recommended |
|----------|---------|-------------|
| Simple filters (EMA, median) | 2KB RAM, 8KB Flash | Arduino Uno |
| DSP (FFT, FIR) | 8KB RAM, 32KB Flash | STM32F1, ESP32 |
| ML inference | 32KB RAM, 64KB Flash | STM32F4, ESP32 |
| Neural networks | 64KB+ RAM, 128KB+ Flash | STM32F4, ESP32 |

---

### Is EIF thread-safe?

**Yes**, if you follow these rules:
1. Don't share state structs between threads
2. Each thread uses its own `eif_xyz_t` instance
3. No global mutable state in EIF

---

### Does EIF require dynamic memory allocation?

**No.** All EIF algorithms use static allocation. You provide buffers, EIF uses them.

---

## Signal Processing

### Why is my filter output delayed?

All causal filters have **group delay**. This is physics, not a bug.

| Filter Type | Delay |
|-------------|-------|
| FIR (N taps) | ~N/2 samples |
| IIR (biquad) | Small, frequency-dependent |
| EMA | ~1/alpha samples |

---

### My filter is "ringing" - why?

**Ringing** (oscillation after impulse) happens when filter Q is too high or order is too high.

**Solutions**:
- Reduce Q factor (try 0.7 for Butterworth)
- Use lower order filter
- Use Bessel filter (less ringing, slower rolloff)

---

### Which filter should I use for noise?

| Noise Type | Filter |
|------------|--------|
| High-frequency noise | Lowpass filter |
| Sudden spikes | Median filter |
| 50/60 Hz power line | Notch filter |
| General sensor noise | EMA (simple) or Butterworth LP |
| Random white noise | Moving average |

---

### How do I choose FFT size?

```
Frequency resolution = Sample_rate / FFT_size

Want 1 Hz resolution at 1000 Hz sample rate?
FFT_size = 1000 / 1 = 1000 → round up to 1024
```

Tradeoff: Larger FFT = better frequency resolution, more memory, slower.

---

## Machine Learning

### How much training data do I need?

Rough guidelines:

| Model | Data Per Class |
|-------|----------------|
| Rule-based | 0 (manual rules) |
| Decision Tree | 100-500 |
| Random Forest | 500-2000 |
| Neural Network | 2000-10000+ |

More data almost always helps.

---

### My model is always predicting the same class

**Common causes**:
1. Input not normalized (most common)
2. Features don't match training
3. Class imbalance in training data
4. Model too simple

**Debug steps**:
1. Print input features, verify range matches training
2. Test with known "easy" example
3. Check training accuracy (if low, model is broken)

---

### Float or INT8 quantization?

| Criterion | Use Float | Use INT8 |
|-----------|-----------|----------|
| Development | ✓ | |
| MCU has FPU | ✓ | |
| Need max accuracy | ✓ | |
| Limited memory | | ✓ |
| No FPU | | ✓ |
| Production, memory-tight | | ✓ |

Typical accuracy loss from INT8: 1-3%

---

## Fixed-Point

### Why does my Q15 code give wrong results?

**Top 3 causes**:
1. Forgot to shift after multiply
2. Accumulator overflow (use int32)
3. Wrong scaling when converting from float

Check the [FIXED_POINT_GUIDE.md](concepts/FIXED_POINT_GUIDE.md).

---

### Q15 or Q7?

| Format | Range | Precision | Use |
|--------|-------|-----------|-----|
| Q15 | ±1.0 | 0.00003 | Audio, DSP |
| Q7 | ±1.0 | 0.008 | NN weights |

Q15 for signal processing, Q7 for neural network weights.

---

## Building

### CMake error: "Could not find..."

Make sure you're building from project root:
```bash
mkdir build && cd build
cmake ..
make
```

---

### Compiler warnings about unused functions

EIF uses `static inline` for header-only functions. Some compilers warn if not all are used. Safe to ignore or suppress with `-Wno-unused-function`.

---

### How do I use EIF in my Arduino project?

1. Copy `arduino/EIF/` to `~/Arduino/libraries/`
2. Restart Arduino IDE
3. Include with `#include <EIF.h>`
4. See [ARDUINO_TUTORIAL.md](ARDUINO_TUTORIAL.md) for examples

---

## Debugging

### How do I print values from ML?

```c
// After feature extraction
printf("Features: ");
for (int i = 0; i < N_FEATURES; i++) {
    printf("%.3f ", features[i]);
}
printf("\n");

// After prediction
printf("Prediction: %d (confidence: %.2f)\n", class, confidence);
```

---

### Demo crashes or hangs

**Check**:
1. Stack size (large arrays overflow stack)
2. Memory available (run with smaller buffer)
3. Division by zero (check inputs)

---

### Filter output is NaN

**Causes**:
1. Division by zero in filter design (Q=0, fs=0)
2. Uninitialized state struct
3. NaN in input data

Add checks:
```c
if (isnan(output)) {
    printf("NaN detected! Input=%.3f\n", input);
}
```

---

## Contributing

### How do I add a new algorithm?

1. Create header in appropriate `module/include/`
2. Follow existing patterns (state struct, init, process)
3. Add demo in `examples/`
4. Add test in `tests/`
5. Update documentation

---

### Code style?

- C99
- 4 spaces (no tabs)
- `eif_module_function()` naming
- State struct as first parameter
- Static inline for small functions
- Document preconditions and units

---

## Performance

### Why is my code slower than expected?

**Check**:
1. Compiler optimization enabled (`-O2` or `-O3`)
2. Debug symbols disabled for production
3. Not running in debugger
4. Using appropriate data type (fixed-point if no FPU)

---

### How do I profile on MCU?

```c
// Simple approach
GPIO_HIGH(DEBUG_PIN);
run_algorithm();
GPIO_LOW(DEBUG_PIN);
// Measure pulse width with oscilloscope

// Or use timer
uint32_t start = timer_read();
run_algorithm();
uint32_t elapsed = timer_read() - start;
```
