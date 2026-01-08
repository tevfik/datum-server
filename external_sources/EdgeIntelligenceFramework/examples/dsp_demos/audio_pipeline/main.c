#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "eif_dsp.h"
#include "eif_ringbuffer.h"
#include "eif_adpcm.h"
#include "eif_vad_advanced.h"

#define FRAME_SIZE 256
#define SAMPLE_RATE 16000
#define TOTAL_FRAMES 100
#define RB_SIZE (1024 * 10) // 10KB buffer

// Simulate Microphone Input
void mock_mic_input(int16_t *buffer, int frame_idx) {
    // Generate signal: 
    // Frames 20-50: Speech-like (Sine 400Hz + Harmonics)
    // Others: Noise
    
    bool is_speech = (frame_idx >= 20 && frame_idx <= 50);
    
    for (int i = 0; i < FRAME_SIZE; i++) {
        float sample = 0.0f;
        
        if (is_speech) {
            // "Voice" component
            sample += 0.5f * sinf(2.0f * M_PI * 400.0f * i / SAMPLE_RATE);
            sample += 0.2f * sinf(2.0f * M_PI * 800.0f * i / SAMPLE_RATE);
            // Envelope modulation to look like syllables
            sample *= (0.5f + 0.5f * sinf(2.0f * M_PI * 5.0f * frame_idx / SAMPLE_RATE)); 
        }
        
        // Background noise
        float noise = ((float)rand() / RAND_MAX - 0.5f) * 0.05f;
        sample += noise;
        
        buffer[i] = (int16_t)(sample * 10000.0f); // Scale to int16
    }
}

int main(void) {
    printf("=== Edge AI Audio Pipeline Demo ===\n");
    printf("Pipeline: Mic -> Advanced VAD -> ADPCM Encoder -> Ring Buffer -> Decoder\n\n");

    // 1. Initialize Components
    eif_vad_adv_config_t vad_cfg = {
        .energy_threshold = 0.0005f, 
        .zcr_threshold = 0.6f, // High ZCR might indicate noise, not voice?
        // ZCR logic in advanced VAD: (zcr > thresh) && (energy > 0.5 * thresh)
        // If we want ZCR to HELP detect fricatives, we keep it reachable (e.g. 0.3)
        // Check my advanced_vad.c implementation:
        // bool zcr_boost = (zcr > vad->config.zcr_threshold) && (energy > vad->config.energy_threshold * 0.5f);
        // My voice has ZCR ~0.07. So ZCR boost won't trigger. 
        // Energy detection will be the main driver.
        .frame_size = FRAME_SIZE,
        .sample_rate = SAMPLE_RATE,
        .hangvoer_frames = 5
    };
    eif_vad_adv_t vad;
    eif_vad_adv_init(&vad, &vad_cfg);

    eif_adpcm_state_t enc_state, dec_state;
    eif_adpcm_init(&enc_state);
    eif_adpcm_init(&dec_state);

    uint8_t *rb_storage = (uint8_t*)malloc(RB_SIZE);
    eif_ringbuffer_t rb;
    eif_ringbuffer_init(&rb, rb_storage, RB_SIZE, false); // No overwrite, we simulate streaming

    int16_t mic_buffer[FRAME_SIZE];
    uint8_t compressed_buffer[FRAME_SIZE / 2]; // 4-bit per sample, so size/2 bytes
    int16_t decoded_buffer[FRAME_SIZE];

    int frames_recorded = 0;
    int bytes_streamed = 0;

    // 2. Processing Loop
    for (int iframe = 0; iframe < TOTAL_FRAMES; iframe++) {
        // A. Capture
        mock_mic_input(mic_buffer, iframe);

        // Calculate Energy for debug display
        float debug_energy = 0.0f;
        for(int k=0; k<FRAME_SIZE; k++) {
            float s = (float)mic_buffer[k] / 32768.0f;
            debug_energy += s * s;
        }
        debug_energy /= FRAME_SIZE;

        // B. VAD Check
        bool is_voice = eif_vad_adv_process(&vad, mic_buffer, FRAME_SIZE);

        printf("Frame %3d | Energy: %1.6f | ZCR: %1.2f | Status: %s", 
               iframe, 
               debug_energy,
               eif_calculate_zcr(mic_buffer, FRAME_SIZE),
               is_voice ? "\033[32mVOICE\033[0m" : "\033[90m......\033[0m");

        if (is_voice) {
            frames_recorded++;
            // C. Compress
            size_t comp_len = eif_adpcm_encode(&enc_state, mic_buffer, compressed_buffer, FRAME_SIZE);
            
            // D. Buffer / Transmit
            size_t written = eif_ringbuffer_write(&rb, compressed_buffer, comp_len);
            bytes_streamed += written;
            
            printf(" | Encoded %zu bytes -> RB Free: %zu", comp_len, eif_ringbuffer_available_write(&rb));
        }
        printf("\n");
    }

    printf("\n=== Transmission Summary ===\n");
    printf("Total Frames Scanned: %d\n", TOTAL_FRAMES);
    printf("Voice Frames Detected: %d\n", frames_recorded);
    printf("Total Compressed Data: %d bytes\n", bytes_streamed);
    printf("Compression Ratio: 4:1 (Fixed IMA-ADPCM)\n");

    // 3. Decoding Test (Consumer side)
    printf("\n=== Decoding Check (Replaying Buffer) ===\n");
    int decoded_frames = 0;
    
    // While enough data for at least one frame (128 bytes compressed = 256 samples)
    uint8_t read_chunk[FRAME_SIZE / 2];
    
    while (eif_ringbuffer_available_read(&rb) >= sizeof(read_chunk)) {
        eif_ringbuffer_read(&rb, read_chunk, sizeof(read_chunk));
        
        eif_adpcm_decode(&dec_state, read_chunk, decoded_buffer, sizeof(read_chunk)); // Produces FRAME_SIZE samples
        decoded_frames++;
        
        if (decoded_frames % 5 == 0) {
           printf("Decoded Frame %d... (First Sample: %d)\n", decoded_frames, decoded_buffer[0]); 
        }
    }

    printf("Successfully decoded %d frames from stream.\n", decoded_frames);

    free(rb_storage);
    return 0;
}
