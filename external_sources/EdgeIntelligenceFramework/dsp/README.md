# Signal Processing Module (`dsp/`)

Digital signal processing algorithms for audio, sensors, and time series.

## Features

### Spectral Analysis
| Algorithm | Description |
|-----------|-------------|
| **FFT** | Fast Fourier Transform (power of 2) |
| **RFFT** | Real FFT (optimized for real signals) |
| **STFT** | Short-Time Fourier Transform |
| **Wavelets** | Haar wavelet decomposition |

### Filters
| Type | Description |
|------|-------------|
| **FIR** | Finite Impulse Response |
| **IIR** | Infinite Impulse Response (Biquad) |
| **Filter Design** | Lowpass, Highpass, Bandpass, Bandstop |

### Audio Features
| Feature | Use Case |
|---------|----------|
| **MFCC** | Speech/Audio recognition |
| **Spectral Features** | Centroid, Bandwidth, Rolloff |
| **Energy** | Voice Activity Detection |
| **ZCR** | Zero Crossing Rate |

### Utilities
- Peak detection
- Envelope extraction
- Resampling
- Voice Activity Detection (VAD)
- **Ring Buffer**: Circular buffer for streaming data
- **Audio Codecs**: IMA-ADPCM (4:1 compression)
- **Advanced VAD**: Robust voice detection using Energy and ZCR

## Usage

```c
#include "eif_dsp.h"
#include "eif_ringbuffer.h"
#include "eif_adpcm.h"
#include "eif_vad_advanced.h"

// Ring Buffer
uint8_t buf[1024];
eif_ringbuffer_t rb;
eif_ringbuffer_init(&rb, buf, 1024, true);

// ADPCM Compression
eif_adpcm_state_t state;
eif_adpcm_init(&state);
uint8_t compressed[128];
eif_adpcm_encode(&state, input_pcm, compressed, 256);

// Advanced VAD
eif_vad_adv_config_t cfg = { .energy_threshold = 0.02f, .zcr_threshold = 0.3f };
eif_vad_adv_t vad;
eif_vad_adv_init(&vad, &cfg);
if (eif_vad_adv_process(&vad, frame, 256)) {
    // Voice detected
}
```

## Examples

- **Audio Pipeline Demo**: `examples/dsp_demos/audio_pipeline`
  - Demonstrates microphone simulation -> VAD -> ADPCM -> RingBuffer -> Decoder.
  - Useful for building low-latency Voice-over-BLE or Walkie-Talkie apps.

## Advanced Audio Pipeline

The framework provides a complete chain for audio processing:
1.  **Capture**: Use standard buffer.
2.  **VAD**: `eif_vad_adv_process` to filter silence.
3.  **Compression**: `eif_adpcm_encode` to save bandwidth/storage.
4.  **Buffering**: `eif_ringbuffer_write` for streaming transmission.


// FFT
float complex output[256];
eif_fft(input, output, 256);

// MFCC
float mfcc[13];
eif_mfcc(audio_frame, 512, 16000, 13, mfcc);

// FIR Filter
eif_fir_filter_t fir;
eif_fir_init(&fir, coeffs, 32);
eif_fir_filter(&fir, input, output, n_samples);

// STFT
eif_stft_t stft;
eif_stft_init(&stft, 512, 256, &pool);
eif_stft_process(&stft, audio, spectrogram);
```

## Audio Pipeline
Complete keyword spotting pipeline:
1. Framing (25ms frames, 10ms hop)
2. Windowing (Hamming)
3. FFT
4. Mel filterbank
5. Log compression
6. DCT → MFCC
7. Neural network classifier

## Demos
```bash
./bin/audio_classifier_demo   # KWS demonstration
./bin/signal_analysis_demo    # FFT, filters
```

## Files
- `eif_dsp_fft.h` - FFT, RFFT
- `eif_dsp_filter.h` - FIR, IIR, design
- `eif_dsp_features.h` - MFCC, spectral
- `eif_dsp_audio.h` - Audio pipeline
- `eif_dsp_wavelet.h` - Wavelets

## Advanced DSP Features (Added)

The module now includes advanced signal processing capabilities:

*   **Resampling**: Convert sample rates efficiently using Linear or Cubic interpolation (`eif_dsp_resample.h`).
*   **AGC (Automatic Gain Control)**: Adaptive gain control with configurable target level, attack, and release times (`eif_dsp_agc.h`).
*   **Beamforming**: Delay-and-Sum beamforming for microphone arrays (`eif_dsp_beamformer.h`).
*   **Transforms**:
    *   **STFT**: Short-Time Fourier Transform for spectrogram analysis.
    *   **Goertzel**: Efficient single-tone detection.
    *   **Haar Wavelet**: Discrete Wavelet Transform (1D).
*   **Signal Generator**: Synthesize Sine, Square, Triangle, Sawtooth, and White Noise (`eif_dsp_generator.h`).
*   **Robust Peak Detector**: Adaptive threshold peak detection resilient to noise (`eif_dsp_peak.h`).

All features are unit-tested in `tests/dsp/test_dsp_advanced.c`.
