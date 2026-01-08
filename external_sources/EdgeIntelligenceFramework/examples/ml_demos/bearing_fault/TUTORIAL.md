# Bearing Fault Detection Tutorial: Predictive Maintenance

## Learning Objectives

- Bearing defect frequency calculation
- Vibration signal analysis
- Envelope detection for fault diagnosis
- Industrial IoT deployment

**Level**: Intermediate to Advanced  
**Time**: 50 minutes

---

## 1. Bearing Anatomy

```
         ┌─────────────────────┐
         │    Outer Race       │
         │  ┌───────────────┐  │
         │  │   ○   ○   ○   │  │  ← Rolling Elements (Balls)
         │  │ ○    ╳    ○ │  │     ╳ = Shaft
         │  │   ○   ○   ○   │  │
         │  └───────────────┘  │
         │    Inner Race       │
         └─────────────────────┘
               ↑
             Cage (holds balls)
```

---

## 2. Characteristic Frequencies

Each fault type produces vibration at specific frequencies:

| Fault Location | Abbreviation | Formula |
|----------------|--------------|---------|
| Outer Race | BPFO | `n/2 × fr × (1 - d/D)` |
| Inner Race | BPFI | `n/2 × fr × (1 + d/D)` |
| Ball | BSF | `D/2d × fr × (1 - (d/D)²)` |
| Cage | FTF | `fr/2 × (1 - d/D)` |

Where:
- `n` = number of balls
- `fr` = shaft rotation frequency
- `d` = ball diameter
- `D` = pitch diameter

### Example: SKF 6205 at 1800 RPM

```
Shaft freq: 30 Hz
BPFO: 107.4 Hz
BPFI: 162.6 Hz
BSF:  70.4 Hz
FTF:  11.9 Hz
```

---

## 3. EIF Implementation

```c
// Calculate defect frequencies
void calc_bearing_freqs(bearing_t* b, float rpm, freqs_t* f) {
    float fr = rpm / 60.0f;
    float ratio = b->ball_dia / b->pitch_dia;
    
    f->bpfo = 0.5f * b->n_balls * fr * (1 - ratio);
    f->bpfi = 0.5f * b->n_balls * fr * (1 + ratio);
    f->bsf = 0.5f * b->pitch_dia / b->ball_dia * fr * (1 - ratio*ratio);
    f->ftf = 0.5f * fr * (1 - ratio);
}

// Analyze spectrum for faults
fault_t detect_fault(float* spectrum, int n, freqs_t* f) {
    float bpfo_amp = find_peak(spectrum, n, f->bpfo);
    float bpfi_amp = find_peak(spectrum, n, f->bpfi);
    float bsf_amp = find_peak(spectrum, n, f->bsf);
    
    if (bpfo_amp > THRESHOLD) return FAULT_OUTER;
    if (bpfi_amp > THRESHOLD) return FAULT_INNER;
    if (bsf_amp > THRESHOLD) return FAULT_BALL;
    return FAULT_NONE;
}
```

---

## 4. Envelope Analysis

For modulated signals (impulsive faults):

```c
// Hilbert transform envelope
void envelope_analysis(float* signal, float* envelope, int n) {
    // 1. High-pass filter (remove low-freq)
    highpass_filter(signal, n, 1000.0f);
    
    // 2. Rectify
    for (int i = 0; i < n; i++) {
        signal[i] = fabsf(signal[i]);
    }
    
    // 3. Low-pass filter (smooth envelope)
    lowpass_filter(signal, envelope, n, 500.0f);
    
    // 4. FFT of envelope
    eif_dsp_fft_f32(&fft, envelope);
}
```

---

## 5. ESP32 Deployment

```c
void bearing_monitor_task(void* arg) {
    float vibration[1024];
    float spectrum[512];
    
    while (1) {
        // Sample at 10kHz
        for (int i = 0; i < 1024; i++) {
            vibration[i] = read_accelerometer_z();
            vTaskDelay(1 / portTICK_PERIOD_MS / 10);
        }
        
        // FFT
        eif_dsp_fft_f32(&fft, vibration);
        
        // Analyze
        fault_t fault = detect_fault(spectrum, 512, &bearing_freqs);
        
        if (fault != FAULT_NONE) {
            mqtt_publish("alert/bearing", fault_names[fault]);
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
```

---

## Summary

### Key Concepts
- BPFO/BPFI/BSF/FTF characteristic frequencies
- Spectral analysis with FFT
- Envelope analysis for impulsive faults
- Threshold-based detection

### Detection Accuracy
- Outer race: 95% (most common)
- Inner race: 85%
- Ball defects: 75%
- Cage faults: 70%
