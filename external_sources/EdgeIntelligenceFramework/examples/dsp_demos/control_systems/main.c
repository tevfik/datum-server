/**
 * @file main.c
 * @brief Control Systems Demo
 *
 * Demonstrates control system utilities:
 * - Deadzone for joystick/input centering
 * - Differentiator for rate-of-change detection
 * - Integrator for accumulation (with anti-windup)
 * - Zero-crossing for frequency detection
 * - Peak detector for envelope following
 *
 * Usage:
 *   ./control_systems_demo                  # Interactive
 *   ./control_systems_demo --json           # JSON output
 *   ./control_systems_demo --batch          # No prompts
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/ascii_plot.h"
#include "../common/demo_cli.h"
#include "eif_dsp_control.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_SAMPLES 60
static bool json_mode = false;

// Demo: Deadzone for joystick input
static void demo_deadzone(void) {
  if (!json_mode) {
    ascii_section("1. Deadzone Filter (Joystick Centering)");
    printf("  Use case: Remove jitter near center position\n");
    printf("  Eliminates false inputs from noisy analog sticks\n\n");
  }

  eif_deadzone_t dz;
  eif_deadzone_init(&dz, 0.15f); // 15% deadzone

  float raw[NUM_SAMPLES];
  float processed[NUM_SAMPLES];

  // Simulate joystick with jitter near center
  for (int i = 0; i < NUM_SAMPLES; i++) {
    if (i < 15) {
      raw[i] = 0.05f * sinf(i * 0.5f); // Small jitter
    } else if (i < 35) {
      raw[i] = 0.6f + 0.1f * sinf(i * 0.3f); // Intentional right
    } else {
      raw[i] = -0.02f + 0.03f * sinf(i * 0.7f); // Return to center
    }
    processed[i] = eif_deadzone_apply(&dz, raw[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"deadzone\", \"threshold\": 0.15}\n");
  } else {
    printf("  Raw joystick (with center jitter):\n");
    ascii_plot_waveform("Raw", raw, NUM_SAMPLES, 50, 4);
    printf("\n  After deadzone (clean center):\n");
    ascii_plot_waveform("Clean", processed, NUM_SAMPLES, 50, 4);
    printf("\n  Note: Small movements near center are eliminated!\n");
  }
}

// Demo: Differentiator for motion detection
static void demo_differentiator(void) {
  if (!json_mode) {
    ascii_section("2. Differentiator (Rate of Change)");
    printf("  Use case: Detect sudden motion changes\n");
    printf("  Compute velocity from position, acceleration from velocity\n\n");
  }

  eif_differentiator_t diff;
  eif_differentiator_init(&diff, 50.0f); // 50 Hz sample rate

  float position[NUM_SAMPLES];
  float velocity[NUM_SAMPLES];

  // Smooth position with sudden change
  for (int i = 0; i < NUM_SAMPLES; i++) {
    if (i < 20) {
      position[i] = 0.0f;
    } else if (i < 40) {
      position[i] = (float)(i - 20) / 20.0f; // Ramp up
    } else {
      position[i] = 1.0f;
    }
    velocity[i] = eif_differentiator_update(&diff, position[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"differentiator\", \"sample_rate\": 50}\n");
  } else {
    printf("  Position (with ramp):\n");
    ascii_plot_waveform("Position", position, NUM_SAMPLES, 50, 4);
    printf("\n  Velocity (derivative):\n");
    ascii_plot_waveform("Velocity", velocity, NUM_SAMPLES, 50, 4);
    printf("\n  Note: Velocity is high during motion, zero when stationary!\n");
  }
}

// Demo: Integrator for energy accumulation
static void demo_integrator(void) {
  if (!json_mode) {
    ascii_section("3. Integrator (Accumulation)");
    printf("  Use case: Energy metering, position from velocity\n");
    printf("  With anti-windup to prevent overflow\n\n");
  }

  eif_integrator_t integ;
  eif_integrator_init(&integ, 50.0f, 0.0f, 10.0f); // 50 Hz, limit [0, 10]

  float power[NUM_SAMPLES];
  float energy[NUM_SAMPLES];

  // Variable power consumption
  for (int i = 0; i < NUM_SAMPLES; i++) {
    if (i < 20) {
      power[i] = 1.0f; // Low power
    } else if (i < 40) {
      power[i] = 5.0f; // High power
    } else {
      power[i] = 0.5f; // Low again
    }
    energy[i] = eif_integrator_update(&integ, power[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"integrator\", \"limit\": 10.0}\n");
  } else {
    printf("  Power consumption:\n");
    ascii_plot_waveform("Power", power, NUM_SAMPLES, 50, 4);
    printf("\n  Accumulated energy (clamped at 10):\n");
    ascii_plot_waveform("Energy", energy, NUM_SAMPLES, 50, 4);
    printf("\n  Note: Energy accumulates but stops at limit!\n");
  }
}

// Demo: Zero-crossing for frequency detection
static void demo_zero_crossing(void) {
  if (!json_mode) {
    ascii_section("4. Zero-Crossing Detector");
    printf("  Use case: Frequency measurement, phase detection\n");
    printf("  Count sign changes in signal\n\n");
  }

  eif_zero_cross_t zc;
  eif_zero_cross_init(&zc);

  float signal[NUM_SAMPLES];
  int crossings = 0;

  // Sine wave
  for (int i = 0; i < NUM_SAMPLES; i++) {
    signal[i] = sinf(i * 0.3f); // ~5 cycles
    int result = eif_zero_cross_update(&zc, signal[i]);
    if (result != 0)
      crossings++;
  }

  if (json_mode) {
    printf("{\"demo\": \"zero_crossing\", \"crossings\": %d}\n", crossings);
  } else {
    printf("  Sine wave:\n");
    ascii_plot_waveform("Signal", signal, NUM_SAMPLES, 50, 4);
    printf("\n  Zero crossings detected: %d\n", crossings);
    printf("  (Each cycle has 2 crossings: rising and falling)\n");
  }
}

// Demo: Peak detector for audio envelope
static void demo_peak_detector(void) {
  if (!json_mode) {
    ascii_section("5. Peak Detector (Envelope Follower)");
    printf("  Use case: Audio level meter, vibration monitoring\n");
    printf("  Fast attack, slow decay\n\n");
  }

  eif_peak_detector_t pd;
  eif_peak_detector_init(&pd, 2.0f, 50.0f, 100.0f); // 2ms attack, 50ms decay

  float audio[NUM_SAMPLES];
  float envelope[NUM_SAMPLES];

  // Simulate audio bursts
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float burst = 0.0f;
    if (i >= 5 && i < 15)
      burst = 0.8f;
    else if (i >= 30 && i < 45)
      burst = 1.0f;

    audio[i] = burst * sinf(i * 2.0f); // Modulated carrier
    envelope[i] = eif_peak_detector_update(&pd, audio[i]);
  }

  if (json_mode) {
    printf("{\"demo\": \"peak_detector\", \"attack_ms\": 2.0, \"decay_ms\": "
           "50.0}\n");
  } else {
    printf("  Audio signal (bursts):\n");
    ascii_plot_waveform("Audio", audio, NUM_SAMPLES, 50, 4);
    printf("\n  Envelope (peak detector):\n");
    ascii_plot_waveform("Envelope", envelope, NUM_SAMPLES, 50, 4);
    printf("\n  Note: Follows peaks with smooth decay!\n");
  }
}

int main(int argc, char **argv) {
  // Parse CLI
  demo_cli_result_t cli_result = demo_parse_args(
      argc, argv, "control_systems_demo",
      "Demonstrates control system utilities: deadzone, differentiator, "
      "integrator, zero-crossing, peak detector");

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
    ascii_section("EIF Control Systems Demo");
    printf("  Demonstrates control utilities from eif_dsp_control.h\n\n");
  }

  demo_deadzone();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_differentiator();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_integrator();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_zero_crossing();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_peak_detector();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  Control utilities demonstrated:\n");
    printf("    • Deadzone      - Clean joystick/input centering\n");
    printf("    • Differentiator - Rate of change detection\n");
    printf("    • Integrator    - Accumulation with limits\n");
    printf("    • Zero-Crossing - Frequency/phase detection\n");
    printf("    • Peak Detector - Envelope following\n\n");
    printf("  All utilities are header-only and embedded-friendly!\n\n");
  }

  return 0;
}
