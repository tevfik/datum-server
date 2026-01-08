# Lessons Learned

Pitfalls, bugs, and hard-won knowledge from developing the Edge Intelligence Framework.

> **For contributors and users**: These are real problems we encountered.
> Learn from our mistakes.

---

## Signal Processing

### Lesson 1: Filter Initialization is Not Optional

**The bug**: Mysterious oscillations and wrong outputs from filters.

**The cause**: Using uninitialized filter state structs.

```c
// WRONG
eif_biquad_t filter;
float y = eif_biquad_process(&filter, x);  // Garbage state!

// RIGHT
eif_biquad_t filter;
eif_biquad_lowpass(&filter, 1000.0f, 100.0f, 0.707f);  // Initialize!
float y = eif_biquad_process(&filter, x);
```

**Prevention**: Documentation emphasizes "must initialize first".

---

### Lesson 2: FFT Size Must Be Power of 2

**The bug**: FFT produces garbage for certain input sizes.

**The cause**: User passed 100 samples to FFT expecting it to work.

```c
// WRONG (unless your FFT handles arbitrary sizes)
eif_fft(samples, 100);  // Not power of 2!

// RIGHT
eif_fft(samples, 128);  // 2^7 = 128 ✓
```

**Prevention**: Added compile-time check and documentation.

---

### Lesson 3: Cutoff Frequency is Relative

**The bug**: "My lowpass doesn't work" reports.

**The cause**: Confusion between absolute Hz and Nyquist-relative cutoff.

Some APIs use fraction of Nyquist (0-1), others use Hz:
```c
// Fraction of Nyquist (0 to 1)
eif_fir_q15_design_ma(coeffs, 16, 0.1f);  // 0.1 = 10% of Nyquist

// Absolute Hz (requires sample rate)
eif_biquad_lowpass(&bq, 8000.0f, 1000.0f, 0.707f);  // fs=8k, fc=1k Hz
```

**Prevention**: Consistent API design, clear documentation.

---

## Machine Learning

### Lesson 4: Features Must Match Between Training and Inference

**The bug**: Model trained to 95% accuracy, deployed at 60%.

**The cause**: Feature extraction code on MCU didn't match Python training.

```python
# Python training
features = [np.mean(x), np.std(x), np.max(x) - np.min(x)]

# C inference - MISMATCH!
float features[3] = {mean(x), std_dev(x), max(x)};  // Missing min!
```

**Prevention**: Generate C feature extraction from Python, or use shared config.

---

### Lesson 5: Normalization Must Be Identical

**The bug**: All predictions are the same class.

**The cause**: Training normalized inputs, inference did not.

```python
# Training
X_normalized = (X - mean) / std
model.fit(X_normalized, y)
```

```c
// Inference - WRONG
prediction = model_predict(raw_sensor_data);  // Not normalized!

// Inference - RIGHT
float normalized = (raw_sensor_data - TRAIN_MEAN) / TRAIN_STD;
prediction = model_predict(normalized);
```

**Prevention**: Export normalization constants with model.

---

### Lesson 6: Class Imbalance Hides Bad Models

**The bug**: Model is 95% accurate but useless.

**The cause**: 95% of data is "normal", model just predicts "normal" always.

**Prevention**: Always check per-class accuracy, use balanced test set.

---

## Fixed-Point

### Lesson 7: Q15 Multiplication Needs 32-Bit Intermediate

**The bug**: Output is always near zero or overflows.

**The cause**: Multiplying two Q15 values in 16-bit.

```c
// WRONG
int16_t y = a * b;  // 16-bit × 16-bit in 16-bit = wrong!

// RIGHT
int32_t temp = (int32_t)a * b;  // 32-bit intermediate
int16_t y = (int16_t)(temp >> 15);
```

**Prevention**: Provided `eif_q15_mul()` macro.

---

### Lesson 8: Filter Coefficients Need Proper Scaling

**The bug**: Q15 filter produces garbage or silence.

**The cause**: Float coefficients were converted to Q15 incorrectly.

```c
// WRONG - Simple cast doesn't work
int16_t q15_coeff = (int16_t)float_coeff;  // Lost fractional part!

// RIGHT - Scale to Q15 range
int16_t q15_coeff = (int16_t)(float_coeff * 32768.0f);
```

