# Signal Analysis Tutorial: Vibration Monitoring & Fault Detection

## Learning Objectives

By the end of this tutorial, you will understand:
- How to convert time-domain signals to frequency domain using FFT
- Key spectral features for fault detection
- Distinguishing between normal operation and various fault types
- Real-time signal analysis on embedded systems

**Level**: Beginner to Intermediate  
**Prerequisites**: Basic trigonometry, understanding of sine waves  
**Time**: 30-45 minutes

---

## 1. Introduction to Predictive Maintenance

### Why Signal Analysis?

Industrial machinery exhibits characteristic vibration patterns:
- **Healthy machines**: Predictable, low-amplitude vibrations
- **Faulty machines**: Abnormal patterns indicating wear or damage

Early detection prevents:
- Unplanned downtime ($10K-500K/hour lost)
- Catastrophic failures
- Safety hazards

### Common Fault Types

| Fault Type | Vibration Signature |
|------------|---------------------|
| **Imbalance** | Strong 1× RPM component |
| **Misalignment** | Strong 2× RPM + axial vibration |
| **Bearing Defect** | High-frequency harmonics (100-500Hz) |
| **Gear Wear** | Mesh frequency + sidebands |
| **Looseness** | Multiple harmonics (1×, 2×, 3×...) |

---

## 2. Time vs Frequency Domain

### Time Domain

Raw vibration signal as amplitude over time:
```
Amplitude
    ↑
    │   ╱╲      ╱╲      ╱╲
    │  ╱  ╲    ╱  ╲    ╱  ╲
────┼─────────────────────────→ Time
    │      ╲╱      ╲╱      ╲╱
```

**Limitations**: Hard to identify frequency components visually.

### Frequency Domain

After FFT, we see amplitude vs frequency:
```
Magnitude
    ↑
    │  █
    │  █ 
    │  █     █
    │  █     █  █
────┼──────────────────────→ Frequency (Hz)
      50   100 150
```

**Advantage**: Each peak = one frequency component!

---

## 3. Fast Fourier Transform (FFT)

### The Math (Simplified)

FFT decomposes a signal into sine waves:

```
Signal(t) = A₁·sin(2π·f₁·t) + A₂·sin(2π·f₂·t) + ...
```

The FFT finds all the (Aᵢ, fᵢ) pairs.

### Key Parameters

| Parameter | Formula | Example |
|-----------|---------|---------|
| **FFT Size (N)** | Power of 2 | 256, 512, 1024 |
| **Frequency Resolution** | fs / N | 1000/256 = 3.9 Hz |
| **Max Frequency** | fs / 2 | 1000/2 = 500 Hz |

### EIF Implementation

```c
// Initialize FFT
eif_fft_config_t fft_cfg;
eif_dsp_fft_init_f32(&fft_cfg, 256, &pool);

// Prepare complex buffer
float32_t buffer[512];  // 2× for complex (real + imag)
for (int i = 0; i < 256; i++) {
    buffer[2*i] = signal[i];      // Real part
    buffer[2*i + 1] = 0.0f;       // Imag part
}

// Compute FFT
eif_dsp_fft_f32(&fft_cfg, buffer);

// Extract magnitude
for (int i = 0; i < 128; i++) {
    float r = buffer[2*i];
    float im = buffer[2*i + 1];
    magnitude[i] = sqrtf(r*r + im*im);
}
```

---

## 4. Windowing

### Why Window?

FFT assumes periodic signals. Non-periodic signals cause **spectral leakage**:

```
Without Window:        With Hann Window:
    █▓░                     █
    █▓░                     █
    █▓░░░░░                 █ ░
─────────────           ─────────────
  (Leakage!)              (Clean!)
```

### Common Windows

| Window | Use Case | Frequency Resolution | Amplitude Accuracy |
|--------|----------|---------------------|-------------------|
| **Rectangular** | Transients | Best | Poor |
| **Hann** | General analysis | Good | Good |
| **Hamming** | Audio/speech | Good | Good |
| **Blackman** | High dynamic range | Poor | Best |

```c
// Apply Hann window
void apply_hann_window(float32_t* signal, int len) {
    for (int i = 0; i < len; i++) {
        float w = 0.5f * (1.0f - cosf(2 * M_PI * i / (len - 1)));
        signal[i] *= w;
    }
}
```

