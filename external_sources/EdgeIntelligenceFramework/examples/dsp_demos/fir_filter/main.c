/**
 * @file main.c
 * @brief FIR Filter Design Demo
 *
 * Demonstrates FIR filter capabilities:
 * - Windowed-sinc filter design
 * - Different window functions (Hamming, Hanning, Blackman)
 * - Lowpass, Highpass, Bandpass filtering
 * - Frequency response visualization
 *
 * Usage:
 *   ./fir_filter_demo --help
 *   ./fir_filter_demo --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/ascii_plot.h"
#include "../common/demo_cli.h"
#include "eif_dsp_fir.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 8000
#define NUM_SAMPLES 256

static bool json_mode = false;

// Generate noisy signal with multiple frequencies
static void generate_multi_tone(float *buffer, int len, float *freqs,
                                int num_freqs, float noise_level) {
  for (int i = 0; i < len; i++) {
    buffer[i] = 0.0f;
    for (int f = 0; f < num_freqs; f++) {
      buffer[i] += sinf(2.0f * M_PI * freqs[f] * i / SAMPLE_RATE);
    }
    buffer[i] /= num_freqs;
    // Add noise
    buffer[i] += noise_level * ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
  }
}

// Demo: Window function comparison
static void demo_window_functions(void) {
  if (!json_mode) {
    ascii_section("1. Window Functions");
    printf("  Comparing filter design windows\n\n");
  }

  int order = 31;
  float cutoff = 0.2f; // Normalized

  eif_fir_t fir_rect, fir_hamming, fir_blackman;

  eif_fir_design_lowpass(&fir_rect, cutoff, order, EIF_WINDOW_RECTANGULAR);
  eif_fir_design_lowpass(&fir_hamming, cutoff, order, EIF_WINDOW_HAMMING);
  eif_fir_design_lowpass(&fir_blackman, cutoff, order, EIF_WINDOW_BLACKMAN);

  if (json_mode) {
    printf("{\"demo\": \"window_functions\", \"order\": %d}\n", order);
  } else {
    printf("  Filter order: %d, Cutoff: %.2f (normalized)\n\n", order, cutoff);

    // Show center coefficients
    printf("  Rectangular window:\n");
    ascii_plot_waveform("Rect", fir_rect.coeffs, order, 50, 4);

    printf("\n  Hamming window:\n");
    ascii_plot_waveform("Hamm", fir_hamming.coeffs, order, 50, 4);

    printf("\n  Blackman window:\n");
    ascii_plot_waveform("Black", fir_blackman.coeffs, order, 50, 4);

    printf("\n  Note: Hamming/Blackman have better stopband attenuation.\n");
  }
}

// Demo: Lowpass filtering
static void demo_lowpass(void) {
  if (!json_mode) {
    ascii_section("2. Lowpass FIR Filter");
    printf("  Remove high frequency noise from signal\n\n");
  }

  eif_fir_t lpf;
  eif_fir_design_lowpass(&lpf, 0.15f, 31, EIF_WINDOW_HAMMING);

  // Generate signal: 100Hz clean + 1500Hz noise
  float input[NUM_SAMPLES], output[NUM_SAMPLES];
  float freqs[] = {100.0f, 1500.0f};
  generate_multi_tone(input, NUM_SAMPLES, freqs, 2, 0.0f);

  // Filter
  for (int i = 0; i < NUM_SAMPLES; i++) {
    output[i] = eif_fir_process(&lpf, input[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"lowpass\", \"cutoff_norm\": 0.15}\n");
  } else {
    printf("  Input (100Hz + 1500Hz mixed):\n");
    ascii_plot_waveform("In", &input[50], 80, 50, 4);

    printf("\n  Output (high frequency removed):\n");
    ascii_plot_waveform("Out", &output[50], 80, 50, 4);

    printf("\n  FIR lowpass removes the 1500Hz component!\n");
  }
}

// Demo: Highpass filtering
static void demo_highpass(void) {
  if (!json_mode) {
    ascii_section("3. Highpass FIR Filter");
    printf("  Remove DC offset and low frequency drift\n\n");
  }

  eif_fir_t hpf;
  eif_fir_design_highpass(&hpf, 0.1f, 31, EIF_WINDOW_HAMMING);

  // Generate signal: DC + slow drift + fast signal
  float input[NUM_SAMPLES], output[NUM_SAMPLES];
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float dc_drift = 0.5f + 0.3f * sinf(2.0f * M_PI * 10.0f * i / SAMPLE_RATE);
    float signal = 0.3f * sinf(2.0f * M_PI * 500.0f * i / SAMPLE_RATE);
    input[i] = dc_drift + signal;
  }

  // Filter
  for (int i = 0; i < NUM_SAMPLES; i++) {
    output[i] = eif_fir_process(&hpf, input[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"highpass\", \"cutoff_norm\": 0.1}\n");
  } else {
    printf("  Input (DC drift + signal):\n");
    ascii_plot_waveform("In", &input[50], 80, 50, 4);

    printf("\n  Output (drift removed, signal preserved):\n");
    ascii_plot_waveform("Out", &output[50], 80, 50, 4);
  }
}

// Demo: Noise reduction
static void demo_noise_reduction(void) {
  if (!json_mode) {
    ascii_section("4. Noise Reduction");
    printf("  Clean noisy sensor data with averaging FIR\n\n");
  }

  // Simple moving average (all coefficients equal)
  float ma_coeffs[16];
  for (int i = 0; i < 16; i++) {
    ma_coeffs[i] = 1.0f / 16.0f;
  }

  eif_fir_t ma_filter;
  eif_fir_init(&ma_filter, ma_coeffs, 16);

  // Generate noisy sensor data
  float input[NUM_SAMPLES], output[NUM_SAMPLES];
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float clean = sinf(2.0f * M_PI * 50.0f * i / SAMPLE_RATE);
    float noise = 0.5f * ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
    input[i] = clean + noise;
  }

  // Filter
  for (int i = 0; i < NUM_SAMPLES; i++) {
    output[i] = eif_fir_process(&ma_filter, input[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"noise_reduction\", \"taps\": 16}\n");
  } else {
    printf("  Input (noisy sensor):\n");
    ascii_plot_waveform("Noisy", &input[30], 80, 50, 4);

    printf("\n  Output (smoothed):\n");
    ascii_plot_waveform("Clean", &output[30], 80, 50, 4);
  }
}

int main(int argc, char **argv) {
  demo_cli_result_t result =
      demo_parse_args(argc, argv, "fir_filter_demo",
                      "FIR filter design and signal processing demo");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  srand(42); // Reproducible results

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF FIR Filter Design Demo");
    printf("  Finite Impulse Response filter capabilities\n\n");
  }

  demo_window_functions();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_lowpass();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_highpass();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_noise_reduction();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  FIR Filter Features:\n");
    printf("    • Windowed-sinc design (LP, HP, BP)\n");
    printf("    • Multiple window functions\n");
    printf("    • Linear phase (symmetric coefficients)\n");
    printf("    • Guaranteed stability\n\n");
  }

  return 0;
}
