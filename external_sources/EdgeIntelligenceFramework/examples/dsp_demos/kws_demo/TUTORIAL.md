# Keyword Spotting Tutorial: Real-Time Voice Commands

## Learning Objectives

- Audio preprocessing pipeline  
- Wake word detection architecture
- Streaming inference techniques
- Low-latency voice control

**Level**: Intermediate  
**Time**: 40 minutes

---

## 1. KWS Pipeline

```
Microphone → Frame → Window → FFT → Mel Filter → Log → DCT → MFCC
                                                           ↓
                                              ┌───────────────────┐
                                              │   Neural Network  │
                                              │   (CNN or LSTM)   │
                                              └───────────────────┘
                                                           ↓
                                              Keyword Detected!
```

---

## 2. Audio Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sample Rate | 16 kHz | Standard for speech |
| Frame Size | 25 ms (400 samples) | Captures phonemes |
| Frame Stride | 10 ms (160 samples) | Overlap |
| FFT Size | 512 | Power of 2 |
| Mel Bins | 40 | Frequency resolution |
| MFCCs | 13 | Compact features |

---

## 3. Wake Word Architecture

### DS-CNN (Depthwise Separable CNN)

```
Input: [49 frames × 13 MFCCs]
    │
    ▼
Conv2D (64 filters, 3×3) → BN → ReLU
    │
    ▼
DepthwiseConv2D (3×3) → BN → ReLU
    │
    ▼
PointwiseConv2D (64 filters) → BN → ReLU
    │
    ▼
GlobalAveragePooling
    │
    ▼
Dense (num_keywords) → Softmax
```

**Model Size**: ~80KB quantized

---

## 4. Streaming Inference

```c
#define FRAME_LEN 400
#define FRAME_STRIDE 160
#define NUM_FRAMES 49

float mfcc_buffer[NUM_FRAMES][13];
int frame_idx = 0;

void process_audio_chunk(float* audio, int len) {
    static float overlap[FRAME_LEN];
    
    for (int i = 0; i + FRAME_LEN <= len; i += FRAME_STRIDE) {
        // Extract MFCC for this frame
        float mfcc[13];
        eif_dsp_mfcc_single_frame(&audio[i], FRAME_LEN, 16000, mfcc);
        
        // Shift buffer and add new frame
        memmove(mfcc_buffer[0], mfcc_buffer[1], (NUM_FRAMES-1) * 13 * sizeof(float));
        memcpy(mfcc_buffer[NUM_FRAMES-1], mfcc, 13 * sizeof(float));
        
        frame_idx++;
        
        // Run inference every 10 frames
        if (frame_idx % 10 == 0 && frame_idx >= NUM_FRAMES) {
            float output[NUM_KEYWORDS];
            eif_nn_model_invoke(&kws_model, (float*)mfcc_buffer, output);
            
            int keyword = argmax(output, NUM_KEYWORDS);
            if (output[keyword] > 0.9f && keyword != KEYWORD_SILENCE) {
                handle_keyword(keyword);
            }
        }
    }
}
```

---

## 5. Keywords

| Index | Keyword | Action |
|-------|---------|--------|
| 0 | "silence" | (ignore) |
| 1 | "hey_device" | Wake up |
| 2 | "yes" | Confirm |
| 3 | "no" | Cancel |
| 4 | "on" | Activate |
| 5 | "off" | Deactivate |

---

## 6. ESP32 I2S Setup

```c
// INMP441 microphone
i2s_config_t config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_RX,
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .dma_buf_count = 4,
    .dma_buf_len = 256
};

i2s_pin_config_t pins = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_in_num = 22
};

i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
i2s_set_pin(I2S_NUM_0, &pins);
```

---

## Summary

### Key Metrics
- Latency: <200ms end-to-end
- Model: ~80KB INT8
- Accuracy: >95% on clean audio

### EIF APIs
- `eif_dsp_mfcc_compute()` - Feature extraction
- `eif_nn_model_invoke()` - CNN inference