---

## 5. Spectral Features

### 5.1 RMS (Root Mean Square)

Overall vibration amplitude:

```c
float rms = 0;
for (int i = 0; i < len; i++) {
    rms += signal[i] * signal[i];
}
rms = sqrtf(rms / len);
```

| RMS Level | Interpretation |
|-----------|---------------|
| < 1.0 | Normal |
| 1.0 - 2.0 | Warning |
| > 2.0 | Critical |

### 5.2 Peak Frequency

Most dominant frequency:

```c
int peak_bin = 0;
float peak_mag = 0;
for (int i = 1; i < len/2; i++) {
    if (magnitude[i] > peak_mag) {
        peak_mag = magnitude[i];
        peak_bin = i;
    }
}
float peak_freq = peak_bin * sample_rate / len;
```

### 5.3 Spectral Centroid

"Center of mass" of spectrum (higher = more high-frequency content):

```c
float weighted_sum = 0, total = 0;
for (int i = 0; i < len/2; i++) {
    float freq = i * sample_rate / len;
    weighted_sum += freq * magnitude[i];
    total += magnitude[i];
}
float centroid = weighted_sum / total;
```

### 5.4 High-Frequency Energy Ratio

Percentage of energy above cutoff (e.g., 200Hz):

```c
float high_energy = 0, total_energy = 0;
int cutoff_bin = 200 * len / sample_rate;

for (int i = 0; i < len/2; i++) {
    float e = magnitude[i] * magnitude[i];
    total_energy += e;
    if (i > cutoff_bin) high_energy += e;
}
float hf_ratio = high_energy / total_energy * 100;
```

---

## 6. Fault Detection Logic

### Decision Tree

```
                    ┌─────────────────┐
                    │ Analyze Signal  │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │ HF Energy > 30% │
                    └────────┬────────┘
                        YES  │  NO
                   ┌─────────┴─────────┐
                   │                   │
           ┌───────▼───────┐   ┌───────▼───────┐
           │ BEARING FAULT │   │ RMS > 2.0 &&  │
           └───────────────┘   │ PeakFreq < 60 │
                               └───────┬───────┘
                                   YES │  NO
                              ┌────────┴────────┐
                      ┌───────▼───────┐ ┌───────▼───────┐
                      │   IMBALANCE   │ │    NORMAL     │
                      └───────────────┘ └───────────────┘
```

### Implementation

```c
if (hf_energy > 30.0f) {
    diagnosis = "BEARING FAULT";
} else if (rms > 2.0f && peak_freq < 60.0f) {
    diagnosis = "IMBALANCE";
} else {
    diagnosis = "NORMAL";
}
```

---

## 7. Experiments

### Experiment 1: Different FFT Sizes
Try 128, 256, 512, 1024 and observe frequency resolution.

### Experiment 2: Add More Fault Types
Implement misalignment detection (strong 2× harmonic).

### Experiment 3: Threshold Tuning
Adjust detection thresholds for your specific machinery.

---

## 8. Hardware Deployment

### Typical Setup

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Accelerometer│────▶│   STM32 MCU  │────▶│   Dashboard  │
│ (ADXL345)    │ SPI │              │ UART│   or Cloud   │
└──────────────┘     └──────────────┘     └──────────────┘
```

### Memory Requirements

| Component | RAM | Notes |
|-----------|-----|-------|
| Signal buffer | 1 KB | 256 samples × 4 bytes |
| FFT buffer | 2 KB | Complex output |
| Twiddle factors | 1 KB | Pre-computed |
| **Total** | **~4 KB** | Compatible with Cortex-M0+ |

---

## 9. Summary

### Key Concepts
1. **FFT**: Convert time→frequency domain
2. **Windowing**: Reduce spectral leakage
3. **Features**: RMS, peak frequency, centroid, HF energy
4. **Detection**: Rule-based fault classification

### EIF APIs Used
- `eif_dsp_fft_init_f32()` - Initialize FFT
- `eif_dsp_fft_f32()` - Compute FFT
- `eif_memory_alloc()` - Memory management

### Next Steps
- Try `anomaly_detection` for ML-based detection
- Explore `time_series` for trend analysis
- Implement on hardware with real accelerometer
