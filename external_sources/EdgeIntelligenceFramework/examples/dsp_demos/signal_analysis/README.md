# Signal Analysis Demo - Vibration Fault Detection

## Overview
This tutorial demonstrates **predictive maintenance** through vibration analysis of rotating machinery using FFT and spectral features.

## Scenario
A factory monitors vibration sensors attached to rotating machines (motors, pumps, compressors). By analyzing the frequency spectrum, we can detect:
- **Normal Operation** - Healthy machine with expected frequency profile
- **Bearing Fault** - High-frequency energy from damaged bearings
- **Imbalance** - Excessive amplitude at fundamental rotation frequency

## Algorithms Used

### 1. Fast Fourier Transform (FFT)
Converts time-domain vibration signal to frequency domain.

```
Time Domain: x(t) → Frequency Domain: X(f)
```

**Why FFT?**
- Reveals hidden periodic patterns
- Identifies fault frequencies specific to machine components
- Enables frequency-based fault signatures

**EIF API:**
```c
eif_fft_config_t fft_cfg;
eif_dsp_fft_init_f32(&fft_cfg, 256, &pool);  // 256-point FFT
eif_dsp_fft_f32(&fft_cfg, complex_buffer);    // In-place FFT
```

### 2. Spectral Features

| Feature | Formula | Meaning |
|---------|---------|---------|
| **RMS** | √(Σx²/N) | Overall vibration intensity |
| **Peak Frequency** | argmax(|X(f)|) | Dominant frequency component |
| **Spectral Centroid** | Σ(f·|X(f)|) / Σ|X(f)| | "Center of mass" of spectrum |
| **High-Freq Energy** | Σ|X(f>200Hz)|² / Σ|X(f)|² | Energy above threshold |

### 3. Fault Classification Rules

```
IF high_freq_energy > 30%:
    → BEARING FAULT (high-frequency impacts)
ELIF rms > 2.0 AND peak_freq < 60Hz:
    → IMBALANCE (excessive fundamental)
ELSE:
    → NORMAL
```

## Demo Walkthrough

1. **Signal Generation** - Simulates three machine conditions with appropriate frequency content
2. **Windowing** - Applies Hann window to reduce spectral leakage
3. **FFT Analysis** - Computes frequency spectrum
4. **Feature Extraction** - Calculates RMS, centroid, peak frequency
5. **Classification** - Applies rule-based fault detection
6. **Visualization** - ASCII waveform and results display

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_dsp_fft_init_f32()` | Initialize FFT with precomputed twiddle factors |
| `eif_dsp_fft_f32()` | Compute in-place complex FFT |
| `eif_dsp_magnitude_f32()` | Compute magnitude from complex output |
| `eif_memory_pool_init()` | Initialize memory pool for allocations |
| `eif_memory_alloc()` | Allocate from pool (no malloc/free) |

## Real-World Applications
- Predictive maintenance in manufacturing
- Motor health monitoring in HVAC systems
- Pump diagnostics in oil & gas
- Rotating equipment monitoring in power plants

## Run the Demo
```bash
cd build && ./bin/signal_analysis_demo
```
