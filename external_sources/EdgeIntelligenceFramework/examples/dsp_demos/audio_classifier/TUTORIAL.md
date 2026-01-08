# Audio Classification Tutorial: Voice Command Recognition

## Learning Objectives

By the end of this tutorial, you will understand:
- How audio signals are represented digitally
- The fundamentals of Mel-Frequency Cepstral Coefficients (MFCCs)
- How to build a keyword spotting (KWS) system
- Neural network classification for audio
- Real-time audio processing on embedded devices

**Level**: Beginner to Intermediate  
**Prerequisites**: Basic C programming, understanding of arrays  
**Time**: 30-45 minutes

---

## 1. Introduction to Keyword Spotting

### What is KWS?
Keyword Spotting (KWS) is the task of detecting specific words or phrases in an audio stream. Common applications include:
- **Voice Assistants**: "Hey Siri", "OK Google", "Alexa"
- **Smart Home**: Voice-controlled lights, thermostats
- **Industrial**: Hands-free commands for machinery
- **Accessibility**: Voice control for disabled users

### The KWS Pipeline

```
┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
│  Audio   │──▶│ Framing  │──▶│  MFCC    │──▶│ Neural   │──▶│ Command  │
│  Input   │   │ Window   │   │ Extract  │   │ Network  │   │ Detected │
└──────────┘   └──────────┘   └──────────┘   └──────────┘   └──────────┘
    16kHz       25ms/10ms       13 coeff      Dense/CNN      "YES","NO"
```

---

## 2. Audio Fundamentals

### 2.1 Digital Audio Representation

Audio is captured as a series of amplitude samples over time:

| Parameter | Typical Value | Description |
|-----------|---------------|-------------|
| Sample Rate | 16,000 Hz | Samples per second |
| Bit Depth | 16-bit | Amplitude resolution |
| Channels | 1 (Mono) | Number of audio channels |

```c
// Example: 1 second of 16kHz audio = 16,000 samples
#define SAMPLE_RATE 16000
float32_t audio[SAMPLE_RATE];  // 1 second buffer
```

### 2.2 Framing and Windowing

Speech signals are **quasi-stationary** - they change slowly compared to the sample rate. We analyze them in **frames**:

| Parameter | Value | Calculation |
|-----------|-------|-------------|
| Frame Length | 25ms | 0.025 × 16000 = 400 samples |
| Frame Stride | 10ms | 0.010 × 16000 = 160 samples |
| Overlap | 15ms | Creates smooth transitions |

```c
#define FRAME_LENGTH   400   // 25ms at 16kHz
#define FRAME_STRIDE   160   // 10ms at 16kHz

// Apply Hamming window to reduce spectral leakage
void apply_hamming_window(float32_t* frame, int len) {
    for (int i = 0; i < len; i++) {
        float w = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (len - 1));
        frame[i] *= w;
    }
}
```

**Why Hamming Window?**
- Raw frames have discontinuities at edges
- FFT assumes periodic signals
- Window smoothly tapers edges to zero
- Reduces "spectral leakage" artifacts

---

## 3. MFCC Feature Extraction

### 3.1 What are MFCCs?

Mel-Frequency Cepstral Coefficients (MFCCs) are features that represent the **spectral envelope** of audio in a way that mimics human hearing.

```
Audio → FFT → Mel Filterbank → Log → DCT → MFCCs
```

### 3.2 The Mel Scale

Humans perceive frequency on a **logarithmic scale** - we're better at distinguishing low frequencies:

```
Mel(f) = 2595 × log₁₀(1 + f/700)

| Frequency (Hz) | Mel    |
|----------------|--------|
| 100            | 150    |
| 500            | 607    |
| 1000           | 1000   |
| 4000           | 2146   |
| 8000           | 2840   |
```

### 3.3 MFCC Computation Steps

```c
// Step 1: Compute FFT (power spectrum)
eif_dsp_rfft(frame, spectrum, FRAME_LENGTH);

// Step 2: Apply Mel filterbank (26 triangular filters)
eif_dsp_mel_filterbank(spectrum, mel_energies, NUM_FILTERS);

// Step 3: Take log of energies
for (int i = 0; i < NUM_FILTERS; i++) {
    mel_energies[i] = logf(mel_energies[i] + 1e-10f);
}

// Step 4: Apply DCT to get MFCCs
eif_dsp_dct(mel_energies, mfcc, NUM_FILTERS, NUM_MFCC);
```

### 3.4 Why 13 MFCCs?

| MFCCs | Information |
|-------|-------------|
| 1 | Overall energy (often discarded) |
| 2-13 | Spectral shape (most useful) |
| 14+ | Fine spectral detail (often noise) |

**Typical configuration**: 13 MFCCs × 49 frames = 637 features for 500ms audio

---

## 4. Neural Network Classification

### 4.1 Network Architecture

For embedded KWS, we use a simple architecture:

```
Input: 13 MFCCs × 49 frames = 637 features
    ↓
Dense Layer: 64 neurons, ReLU
    ↓
Dense Layer: 32 neurons, ReLU
    ↓
Output Layer: 4 neurons, Softmax
    ↓
Classes: "YES", "NO", "STOP", "GO"
```

### 4.2 Model Size Estimation

| Component | Parameters | Memory (float32) |
|-----------|------------|------------------|
| Dense 637→64 | 637×64 + 64 = 40,832 | 163 KB |
| Dense 64→32 | 64×32 + 32 = 2,080 | 8 KB |
| Dense 32→4 | 32×4 + 4 = 132 | 0.5 KB |
| **Total** | **43,044** | **172 KB** |

