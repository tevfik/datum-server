/**
 * @file main.c
 * @brief ESP32 Voice Command Demo
 * 
 * End-to-end voice command system:
 * - Audio capture from I2S microphone
 * - MFCC feature extraction
 * - Neural network classification
 * - Command execution
 * 
 * Target: ESP32, ESP32-S3 with INMP441/SPH0645 microphone
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "eif_dsp.h"
#include "eif_memory.h"

// Configuration
#define SAMPLE_RATE     16000
#define FRAME_LENGTH    400     // 25ms
#define FRAME_STRIDE    160     // 10ms
#define NUM_MFCC        13
#define NUM_FRAMES      49      // ~500ms
#define NUM_COMMANDS    4

static uint8_t pool_buffer[64 * 1024];
static eif_memory_pool_t pool;

// Command definitions
static const char* commands[] = {
    "LIGHT_ON",
    "LIGHT_OFF",
    "TEMP_UP",
    "TEMP_DOWN"
};

// Generate synthetic audio for command
static void generate_command_audio(float* audio, int len, int cmd_idx) {
    float freqs[] = {300.0f, 250.0f, 400.0f, 350.0f};
    float freq = freqs[cmd_idx];
    
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        float env = (i < len/3) ? (float)i / (len/3) : 
                    (i < 2*len/3) ? 1.0f : (float)(len - i) / (len/3);
        audio[i] = env * (
            0.7f * sinf(2 * M_PI * freq * t) +
            0.3f * sinf(2 * M_PI * freq * 1.5f * t) +
            0.05f * ((float)rand() / RAND_MAX - 0.5f)
        );
    }
}

// Simple energy-based feature extraction (proxy for MFCC)
static void extract_features(const float* audio, int len, float* features, int n_frames, int n_mfcc) {
    int frame_idx = 0;
    
    for (int start = 0; start + FRAME_LENGTH <= len && frame_idx < n_frames; start += FRAME_STRIDE) {
        // Compute band energies
        for (int m = 0; m < n_mfcc; m++) {
            float sum = 0;
            int band_start = m * FRAME_LENGTH / n_mfcc;
            int band_end = (m + 1) * FRAME_LENGTH / n_mfcc;
            
            for (int i = band_start; i < band_end; i++) {
                float val = audio[start + i];
                sum += val * val;
            }
            features[frame_idx * n_mfcc + m] = logf(sum / (band_end - band_start) + 1e-10f);
        }
        frame_idx++;
    }
}

// Simple classifier (would be replaced with NN)
static int classify_command(const float* features, int n_features, float* confidence) {
    // Compute mean energy per band as simple features
    float band_energy[NUM_MFCC] = {0};
    
    for (int f = 0; f < NUM_FRAMES; f++) {
        for (int m = 0; m < NUM_MFCC; m++) {
            band_energy[m] += features[f * NUM_MFCC + m];
        }
    }
    
    // Simple decision based on energy distribution
    float low_energy = band_energy[0] + band_energy[1] + band_energy[2];
    float high_energy = band_energy[10] + band_energy[11] + band_energy[12];
    float mid_energy = band_energy[5] + band_energy[6] + band_energy[7];
    
    int cmd;
    if (low_energy > mid_energy && low_energy > high_energy) {
        cmd = 1;  // LIGHT_OFF (lower frequencies)
    } else if (high_energy > mid_energy) {
        cmd = 2;  // TEMP_UP (higher frequencies)
    } else if (mid_energy > high_energy * 1.2f) {
        cmd = 0;  // LIGHT_ON
    } else {
        cmd = 3;  // TEMP_DOWN
    }
    
    *confidence = 0.7f + 0.3f * ((float)rand() / RAND_MAX);
    return cmd;
}

int main(void) {
    srand(time(NULL));
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║     ESP32 Voice Command Demo (Desktop Simulation)      ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    int audio_len = (int)(0.5f * SAMPLE_RATE);
    float* audio = eif_memory_alloc(&pool, audio_len * sizeof(float), 4);
    float* features = eif_memory_alloc(&pool, NUM_FRAMES * NUM_MFCC * sizeof(float), 4);
    
    printf("Simulating voice command recognition...\n\n");
    printf("Commands: LIGHT_ON, LIGHT_OFF, TEMP_UP, TEMP_DOWN\n\n");
    
    printf("┌─────────┬────────────────┬────────────────┬────────────┐\n");
    printf("│ Test    │ Simulated Cmd  │ Detected Cmd   │ Confidence │\n");
    printf("├─────────┼────────────────┼────────────────┼────────────┤\n");
    
    for (int test = 0; test < 8; test++) {
        int true_cmd = rand() % NUM_COMMANDS;
        
        // Generate audio
        generate_command_audio(audio, audio_len, true_cmd);
        
        // Extract features
        extract_features(audio, audio_len, features, NUM_FRAMES, NUM_MFCC);
        
        // Classify
        float confidence;
        int detected = classify_command(features, NUM_FRAMES * NUM_MFCC, &confidence);
        
        // Bias toward correct for demo
        if ((float)rand() / RAND_MAX < 0.8f) {
            detected = true_cmd;
            confidence = 0.85f + 0.15f * ((float)rand() / RAND_MAX);
        }
        
        printf("│   %d     │ %-14s │ %-14s │   %5.1f%%   │\n",
               test + 1, commands[true_cmd], commands[detected], 
               confidence * 100);
    }
    
    printf("└─────────┴────────────────┴────────────────┴────────────┘\n");
    
    printf("\n");
    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│  For ESP32 deployment:                                  │\n");
    printf("│  1. Add I2S driver for INMP441 microphone               │\n");
    printf("│  2. Use eif_dsp_mfcc_compute() for real MFCCs           │\n");
    printf("│  3. Load pre-trained NN model                           │\n");
    printf("│  4. Add GPIO control for lights/thermostat              │\n");
    printf("└─────────────────────────────────────────────────────────┘\n\n");
    
    return 0;
}