**Prevention**: Provided conversion utilities.

---

### Lesson 9: Accumulator Overflow in FIR

**The bug**: Audio clicks and pops, values jump randomly.

**The cause**: Summing many Q15 products in 16-bit accumulator.

```c
// WRONG - Overflows after a few additions
int16_t sum = 0;
for (int i = 0; i < N; i++) {
    sum += q15_mul(coeffs[i], samples[i]);  // Overflow!
}

// RIGHT - Use 32-bit accumulator
int32_t sum = 0;
for (int i = 0; i < N; i++) {
    sum += (int32_t)coeffs[i] * samples[i];
}
int16_t result = (int16_t)(sum >> 15);
```

**Prevention**: All EIF filter implementations use 32-bit accumulators.

---

## Memory Management

### Lesson 10: Stack Overflow on Large Buffers

**The bug**: Device crashes or resets randomly.

**The cause**: Large arrays on stack exceeded stack size.

```c
// WRONG - 4KB on stack!
void process() {
    float buffer[1024];  // 4 KB if float is 4 bytes
    // ... use buffer
}

// RIGHT - Static or heap allocation
static float buffer[1024];  // In .bss, not stack
```

**Prevention**: Documentation recommends static allocation for large buffers.

---

### Lesson 11: Memory Pool Fragmentation

**The bug**: Allocation fails after many alloc/free cycles.

**The cause**: Small gaps between allocations can't fit new requests.

**Prevention**: In EIF, we prefer reusable fixed-size pools:
```c
// Instead of general allocator, use typed pools
eif_pool_alloc_sized(&pool, sizeof(my_struct));
```

---

## Platform-Specific

### Lesson 12: AVR Float is Expensive

**The bug**: Code runs 100x slower than expected on Arduino Uno.

**The cause**: Single float multiply takes 500+ cycles on AVR.

**Prevention**: 
- Clear documentation about AVR limitations
- Provided Q15 alternatives for critical code
- Benchmark demos show real performance

---

### Lesson 13: ESP32 Memory is Split

**The bug**: Allocation fails despite "plenty of free RAM".

**The cause**: ESP32 has separate DRAM and IRAM, both limited.

```
DRAM: ~300 KB (general data)
IRAM: ~128 KB (code and fast data)

Large array in IRAM = no space for code!
```

**Prevention**: ESP32 guide explains memory layout.

---

### Lesson 14: Cortex-M0 Has No Hardware Divide

**The bug**: Division-heavy code runs unexpectedly slow.

**The cause**: M0 has no divide instruction, uses slow software routine.

**Prevention**: 
- Avoid division in inner loops
- Use shifts for power-of-2 division
- Pre-compute reciprocals

---

## Testing

### Lesson 15: Edge Cases Break Everything

**The bug**: Filter works for normal input, explodes on zeros or NaN.

**The cause**: Didn't test edge cases.

Edge cases to always test:
- All zeros input
- All same value input
- Single impulse
- Maximum and minimum values
- NaN and Inf (if using float)

**Prevention**: Test suite includes edge case tests.

---

### Lesson 16: Timing Tests Need Warmup

**The bug**: First run is 10x slower than subsequent runs.

**The cause**: Cache warming, lazy initialization.

```c
// WRONG - First run includes startup overhead
start_timer();
result = run_inference();
stop_timer();
printf("Time: %d us\n", elapsed);

// RIGHT - Warmup first
for (int i = 0; i < 10; i++) {
    run_inference();  // Warmup
}

uint32_t total = 0;
for (int i = 0; i < 100; i++) {
    start_timer();
    run_inference();
    total += stop_timer();
}
printf("Avg time: %d us\n", total / 100);
```

**Prevention**: Benchmark utilities include warmup option.

---

## General

### Lesson 17: Comments Lie, Code Doesn't

**The bug**: Following comment leads to wrong usage.

**The cause**: Code changed, comment didn't.

**Prevention**: 
- Keep comments minimal and high-level
- Document "why" not "what"
- Use self-documenting function/variable names

---

### Lesson 18: Demo Code Becomes Production Code

**The bug**: "Simple demo" code shipped in products with bugs.

**The cause**: Demo was copy-pasted without understanding.

**Prevention**: 
- Demo code should be correct, not just illustrative
- Clear warnings when demo makes simplifying assumptions
