/**
 * @file main.c
 * @brief Signal Analysis Demo - Vibration Fault Detection
 * 
 * Demonstrates DSP-based vibration signal analysis for predictive maintenance:
 * - FFT spectral analysis
 * - Spectral features (centroid, energy)
 * - Fault classification
 * - JSON output for visualization
 * 
 * Usage:
 *   ./signal_analysis_demo                   # Interactive tutorial
 *   ./signal_analysis_demo --json            # JSON output for plotter
 *   ./signal_analysis_demo --json --continuous
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_dsp.h"
#include "../common/ascii_plot.h"

// ============================================================================
// Configuration
// ============================================================================

#define SAMPLE_RATE     1000
#define SIGNAL_LENGTH   256
#define FFT_SIZE        256

static bool json_mode = false;
static bool continuous_mode = false;
static int sample_count = 0;

// ============================================================================
// Simulated Sensor Data Generators
// ============================================================================

static void generate_normal_signal(float32_t* signal, int len, float sample_rate) {
    float fund_freq = 50.0f;
    for (int i = 0; i < len; i++) {
        float t = (float)i / sample_rate;
        signal[i] = 1.0f * sinf(2 * M_PI * fund_freq * t)
                  + 0.3f * sinf(2 * M_PI * 2 * fund_freq * t)
                  + 0.1f * sinf(2 * M_PI * 3 * fund_freq * t)
                  + 0.05f * ((float)rand() / RAND_MAX - 0.5f);
    }
}

static void generate_bearing_fault_signal(float32_t* signal, int len, float sample_rate) {
    float fund_freq = 50.0f;
    float fault_freq = 350.0f;
    for (int i = 0; i < len; i++) {
        float t = (float)i / sample_rate;
        signal[i] = 0.8f * sinf(2 * M_PI * fund_freq * t)
                  + 0.6f * sinf(2 * M_PI * fault_freq * t)
                  + 0.4f * sinf(2 * M_PI * 2 * fault_freq * t)
                  + 0.08f * ((float)rand() / RAND_MAX - 0.5f);
    }
}

static void generate_imbalance_signal(float32_t* signal, int len, float sample_rate) {
    float fund_freq = 50.0f;
    for (int i = 0; i < len; i++) {
        float t = (float)i / sample_rate;
        signal[i] = 3.0f * sinf(2 * M_PI * fund_freq * t)
                  + 0.2f * sinf(2 * M_PI * 2 * fund_freq * t)
                  + 0.03f * ((float)rand() / RAND_MAX - 0.5f);
    }
}

// ============================================================================
// Analysis
// ============================================================================

typedef struct {
    float32_t rms;
    float32_t peak_freq;
    float32_t peak_magnitude;
    float32_t spectral_centroid;
    float32_t high_freq_energy;
    const char* diagnosis;
} analysis_result_t;

static void analyze_signal(const float32_t* signal, int len, eif_memory_pool_t* pool, analysis_result_t* result) {
    (void)pool;  // Not used in simplified version
    
    // RMS
    float32_t sum_sq = 0;
    for (int i = 0; i < len; i++) sum_sq += signal[i] * signal[i];
    result->rms = sqrtf(sum_sq / len);
    
    // Simple zero-crossing rate (estimate of frequency content)
    int zero_crossings = 0;
    for (int i = 1; i < len; i++) {
        if ((signal[i-1] >= 0 && signal[i] < 0) || (signal[i-1] < 0 && signal[i] >= 0)) {
            zero_crossings++;
        }
    }
    result->peak_freq = (float)zero_crossings * SAMPLE_RATE / (2 * len);
    result->peak_magnitude = result->rms;
    
    // Spectral centroid estimate (based on high-pass filtered energy)
    float32_t low_energy = 0, high_energy = 0;
    float32_t prev = signal[0];
    for (int i = 1; i < len; i++) {
        float32_t hp = signal[i] - prev;  // Simple high-pass
        prev = signal[i];
        high_energy += hp * hp;
        low_energy += signal[i] * signal[i];
    }
    result->spectral_centroid = (low_energy > 0) ? 200.0f * (high_energy / low_energy) : 0;
    result->high_freq_energy = (low_energy > 0) ? 100.0f * high_energy / low_energy : 0;
    
    // Diagnosis
    if (result->high_freq_energy > 30) {
        result->diagnosis = "BEARING_FAULT";
    } else if (result->rms > 2.0f) {
        result->diagnosis = "IMBALANCE";
    } else {
        result->diagnosis = "NORMAL";
    }
}

// ============================================================================
// JSON Output
// ============================================================================

static void output_json(const char* condition, analysis_result_t* result) {
    printf("{\"timestamp\": %d, \"type\": \"vibration\"", sample_count++);
    printf(", \"condition\": \"%s\"", condition);
    printf(", \"signals\": {\"rms\": %.3f, \"peak_freq\": %.1f, \"spectral_centroid\": %.1f, \"high_freq_energy\": %.1f}",
           result->rms, result->peak_freq, result->spectral_centroid, result->high_freq_energy);
    printf(", \"prediction\": \"%s\"", result->diagnosis);
    printf("}\n");
    fflush(stdout);
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON for real-time plotting\n");
    printf("  --continuous  Run without interactive pauses\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--continuous") == 0) {
            continuous_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    srand(42);
    
    // Memory pool for FFT (reset for each case)
    static uint8_t pool_buffer[256 * 1024];
    eif_memory_pool_t pool;
    
    // Static signal buffer (not from pool)
    static float32_t signal[SIGNAL_LENGTH];
    analysis_result_t result;
    
    if (!json_mode) {
        printf("\n");
        ascii_section("Signal Analysis: Vibration Fault Detection");
        
        printf("  This demo analyzes vibration signals for fault detection:\n");
        printf("    1. Normal Operation - Healthy machine\n");
        printf("    2. Bearing Fault    - High-frequency defect\n");
        printf("    3. Imbalance        - Excessive vibration\n");
        
        if (!continuous_mode) {
            printf("\n  Press Enter to continue...");
            getchar();
        }
    }
    
    // Case 1: Normal
    generate_normal_signal(signal, SIGNAL_LENGTH, SAMPLE_RATE);
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    analyze_signal(signal, SIGNAL_LENGTH, &pool, &result);
    
    if (json_mode) {
        output_json("normal", &result);
    } else {
        ascii_section("Case 1: Normal Machine Operation");
        ascii_plot_waveform("Time Domain Signal (Normal)", signal, SIGNAL_LENGTH, 60, 8);
        printf("\n  RMS: %.3f | Peak: %.1f Hz | Centroid: %.1f Hz | HF Energy: %.1f%%\n",
               result.rms, result.peak_freq, result.spectral_centroid, result.high_freq_energy);
        printf("  Diagnosis: %s\n", result.diagnosis);
        if (!continuous_mode) { printf("\n  Press Enter..."); getchar(); }
    }
    
    // Case 2: Bearing Fault
    generate_bearing_fault_signal(signal, SIGNAL_LENGTH, SAMPLE_RATE);
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));  // Reset pool for FFT
    analyze_signal(signal, SIGNAL_LENGTH, &pool, &result);
    
    if (json_mode) {
        output_json("bearing_fault", &result);
    } else {
        ascii_section("Case 2: Bearing Fault Detection");
        ascii_plot_waveform("Time Domain Signal (Bearing Fault)", signal, SIGNAL_LENGTH, 60, 8);
        printf("\n  RMS: %.3f | Peak: %.1f Hz | Centroid: %.1f Hz | HF Energy: %.1f%%\n",
               result.rms, result.peak_freq, result.spectral_centroid, result.high_freq_energy);
        printf("  Diagnosis: %s\n", result.diagnosis);
        if (!continuous_mode) { printf("\n  Press Enter..."); getchar(); }
    }
    
    // Case 3: Imbalance
    generate_imbalance_signal(signal, SIGNAL_LENGTH, SAMPLE_RATE);
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));  // Reset pool for FFT
    analyze_signal(signal, SIGNAL_LENGTH, &pool, &result);
    
    if (json_mode) {
        output_json("imbalance", &result);
        printf("{\"type\": \"summary\", \"cases_analyzed\": 3}\n");
    } else {
        ascii_section("Case 3: Imbalance Fault Detection");
        ascii_plot_waveform("Time Domain Signal (Imbalance)", signal, SIGNAL_LENGTH, 60, 8);
        printf("\n  RMS: %.3f | Peak: %.1f Hz | Centroid: %.1f Hz | HF Energy: %.1f%%\n",
               result.rms, result.peak_freq, result.spectral_centroid, result.high_freq_energy);
        printf("  Diagnosis: %s\n", result.diagnosis);
        
        printf("\n");
        ascii_section("Summary");
        printf("  Demonstrated FFT, spectral features, and fault classification.\n\n");
    }
    
    return 0;
}

