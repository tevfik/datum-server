/**
 * @file exp1_window_comparison.c
 * @brief Experiment: Compare different FFT windows
 * 
 * Compares Hann, Hamming, and Blackman windows for
 * spectral analysis of vibration signals.
 */

#include <stdio.h>
#include <math.h>

#define SIGNAL_LEN 256
#define M_PI 3.14159265358979323846

// Window functions
void apply_hann(float* signal, int n) {
    for (int i = 0; i < n; i++) {
        float w = 0.5f * (1.0f - cosf(2 * M_PI * i / (n - 1)));
        signal[i] *= w;
    }
}

void apply_hamming(float* signal, int n) {
    for (int i = 0; i < n; i++) {
        float w = 0.54f - 0.46f * cosf(2 * M_PI * i / (n - 1));
        signal[i] *= w;
    }
}

void apply_blackman(float* signal, int n) {
    for (int i = 0; i < n; i++) {
        float w = 0.42f - 0.5f * cosf(2 * M_PI * i / (n - 1)) 
                       + 0.08f * cosf(4 * M_PI * i / (n - 1));
        signal[i] *= w;
    }
}

// Simple energy calculation
float calculate_energy(const float* signal, int n) {
    float energy = 0;
    for (int i = 0; i < n; i++) {
        energy += signal[i] * signal[i];
    }
    return energy;
}

int main(void) {
    printf("\n=== Window Function Comparison ===\n\n");
    
    float signal_hann[SIGNAL_LEN];
    float signal_hamming[SIGNAL_LEN];
    float signal_blackman[SIGNAL_LEN];
    
    // Generate test signal: 50Hz + 120Hz
    for (int i = 0; i < SIGNAL_LEN; i++) {
        float t = (float)i / 1000.0f;
        float s = sinf(2 * M_PI * 50 * t) + 0.5f * sinf(2 * M_PI * 120 * t);
        signal_hann[i] = s;
        signal_hamming[i] = s;
        signal_blackman[i] = s;
    }
    
    float orig_energy = calculate_energy(signal_hann, SIGNAL_LEN);
    
    apply_hann(signal_hann, SIGNAL_LEN);
    apply_hamming(signal_hamming, SIGNAL_LEN);
    apply_blackman(signal_blackman, SIGNAL_LEN);
    
    printf("Window Type    | Energy Ratio | Main Lobe | Side Lobe\n");
    printf("---------------|--------------|-----------|----------\n");
    printf("Hann           | %.3f        | Narrow    | -31 dB\n", 
           calculate_energy(signal_hann, SIGNAL_LEN) / orig_energy);
    printf("Hamming        | %.3f        | Narrow    | -43 dB\n",
           calculate_energy(signal_hamming, SIGNAL_LEN) / orig_energy);
    printf("Blackman       | %.3f        | Wide      | -58 dB\n",
           calculate_energy(signal_blackman, SIGNAL_LEN) / orig_energy);
    
    printf("\nConclusion:\n");
    printf("- Hann: General purpose, good frequency resolution\n");
    printf("- Hamming: Better sidelobe suppression\n");
    printf("- Blackman: Best dynamic range, wider main lobe\n");
    
    return 0;
}
