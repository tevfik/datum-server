# FIR & Biquad Filter Tutorial

This tutorial covers digital filter design and implementation using the Edge Intelligence Framework.

## Table of Contents
1. [FIR Filters](#fir-filters)
2. [Biquad Filters](#biquad-filters)
3. [Filter Comparison](#filter-comparison)
4. [Practical Examples](#practical-examples)

---

## FIR Filters

### What is an FIR Filter?

**Finite Impulse Response (FIR)** filters have a finite response to an impulse input. Key characteristics:
- **Linear phase** (symmetric coefficients)
- **Always stable**
- **Higher order** needed for sharp cutoffs

### Basic Usage

```c
#include "eif_dsp_fir.h"

// Create a 31-tap lowpass filter
eif_fir_t lpf;
eif_fir_design_lowpass(&lpf, 0.2f, 31, EIF_WINDOW_HAMMING);

// Process samples
float output = eif_fir_process(&lpf, input_sample);
```

### Window Functions

| Window | Sidelobe | Transition Width | Use Case |
|--------|----------|------------------|----------|
| Rectangular | -13 dB | Narrow | Analysis |
| Hamming | -43 dB | Medium | General audio |
| Hanning | -32 dB | Medium | Spectral analysis |
| Blackman | -58 dB | Wide | High quality audio |

### Design Parameters

- **Cutoff**: Normalized frequency (0 to 0.5, where 0.5 = Nyquist)
- **Order**: Number of coefficients (higher = sharper cutoff)
- **Window**: Tradeoff between sidelobe rejection and transition width

---

## Biquad Filters

### What is a Biquad?

A **Biquad** is a second-order IIR filter section. Benefits:
- **Efficient** (only 5 coefficients per section)
- **Cascadable** for higher orders
- **Standard audio EQ** topology

### Basic Usage

```c
#include "eif_dsp_biquad.h"

// Create a lowpass filter
eif_biquad_t lpf;
eif_biquad_lowpass(&lpf, 1000.0f, 44100.0f, 0.707f);

// Process samples
float output = eif_biquad_process(&lpf, input_sample);
```

### Filter Types

| Type | Function | Use Case |
|------|----------|----------|
| Lowpass | `eif_biquad_lowpass()` | Remove high frequency noise |
| Highpass | `eif_biquad_highpass()` | Remove DC offset/rumble |
| Bandpass | `eif_biquad_bandpass()` | Isolate frequency band |
| Notch | `eif_biquad_notch()` | Remove specific frequency (e.g., 50/60 Hz hum) |
| Peaking | `eif_biquad_peaking()` | Parametric EQ band |
| Low Shelf | `eif_biquad_lowshelf()` | Bass boost/cut |
| High Shelf | `eif_biquad_highshelf()` | Treble boost/cut |

### Butterworth Cascade

For steeper rolloff, cascade multiple biquads:

```c
eif_biquad_cascade_t cascade;
eif_biquad_butter4_lowpass(&cascade, 1000.0f, 44100.0f);  // 4th order

float output = eif_biquad_cascade_process(&cascade, input);
```

---

## Filter Comparison

| Feature | FIR | Biquad (IIR) |
|---------|-----|--------------|
| Phase | Linear | Non-linear |
| Stability | Always stable | Can be unstable |
| Efficiency | Low (many taps) | High (few coeffs) |
| Sharp cutoff | Needs high order | Cascade biquads |
| Audio EQ | Rare | Standard |

### When to Use FIR
- Phase matters (audio crossovers, measurement)
- Absolute stability required
- Pre-designed coefficients (e.g., from MATLAB)

### When to Use Biquad
- Real-time audio processing
- Memory constrained
- Standard EQ curves needed

---

## Practical Examples

### Audio Equalizer

```c
// 5-band parametric EQ
eif_biquad_t eq[5];
eif_biquad_lowshelf(&eq[0], 100.0f, 44100.0f, 3.0f, 0.9f);
eif_biquad_peaking(&eq[1], 400.0f, 44100.0f, 1.5f, -2.0f);
eif_biquad_peaking(&eq[2], 1000.0f, 44100.0f, 1.5f, 1.0f);
eif_biquad_peaking(&eq[3], 4000.0f, 44100.0f, 1.5f, 2.0f);
eif_biquad_highshelf(&eq[4], 8000.0f, 44100.0f, 4.0f, 0.9f);

// Process
for (int i = 0; i < NUM_SAMPLES; i++) {
    float x = input[i];
    for (int b = 0; b < 5; b++) {
        x = eif_biquad_process(&eq[b], x);
    }
    output[i] = x;
}
```

### Noise Reduction (FIR Moving Average)

```c
// 16-tap moving average
float ma_coeffs[16];
for (int i = 0; i < 16; i++) ma_coeffs[i] = 1.0f/16.0f;

eif_fir_t ma;
eif_fir_init(&ma, ma_coeffs, 16);

float clean = eif_fir_process(&ma, noisy_sensor);
```

---

## Running the Demos

```bash
cd build
./bin/fir_filter_demo --batch    # FIR filter demo
./bin/biquad_eq_demo --batch     # Biquad EQ demo
```
