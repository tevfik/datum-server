/**
 * @file main.c
 * @brief Signal Smoothing Filters Demo
 *
 * Demonstrates real-time signal processing filters:
 * - Exponential Moving Average (EMA)
 * - Median Filter (spike removal)
 * - Moving Average
 * - Rate Limiter (slew control)
 * - Hysteresis (Schmitt trigger)
 * - Debounce (button filtering)
 *
 * Usage:
 *   ./smooth_filters_demo                  # Interactive demo
 *   ./smooth_filters_demo --json           # JSON output
 *   ./smooth_filters_demo --help           # Help
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/ascii_plot.h"
#include "../common/demo_cli.h"
#include "eif_dsp_smooth.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Demo configuration
#define NUM_SAMPLES 50
static bool json_mode = false;

// Generate noisy sine wave
static void generate_noisy_signal(float *signal, int len, float noise_level) {
  for (int i = 0; i < len; i++) {
    float t = (float)i / len * 4 * M_PI;
    signal[i] = sinf(t) + ((float)rand() / RAND_MAX - 0.5f) * noise_level;
  }
}

// Add spikes to signal
static void add_spikes(float *signal, int len, int num_spikes) {
  for (int i = 0; i < num_spikes; i++) {
    int idx = rand() % len;
    signal[idx] = (rand() % 2) ? 5.0f : -5.0f; // Large spike
  }
}

static void demo_ema(void) {
  if (!json_mode) {
    ascii_section("1. Exponential Moving Average (EMA)");
    printf("  Use case: Smooth sensor readings (temperature, voltage)\n");
    printf("  Property: More recent samples have higher weight\n\n");
  }

  float raw[NUM_SAMPLES];
  float filtered[NUM_SAMPLES];

  generate_noisy_signal(raw, NUM_SAMPLES, 0.5f);

  eif_ema_t ema;
  eif_ema_init(&ema, 0.2f); // alpha = 0.2 (smooth)

  for (int i = 0; i < NUM_SAMPLES; i++) {
    filtered[i] = eif_ema_update(&ema, raw[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"ema\", \"alpha\": 0.2, \"input\": [");
    for (int i = 0; i < NUM_SAMPLES; i++)
      printf("%.3f%s", raw[i], i < NUM_SAMPLES - 1 ? "," : "");
    printf("], \"output\": [");
    for (int i = 0; i < NUM_SAMPLES; i++)
      printf("%.3f%s", filtered[i], i < NUM_SAMPLES - 1 ? "," : "");
    printf("]}\n");
  } else {
    printf("  Raw signal (noisy):\n");
    ascii_plot_waveform("Raw", raw, NUM_SAMPLES, 50, 5);
    printf("\n  EMA filtered (alpha=0.2):\n");
    ascii_plot_waveform("Filtered", filtered, NUM_SAMPLES, 50, 5);
  }
}

static void demo_median(void) {
  if (!json_mode) {
    ascii_section("2. Median Filter");
    printf("  Use case: Remove impulse noise (spikes)\n");
    printf("  Property: Preserves edges while removing outliers\n\n");
  }

  float raw[NUM_SAMPLES];
  float filtered[NUM_SAMPLES];

  // Generate clean sine and add spikes
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float t = (float)i / NUM_SAMPLES * 4 * M_PI;
    raw[i] = sinf(t);
  }
  add_spikes(raw, NUM_SAMPLES, 5);

  eif_median_t mf;
  eif_median_init(&mf, 5); // 5-sample window

  for (int i = 0; i < NUM_SAMPLES; i++) {
    filtered[i] = eif_median_update(&mf, raw[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"median\", \"window\": 5, \"spikes_removed\": 5}\n");
  } else {
    printf("  Signal with spikes:\n");
    ascii_plot_waveform("Spiked", raw, NUM_SAMPLES, 50, 5);
    printf("\n  Median filtered (window=5):\n");
    ascii_plot_waveform("Cleaned", filtered, NUM_SAMPLES, 50, 5);
    printf("\n  Note: Spikes are completely removed!\n");
  }
}

static void demo_rate_limiter(void) {
  if (!json_mode) {
    ascii_section("3. Rate Limiter (Slew Control)");
    printf("  Use case: Smooth motor/servo control, prevent jarring motion\n");
    printf("  Property: Limits how fast output can change\n\n");
  }

  float target[NUM_SAMPLES];
  float output[NUM_SAMPLES];

  // Step input: 0 -> 10 -> 0
  for (int i = 0; i < NUM_SAMPLES; i++) {
    if (i < 10)
      target[i] = 0.0f;
    else if (i < 25)
      target[i] = 10.0f;
    else if (i < 40)
      target[i] = 0.0f;
    else
      target[i] = 5.0f;
  }

  eif_rate_limiter_t rl;
  eif_rate_limiter_init(&rl, 0.5f); // Max 0.5 per sample

  for (int i = 0; i < NUM_SAMPLES; i++) {
    output[i] = eif_rate_limiter_update(&rl, target[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"rate_limiter\", \"max_rate\": 0.5}\n");
  } else {
    printf("  Target (step changes):\n");
    ascii_plot_waveform("Target", target, NUM_SAMPLES, 50, 5);
    printf("\n  Rate limited output (max=0.5/sample):\n");
    ascii_plot_waveform("Smoothed", output, NUM_SAMPLES, 50, 5);
    printf("\n  Note: Sharp steps become smooth ramps!\n");
  }
}

static void demo_hysteresis(void) {
  if (!json_mode) {
    ascii_section("4. Hysteresis (Schmitt Trigger)");
    printf("  Use case: Thermostat control, level detection\n");
    printf("  Property: Two thresholds prevent rapid toggling\n\n");
  }

  float input[NUM_SAMPLES];
  float output[NUM_SAMPLES];

  // Noisy signal around threshold
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float t = (float)i / NUM_SAMPLES * 2 * M_PI;
    input[i] = 5.0f + 3.0f * sinf(t) + ((float)rand() / RAND_MAX - 0.5f);
  }

  eif_hysteresis_t hyst;
  eif_hysteresis_init(&hyst, 4.0f, 6.0f); // Low < 4, High > 6

  for (int i = 0; i < NUM_SAMPLES; i++) {
    output[i] = eif_hysteresis_update(&hyst, input[i]) ? 8.0f : 2.0f;
  }

  if (json_mode) {
    printf("{\"demo\": \"hysteresis\", \"low_threshold\": 4.0, "
           "\"high_threshold\": 6.0}\n");
  } else {
    printf("  Input signal (oscillating near threshold):\n");
    ascii_plot_waveform("Input", input, NUM_SAMPLES, 50, 5);
    printf("\n  Hysteresis output (thresholds: 4 and 6):\n");
    ascii_plot_waveform("State", output, NUM_SAMPLES, 50, 5);
    printf("\n  Note: Clean switching without oscillation!\n");
  }
}

static void demo_debounce(void) {
  if (!json_mode) {
    ascii_section("5. Debounce Filter");
    printf("  Use case: Button/switch input filtering\n");
    printf("  Property: Requires N consecutive samples to change state\n\n");
  }

  // Simulate bouncy button press
  bool raw_button[NUM_SAMPLES];
  bool debounced[NUM_SAMPLES];

  // Pattern: 10 low, 5 bouncy, 20 high, 5 bouncy, 10 low
  for (int i = 0; i < NUM_SAMPLES; i++) {
    if (i < 10)
      raw_button[i] = false;
    else if (i < 15)
      raw_button[i] = (i % 2 == 0); // Bouncing
    else if (i < 35)
      raw_button[i] = true;
    else if (i < 40)
      raw_button[i] = (i % 2 == 0); // Bouncing
    else
      raw_button[i] = false;
  }

  eif_debounce_t db;
  eif_debounce_init(&db, 3); // Need 3 consecutive samples

  for (int i = 0; i < NUM_SAMPLES; i++) {
    debounced[i] = eif_debounce_update(&db, raw_button[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"debounce\", \"threshold\": 3}\n");
  } else {
    // Print as text timeline
    printf("  Raw button (with bounce):\n    ");
    for (int i = 0; i < NUM_SAMPLES; i++)
      printf("%c", raw_button[i] ? '1' : '0');
    printf("\n\n  Debounced output (threshold=3):\n    ");
    for (int i = 0; i < NUM_SAMPLES; i++)
      printf("%c", debounced[i] ? '1' : '0');
    printf("\n\n  Note: Bounces are filtered out!\n");
  }
}

int main(int argc, char **argv) {
  // Parse CLI
  demo_cli_result_t cli_result =
      demo_parse_args(argc, argv, "smooth_filters_demo",
                      "Demonstrates signal smoothing and filtering algorithms");

  if (cli_result == DEMO_EXIT) {
    return 0;
  }

  // Check for JSON mode
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0) {
      json_mode = true;
      break;
    }
  }

  srand(time(NULL));

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Signal Smoothing Filters Demo");
    printf(
        "  This demo showcases the smoothing filters in eif_dsp_smooth.h\n\n");
  }

  demo_ema();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_median();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_rate_limiter();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_hysteresis();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_debounce();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  Filters demonstrated:\n");
    printf("    • EMA         - Smooth sensor noise\n");
    printf("    • Median      - Remove spikes/outliers\n");
    printf("    • Rate Limiter - Limit change rate\n");
    printf("    • Hysteresis  - Clean switching\n");
    printf("    • Debounce    - Filter button bounce\n\n");
    printf("  All filters are header-only and embedded-friendly!\n\n");
  }

  return 0;
}
