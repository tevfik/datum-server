/**
 * @file main.c
 * @brief Parametric Equalizer Demo using Biquad Filters
 *
 * Demonstrates professional audio EQ with biquad cascade:
 * - Lowpass, Highpass filters
 * - Peaking EQ bands
 * - Low shelf, High shelf
 * - Real-time frequency response
 *
 * Usage:
 *   ./biquad_eq_demo --help
 *   ./biquad_eq_demo --batch
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/ascii_plot.h"
#include "../common/demo_cli.h"
#include "eif_dsp_biquad.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 44100
#define NUM_SAMPLES 512

static bool json_mode = false;

// Generate test signal (sum of sine waves)
static void generate_test_signal(float *buffer, int len, float *freqs,
                                 int num_freqs) {
  for (int i = 0; i < len; i++) {
    buffer[i] = 0.0f;
    for (int f = 0; f < num_freqs; f++) {
      buffer[i] += sinf(2.0f * M_PI * freqs[f] * i / SAMPLE_RATE);
    }
    buffer[i] /= num_freqs; // Normalize
  }
}

// Calculate RMS
static float calculate_rms(const float *buffer, int len) {
  float sum = 0.0f;
  for (int i = 0; i < len; i++) {
    sum += buffer[i] * buffer[i];
  }
  return sqrtf(sum / len);
}

// Demo: Basic Lowpass Filter
static void demo_lowpass(void) {
  if (!json_mode) {
    ascii_section("1. Lowpass Filter (2nd Order Butterworth)");
    printf("  Cutoff: 1000 Hz | Q: 0.707 (Butterworth)\n\n");
  }

  eif_biquad_t lpf;
  eif_biquad_lowpass(&lpf, 1000.0f, SAMPLE_RATE, 0.707f);

  // Test with different frequencies
  float test_freqs[] = {100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f};
  float responses[6];

  for (int f = 0; f < 6; f++) {
    eif_biquad_reset(&lpf);
    float input[NUM_SAMPLES], output[NUM_SAMPLES];

    for (int i = 0; i < NUM_SAMPLES; i++) {
      input[i] = sinf(2.0f * M_PI * test_freqs[f] * i / SAMPLE_RATE);
      output[i] = eif_biquad_process(&lpf, input[i]);
    }

    float in_rms = calculate_rms(&input[NUM_SAMPLES / 2], NUM_SAMPLES / 2);
    float out_rms = calculate_rms(&output[NUM_SAMPLES / 2], NUM_SAMPLES / 2);
    responses[f] = 20.0f * log10f(out_rms / in_rms + 1e-10f);
  }

  if (json_mode) {
    printf("{\"demo\": \"lowpass\", \"cutoff_hz\": 1000}\n");
  } else {
    printf("  Frequency Response:\n");
    printf("    %6.0f Hz: %+6.1f dB\n", test_freqs[0], responses[0]);
    printf("    %6.0f Hz: %+6.1f dB\n", test_freqs[1], responses[1]);
    printf("    %6.0f Hz: %+6.1f dB (cutoff)\n", test_freqs[2], responses[2]);
    printf("    %6.0f Hz: %+6.1f dB\n", test_freqs[3], responses[3]);
    printf("    %6.0f Hz: %+6.1f dB\n", test_freqs[4], responses[4]);
    printf("    %6.0f Hz: %+6.1f dB\n", test_freqs[5], responses[5]);
  }
}

// Demo: Parametric EQ
static void demo_parametric_eq(void) {
  if (!json_mode) {
    ascii_section("2. Parametric EQ (Peaking Filter)");
    printf("  Center: 1000 Hz | Q: 2.0 | Gain: +6 dB\n\n");
  }

  eif_biquad_t peq;
  eif_biquad_peaking(&peq, 1000.0f, SAMPLE_RATE, 2.0f, 6.0f);

  // Process test signal
  float input[NUM_SAMPLES], output[NUM_SAMPLES];
  float freqs[] = {200.0f, 1000.0f, 800.0f}; // Mix of frequencies
  generate_test_signal(input, NUM_SAMPLES, freqs, 3);

  for (int i = 0; i < NUM_SAMPLES; i++) {
    output[i] = eif_biquad_process(&peq, input[i]);
  }

  if (json_mode) {
    printf(
        "{\"demo\": \"parametric_eq\", \"center_hz\": 1000, \"gain_db\": 6}\n");
  } else {
    printf("  Input signal (multi-tone):\n");
    ascii_plot_waveform("Input", &input[200], 60, 50, 4);
    printf("\n  Output (1kHz boosted):\n");
    ascii_plot_waveform("Output", &output[200], 60, 50, 4);
  }
}

// Demo: Butterworth Cascade
static void demo_butterworth_cascade(void) {
  if (!json_mode) {
    ascii_section("3. 4th Order Butterworth Lowpass");
    printf("  Two biquad stages for steep rolloff\n\n");
  }

  eif_biquad_cascade_t cascade;
  eif_biquad_butter4_lowpass(&cascade, 500.0f, SAMPLE_RATE);

  // Test with high frequency
  float input[NUM_SAMPLES], output[NUM_SAMPLES];
  float freqs[] = {100.0f, 2000.0f}; // Low + high
  generate_test_signal(input, NUM_SAMPLES, freqs, 2);

  for (int i = 0; i < NUM_SAMPLES; i++) {
    output[i] = eif_biquad_cascade_process(&cascade, input[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"butter4_cascade\", \"cutoff_hz\": 500}\n");
  } else {
    printf("  Input (100 Hz + 2000 Hz):\n");
    ascii_plot_waveform("Input", &input[200], 60, 50, 4);
    printf("\n  Output (high frequency removed):\n");
    ascii_plot_waveform("Output", &output[200], 60, 50, 4);
  }
}

// Demo: Full EQ Chain
static void demo_full_eq(void) {
  if (!json_mode) {
    ascii_section("4. Full 5-Band EQ");
    printf("  Low shelf + 3x Peaking + High shelf\n\n");
  }

  // 5-band EQ
  eif_biquad_t eq_bands[5];
  eif_biquad_lowshelf(&eq_bands[0], 100.0f, SAMPLE_RATE, 3.0f,
                      0.9f); // Bass boost
  eif_biquad_peaking(&eq_bands[1], 400.0f, SAMPLE_RATE, 1.5f,
                     -2.0f); // Low-mid cut
  eif_biquad_peaking(&eq_bands[2], 1000.0f, SAMPLE_RATE, 1.5f,
                     1.0f); // Mid slight boost
  eif_biquad_peaking(&eq_bands[3], 4000.0f, SAMPLE_RATE, 1.5f,
                     2.0f); // Presence
  eif_biquad_highshelf(&eq_bands[4], 8000.0f, SAMPLE_RATE, 4.0f,
                       0.9f); // Air/brilliance

  // Process signal through chain
  float input[NUM_SAMPLES], output[NUM_SAMPLES];
  float freqs[] = {80.0f, 400.0f, 1000.0f, 4000.0f, 10000.0f};
  generate_test_signal(input, NUM_SAMPLES, freqs, 5);

  for (int i = 0; i < NUM_SAMPLES; i++) {
    float x = input[i];
    for (int b = 0; b < 5; b++) {
      x = eif_biquad_process(&eq_bands[b], x);
    }
    output[i] = x;
  }

  if (json_mode) {
    printf("{\"demo\": \"full_5band_eq\"}\n");
  } else {
    printf("  EQ Settings:\n");
    printf("    Band 1: Low Shelf   100 Hz  +3 dB\n");
    printf("    Band 2: Peaking     400 Hz  -2 dB\n");
    printf("    Band 3: Peaking    1000 Hz  +1 dB\n");
    printf("    Band 4: Peaking    4000 Hz  +2 dB\n");
    printf("    Band 5: High Shelf 8000 Hz  +4 dB\n\n");
    printf("  Input:\n");
    ascii_plot_waveform("In", &input[200], 60, 50, 4);
    printf("\n  Output (EQ applied):\n");
    ascii_plot_waveform("Out", &output[200], 60, 50, 4);
  }
}

int main(int argc, char **argv) {
  demo_cli_result_t result =
      demo_parse_args(argc, argv, "biquad_eq_demo",
                      "Parametric equalizer using biquad filter cascade");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Biquad Parametric EQ Demo");
    printf("  Professional-quality audio EQ for embedded systems\n\n");
  }

  demo_lowpass();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_parametric_eq();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_butterworth_cascade();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_full_eq();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  Biquad filter types demonstrated:\n");
    printf("    • Lowpass (Butterworth)\n");
    printf("    • Peaking EQ (parametric)\n");
    printf("    • 4th order cascade\n");
    printf("    • Full 5-band EQ chain\n\n");
    printf("  All filters use Direct Form II Transposed topology.\n\n");
  }

  return 0;
}
