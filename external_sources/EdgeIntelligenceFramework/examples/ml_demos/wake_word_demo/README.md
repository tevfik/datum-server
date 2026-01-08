# Wake Word Detection Demo

An interactive demo showing how wake word / keyword spotting works.

## Features

- 🎤 **MFCC Feature Extraction**: See mel-frequency cepstral coefficients visualized
- 📊 **Spectrogram Display**: ASCII heatmap of audio features
- 🎯 **DTW Template Matching**: Dynamic time warping for phrase recognition
- 🔊 **Multiple Audio Types**: Test with silence, noise, and speech

## Quick Start

```bash
# Build
cd build && make wake_word_demo

# Run in batch mode
./bin/wake_word_demo --batch

# Interactive mode
./bin/wake_word_demo
```

## How It Works

1. **Audio Input** → Simulated speech/noise
2. **Windowing** → Hamming window, 32ms frames
3. **FFT** → Convert to frequency domain
4. **Mel Filterbank** → Perceptually-weighted frequencies
5. **DCT** → Extract cepstral coefficients (MFCCs)
6. **DTW** → Compare to stored wake word template
7. **Detection** → Trigger if distance below threshold

## Test Scenarios

| Audio Type | Expected Result |
|------------|-----------------|
| Silence | Ignored ✓ |
| Background Noise | Ignored ✓ |
| Other Speech | Ignored ✓ |
| "Hey" only | Ignored ✓ |
| "Device" only | Ignored ✓ |
| **"Hey TinyEdge"** | **Detected! 🔔** |

## API

```c
// Extract MFCCs from audio frame
void extract_mfcc(float* audio, int len, mfcc_frame_t* out);

// DTW distance between MFCC sequences
float dtw_mfcc(mfcc_frame_t* query, int q_len, 
               mfcc_frame_t* template, int t_len);
```