For MCUs, use **INT8 quantization** to reduce to ~43 KB!

### 4.3 EIF Neural Network API

```c
// Create model
eif_nn_model_t model;
eif_nn_model_init(&model, &pool);

// Add layers
eif_nn_layer_t dense1 = eif_nn_layer_dense(637, 64, EIF_ACTIVATION_RELU);
eif_nn_layer_t dense2 = eif_nn_layer_dense(64, 32, EIF_ACTIVATION_RELU);
eif_nn_layer_t output = eif_nn_layer_dense(32, 4, EIF_ACTIVATION_SOFTMAX);

eif_nn_model_add_layer(&model, &dense1);
eif_nn_model_add_layer(&model, &dense2);
eif_nn_model_add_layer(&model, &output);

// Inference
float32_t output[4];
eif_nn_model_invoke(&model, mfcc_features, output);

// Get prediction
int predicted_class = argmax(output, 4);
```

---

## 5. Complete Code Walkthrough

### 5.1 Configuration

```c
#define SAMPLE_RATE     16000   // 16kHz audio
#define FRAME_LENGTH    400     // 25ms at 16kHz
#define FRAME_STRIDE    160     // 10ms at 16kHz
#define NUM_MFCC        13      // MFCC coefficients
#define NUM_FRAMES      49      // ~500ms of audio
#define NUM_CLASSES     4       // Number of commands
```

### 5.2 Feature Extraction

```c
// Extract MFCCs from audio
void extract_mfcc_features(const float32_t* audio, int audio_len,
                           float32_t* features, int* num_frames) {
    int frame_idx = 0;
    float32_t frame[FRAME_LENGTH];
    float32_t mfcc[NUM_MFCC];
    
    for (int start = 0; start + FRAME_LENGTH <= audio_len; 
         start += FRAME_STRIDE) {
        
        // Copy frame
        memcpy(frame, &audio[start], FRAME_LENGTH * sizeof(float32_t));
        
        // Apply window
        apply_hamming_window(frame, FRAME_LENGTH);
        
        // Compute MFCC
        eif_dsp_mfcc_compute(frame, FRAME_LENGTH, SAMPLE_RATE,
                             NUM_MFCC, &features[frame_idx * NUM_MFCC]);
        
        frame_idx++;
    }
    *num_frames = frame_idx;
}
```

### 5.3 Classification

```c
// Classify audio command
int classify_command(const float32_t* features, int num_features) {
    float32_t output[NUM_CLASSES];
    
    // Run neural network
    eif_nn_model_invoke(&model, features, output);
    
    // Find highest probability
    int best = 0;
    for (int i = 1; i < NUM_CLASSES; i++) {
        if (output[i] > output[best]) {
            best = i;
        }
    }
    
    return best;
}
```

---

## 6. Experiments

### Experiment 1: Try Different Frame Lengths
Modify `FRAME_LENGTH` and observe the effect:
- 200 samples (12.5ms): More temporal resolution
- 800 samples (50ms): More frequency resolution

### Experiment 2: Noise Robustness
Add noise to the audio and test classification:
```c
// Add white noise
for (int i = 0; i < audio_len; i++) {
    audio[i] += 0.1f * ((float)rand()/RAND_MAX - 0.5f);
}
```

### Experiment 3: Different Word Patterns
Modify `word_patterns[]` to create new words and test recognition:
```c
{"HELLO", 250.0f, 400.0f, 0.6f},
{"WORLD", 300.0f, 200.0f, 0.5f}
```

---

## 7. Hardware Deployment

### STM32F4 + I2S Microphone

```c
// I2S configuration for digital microphone
void i2s_init(void) {
    // Enable I2S peripheral
    // Configure for 16kHz, 16-bit, mono
    // Set up DMA for continuous capture
}

// Main loop
while (1) {
    // Wait for audio buffer ready
    if (audio_buffer_ready) {
        extract_mfcc_features(audio_buffer, AUDIO_LEN, features, &num_frames);
        int cmd = classify_command(features, num_frames * NUM_MFCC);
        
        if (cmd != NONE) {
            execute_command(cmd);
        }
    }
}
```

### Memory Requirements

| Component | RAM | Flash |
|-----------|-----|-------|
| Audio buffer (500ms) | 32 KB | - |
| MFCC features | 2.5 KB | - |
| NN weights (INT8) | - | 43 KB |
| Code + data | 5 KB | 30 KB |
| **Total** | **~40 KB** | **~73 KB** |

Compatible with: STM32F4, nRF52840, ESP32

---

## 8. Summary

### Key Concepts
1. **Framing**: Divide audio into overlapping 25ms frames
2. **MFCC**: Extract 13 frequency features per frame
3. **Neural Network**: Classify feature vectors to commands
4. **Real-time**: Process in streaming fashion

### EIF APIs Used
- `eif_dsp_mfcc_compute()` - MFCC extraction
- `eif_dsp_rfft()` - FFT computation
- `eif_nn_model_invoke()` - Neural network inference
- `eif_memory_pool_*()` - Memory management

### Next Steps
- Try `kws_demo` for streaming detection
- Explore `voice_command` for complete voice UI
- Implement on actual hardware with I2S microphone

---

## Further Reading
- [MFCC Tutorial](https://www.kaggle.com/code/davids1992/mfcc-explained)
- [Speech and Language Processing (Jurafsky & Martin)](https://web.stanford.edu/~jurafsky/slp3/)
- [Google Speech Commands Dataset](https://ai.googleblog.com/2017/08/launching-speech-commands-dataset.html)
