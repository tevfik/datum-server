# Opus Audio Codec Integration

This guide explains how to integrate the **Opus Audio Codec** into the Edge Intelligence Framework (EIF) for your applications.

## Overview
The EIF Opus integration provides a seamless wrapper (`eif_opus.h`) around the industry-standard `libopus` library. It abstracts initialization, encoding, and decoding into a simple, embedded-friendly API.

## Prerequisites
*   **EIF Framework**: Ensure the `dsp` module is included in your build.
*   **Libopus Source**: Due to licensing/size, you must provide the `libopus` source code.

## File Structure
Your `dsp/third_party` directory should look like this:
```
dsp/
  third_party/
    libopus/
      include/
        opus.h
        opus_types.h
        ...
      src/
        opus_encoder.c
        opus_decoder.c
        ...
      celt/
      silk/
```

## Setup Instructions

### 1. Download Libopus
Download the official release (e.g., v1.4 or v1.5) from [opus-codec.org](https://opus-codec.org) or clone the repository:
```bash
git clone https://gitlab.xiph.org/xiph/opus.git dsp/third_party/libopus
```

### 2. Configure Build (CMake)
The EIF build system (`dsp/CMakeLists.txt`) automatically detects if you have enabled Opus.

**Options:**
*   `EIF_USE_OPUS` (Default: `ON`): Enables compilation of the wrapper and third-party sources.
*   `EIF_OPUS_FIXED_POINT` (Default: `OFF`): 
    *   **OFF**: Uses Floating Point math (Recommended for ESP32, Cortex-M4F/M7).
    *   **ON**: Uses Fixed Point math (Recommended for Cortex-M0/M3 or strict power optimization).

### 3. Usage Example

```c
#include "eif_opus.h"

void audio_task(void) {
    eif_status_t status;
    eif_opus_t opus;

    // 1. Initialize (16kHz, Mono, VOIP mode)
    // Application modes: EIF_OPUS_APP_VOIP, EIF_OPUS_APP_AUDIO, EIF_OPUS_APP_LOWDELAY
    status = eif_opus_init(&opus, 16000, 1, EIF_OPUS_APP_VOIP);
    
    if (status != EIF_STATUS_OK) {
        // Handle error
        return;
    }

    // Set Complexity (0-10) for CPU usage control
    // 0-3: Low CPU (Voice), 4-8: Medium/High Quality (Music), 9-10: Maximum
    eif_opus_set_complexity(&opus, 2);

    // Buffers
    // 20ms frame at 16kHz = 320 samples
    int16_t pcm_in[320]; 
    uint8_t opus_packet[100]; // Output buffer
    int16_t pcm_out[320];

    while (1) {
        // ... Capture Audio into pcm_in ...

        // 2. Encode
        // Returns number of bytes written to opus_packet
        int bytes = eif_opus_encode(&opus, pcm_in, 320, opus_packet, sizeof(opus_packet));

        if (bytes > 0) {
            // ... Transmit packet ...

            // 3. Decode (on the receiving end)
            // Returns number of samples decoded
            int samples = eif_opus_decode(&opus, opus_packet, bytes, pcm_out, 320);
        }
    }

    // 4. Cleanup
    eif_opus_destroy(&opus);
}
```

## ESP32 Specifics
For ESP32 (Xtensa LX6/LX7):
*   **Floating Point**: It is generally recommended to keep `EIF_OPUS_FIXED_POINT` set to `OFF`. The ESP32 floating-point unit handles Opus generic math efficiently.
*   **Performance**: A single Encode/Decode cycle (Complexity 2, 20ms frame) typically takes < 2ms on an ESP32 running at 240MHz.
*   **Stack**: Ensure your FreeRTOS task has at least **4KB to 8KB** of stack space, as Opus allocates significant scratch memory on the stack in some configurations.
