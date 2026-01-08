# DSP Fundamentals for Embedded Developers

A conceptual guide to Digital Signal Processing for embedded systems.

> **For embedded developers**: You know registers and interrupts. This guide explains
> DSP concepts you need for sensor processing, audio, and machine learning.

---

## Table of Contents

1. [Why DSP on Embedded?](#why-dsp-on-embedded)
2. [Signals and Sampling](#signals-and-sampling)
3. [Filters: The Foundation](#filters-the-foundation)
4. [FIR vs IIR: Which to Choose?](#fir-vs-iir-which-to-choose)
5. [Frequency Domain and FFT](#frequency-domain-and-fft)
6. [Windowing: Why It Matters](#windowing-why-it-matters)
7. [Filter Design in Practice](#filter-design-in-practice)
8. [Common Mistakes](#common-mistakes)

---

## Why DSP on Embedded?

Every sensor you connect produces a **signal**:
- Accelerometer → motion signal
- Microphone → audio signal  
- Temperature → slow-changing signal
- ADC readings → discrete samples

Raw signals are **noisy, unstable, and hard to interpret**. DSP transforms them into **clean, meaningful data**.

### What DSP Does For You

| Problem | DSP Solution |
|---------|--------------|
| Noisy ADC readings | Smoothing filter (EMA, median) |
| 50/60 Hz power line interference | Notch filter |
| High-frequency sensor noise | Lowpass filter |
| Detecting specific frequencies | FFT, bandpass filter |
| Audio feature extraction | MFCC |

---

## Signals and Sampling

### The Nyquist Rule

**You must sample at least 2x the highest frequency you want to capture.**

```
Signal frequency: 1 kHz
Minimum sample rate: 2 kHz
Recommended sample rate: 4-10 kHz (gives you headroom)
```

**What happens if you violate Nyquist?**
Aliasing - high frequencies fold back as fake low frequencies, corrupting your data.

### Practical Sampling Rates

| Application | Typical Sample Rate |
|-------------|---------------------|
| Temperature | 1-10 Hz |
| Accelerometer (activity) | 50-100 Hz |
| Accelerometer (vibration) | 1-10 kHz |
| Audio (voice) | 8-16 kHz |
| Audio (music) | 44.1 kHz |

### ADC Resolution vs. Noise

Higher ADC bits don't always mean better data:

```
10-bit ADC: 1024 levels
12-bit ADC: 4096 levels  
16-bit ADC: 65536 levels

But if your noise floor is 8 bits, the extra bits just measure noise!
```

**Rule**: Your ADC resolution should match your actual signal quality.

---

## Filters: The Foundation

A filter is a system that **modifies a signal** by:
- Removing unwanted frequencies (noise)
- Keeping wanted frequencies (signal)
- Shaping the signal response

### The Three Filter Types

```
                Frequency Response
    Gain
     │
   1 ├──────┐
     │      │   LOWPASS
     │      └──────────
     └─────────────────► Frequency


     │
   1 │      ┌──────────
     │      │   HIGHPASS
     ├──────┘
     └─────────────────► Frequency


     │
   1 │   ┌──────┐
     │   │      │   BANDPASS
     ├───┘      └───
     └─────────────────► Frequency
```

### When to Use Each

| Filter | Use Case |
|--------|----------|
| **Lowpass** | Remove high-frequency noise, smooth data |
| **Highpass** | Remove DC offset, detect changes |
| **Bandpass** | Isolate specific frequency range |
| **Notch** | Remove specific interference (50/60Hz) |

---

## FIR vs IIR: Which to Choose?

This is the most important filter design decision for embedded systems.

### FIR (Finite Impulse Response)

```c
// FIR: output depends on current and past inputs only
y[n] = b[0]*x[n] + b[1]*x[n-1] + b[2]*x[n-2] + ... + b[N-1]*x[n-N+1]
```

**Pros**:
- Always stable (no feedback)
- Linear phase (important for audio, radar)
- Easy to design

**Cons**:
- Needs more taps (coefficients) for sharp cutoff
- More memory and computation

### IIR (Infinite Impulse Response)

```c
// IIR: output depends on inputs AND past outputs (feedback)
y[n] = b[0]*x[n] + b[1]*x[n-1] + ... - a[1]*y[n-1] - a[2]*y[n-2]
```

**Pros**:
- Much sharper cutoff with fewer coefficients
- Less computation per sample
- Less memory

**Cons**:
- Can become unstable (coefficients must be carefully designed)
- Non-linear phase (distorts waveforms)
- Quantization can cause limit cycles on fixed-point

### Decision Matrix

| Criterion | Choose FIR | Choose IIR |
|-----------|-----------|------------|
| Sharp cutoff needed | ❌ (needs many taps) | ✅ (fewer coefficients) |
| Phase linearity critical | ✅ | ❌ |
| Limited memory/CPU | ❌ | ✅ |
| Fixed-point MCU | ✅ (easier) | ⚠️ (careful with stability) |
| Real-time audio | ✅ | ⚠️ (phase issues) |
| Simple sensor smoothing | Either | Either |

### Practical Recommendation

**For most embedded applications, start with IIR (biquad)**:
- Use pre-designed Butterworth/Chebyshev coefficients
- 2nd or 4th order is usually enough
- Test stability with your actual data

**Use FIR when**:
- You need linear phase (audio processing)
- You're doing fixed-point on AVR (easier to get right)
- You need very specific frequency response

---

## Frequency Domain and FFT

### Why Frequency Domain?

Time domain shows **when** things happen.
Frequency domain shows **what frequencies** are present.

```
Time Domain              Frequency Domain
    │  ∿∿∿                    │
    │ /    \                  │    ┃
    │/      \                 │    ┃
    └────────► time           └────► frequency
                                   ↑
                              Main frequency
```

### FFT: Fast Fourier Transform

FFT converts N time samples into N/2 frequency bins in O(N log N) time.

```c
// Input: 256 time samples at 8 kHz
// Output: 128 frequency bins, each 31.25 Hz wide

Bin 0:   0 Hz (DC)
Bin 1:   31.25 Hz
Bin 2:   62.5 Hz
...
Bin 127: 3968.75 Hz
```

**Frequency resolution = Sample Rate / FFT Size**

### FFT Trade-offs

| FFT Size | Frequency Resolution | Time Resolution | Memory |
|----------|---------------------|-----------------|--------|
| 64 | Poor (125 Hz at 8kHz) | Good | 256 B |
| 256 | Medium (31.25 Hz) | Medium | 1 KB |
| 1024 | Good (7.8 Hz) | Poor | 4 KB |

**You can't have both high frequency AND time resolution** - this is fundamental.

### When to Use FFT

✅ **Good for**:
- Identifying dominant frequencies
- Spectral analysis (vibration, audio)
- Feature extraction for ML

❌ **Not good for**:
- Real-time filtering (use IIR/FIR)
- Very low-memory systems
- Per-sample processing

---

## Windowing: Why It Matters

### The Problem

FFT assumes your signal repeats forever. But your 256 samples have a **start and end**:

```
Signal: ──────/\─────/\─────/\──────
              (what we capture)
              │←────────────→│
              Start          End
              
FFT sees: ────│/\─────/\─────│────
              ↑               ↑
        DISCONTINUITY!  DISCONTINUITY!
```

This causes **spectral leakage** - fake frequencies appear in your FFT.

### The Solution: Window Functions

Apply a window that smoothly tapers the signal at the edges:

```c
for (int i = 0; i < n; i++) {
    sample[i] *= window[i];  // Multiply by window
}
fft(sample);
```

### Which Window to Use?

| Window | Use Case | Leakage | Resolution |
|--------|----------|---------|------------|
| **Rectangular** (none) | Never use for FFT | Worst | Best |
| **Hanning** | General purpose | Good | Good |
| **Hamming** | Speech processing | Good | Good |
| **Blackman** | High dynamic range | Best | Worst |

**Default choice: Hanning or Hamming** - they work for 90% of cases.

---

## Filter Design in Practice

### Step 1: Define Requirements

Before coding, answer these questions:
1. What are you trying to remove? (noise, interference, etc.)
2. What must you keep? (signal of interest)
3. What's your sample rate?
4. What's your CPU/memory budget?

### Step 2: Calculate Cutoff Frequency

```
Cutoff as fraction of Nyquist = Cutoff_Hz / (Sample_Rate / 2)

Example:
  Sample rate: 1000 Hz
  Want to remove > 50 Hz
  Cutoff fraction: 50 / 500 = 0.1
```

### Step 3: Choose Filter Type and Order

| Your Need | Filter Type | Order |
|-----------|-------------|-------|
| Basic smoothing | IIR Butterworth Lowpass | 2 |
| Sharp cutoff | IIR Butterworth Lowpass | 4-6 |
| Linear phase | FIR with Hamming window | 16-64 |
| Remove specific freq | IIR Notch | 2 |

### Step 4: Implement in EIF

```c
#include "eif_dsp_biquad.h"

// Design 4th order Butterworth lowpass
eif_biquad_cascade_t filter;
eif_biquad_butter4_lowpass(&filter, sample_rate, cutoff_hz);

// Process samples
float output = eif_biquad_cascade_process(&filter, input);
```

---

## Common Mistakes

### Mistake 1: Wrong Sample Rate in Filter Design

```c
// WRONG: Using cutoff in Hz directly
eif_biquad_lowpass(&bq, 1000.0f);  // What sample rate??

// RIGHT: Specify sample rate
eif_biquad_lowpass(&bq, sample_rate, cutoff_hz, Q);
```

### Mistake 2: Filter Not Initialized

```c
// WRONG: Using uninitialized filter
eif_biquad_t bq;
float y = eif_biquad_process(&bq, x);  // Garbage!

// RIGHT: Initialize first
eif_biquad_t bq;
eif_biquad_lowpass(&bq, 1000.0f, 50.0f, 0.707f);
float y = eif_biquad_process(&bq, x);
```

### Mistake 3: Processing Too Slowly

```c
// WRONG: Processing in main loop with delays
while (1) {
    float sample = read_adc();
    process(sample);
    delay(100);  // Missed 99% of samples!
}

// RIGHT: Use timer interrupt or DMA
void TIMER_IRQ(void) {
    float sample = read_adc();
    process(sample);  // Called at exact sample rate
}
```

### Mistake 4: Ignoring Overflow in Fixed-Point

```c
// WRONG: Q15 multiplication without scaling
int16_t y = a * x;  // May overflow!

// RIGHT: Use proper Q15 multiply
int16_t y = (int16_t)(((int32_t)a * x) >> 15);
```

### Mistake 5: FFT Without Windowing

```c
// WRONG: Direct FFT
eif_fft(samples, N);

// RIGHT: Window first
eif_apply_window(samples, N, EIF_WINDOW_HANNING);
eif_fft(samples, N);
```

---

## EIF DSP Cheat Sheet

### Quick Filter Selection

```
Need to smooth sensor data?          → eif_ema_update()
Need to remove spikes?               → eif_median_update()
Need to remove high frequencies?     → eif_biquad_lowpass()
Need to remove DC offset?            → eif_biquad_highpass()
Need to remove 50/60Hz?             → eif_biquad_notch()
Need frequency analysis?            → eif_fft() + window
```

### Memory Budget

| Filter | Memory |
|--------|--------|
| EMA | 12 bytes |
| Median (7) | 40 bytes |
| Biquad | 28 bytes |
| Biquad Cascade (4 stages) | 112 bytes |
| FIR (16 taps) | 520 bytes |
| FFT (256 points) | ~2 KB |

---

## Next Steps

1. **Try the demos**: `./bin/filter_benchmark --batch`
2. **Read**: [ML_FUNDAMENTALS.md](ML_FUNDAMENTALS.md) for feature extraction
3. **Practice**: Implement a filter for your sensor
4. **Experiment**: Compare EMA vs median vs biquad on real data
