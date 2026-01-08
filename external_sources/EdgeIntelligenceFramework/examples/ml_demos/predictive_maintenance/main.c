/**
 * @file main.c
 * @brief Predictive Maintenance Demo
 *
 * Demonstrates industrial IoT predictive maintenance:
 * - Health indicator monitoring
 * - RUL (Remaining Useful Life) estimation
 * - Vibration analysis
 * - Maintenance recommendations
 *
 * Usage:
 *   ./predictive_maintenance_demo --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"
#include "eif_predictive_maintenance.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool json_mode = false;

// Health state names
static const char *health_names[] = {"GOOD", "FAIR", "WARNING", "CRITICAL",
                                     "FAILURE"};

// Demo: Health indicator tracking
static void demo_health_indicator(void) {
  if (!json_mode) {
    ascii_section("1. Health Indicator Monitoring");
    printf("  Tracking equipment health over time\n\n");
  }

  eif_health_indicator_t hi;
  eif_health_init(&hi, 1.0f, 0.2f,
                  0.5f); // baseline=1.0, warn at 20%, critical at 50%

  // Simulate degrading equipment
  float readings[] = {
      1.02f, 1.01f, 1.03f, 1.05f, 1.08f, // Normal
      1.12f, 1.15f, 1.18f, 1.22f, 1.25f, // Starting to degrade
      1.30f, 1.35f, 1.42f, 1.50f, 1.58f  // Significant degradation
  };

  if (!json_mode) {
    printf("  Day   Reading   Health    State      Trend\n");
    printf("  ----  --------  --------  ---------  -----\n");
  }

  for (int i = 0; i < 15; i++) {
    eif_health_state_t state = eif_health_update(&hi, readings[i]);
    float norm_health = eif_health_get_normalized(&hi);

    if (!json_mode) {
      printf("  %3d   %7.2f   %6.1f%%   %-9s  %+.3f\n", i + 1, readings[i],
             norm_health * 100.0f, health_names[state], hi.trend_slope);
    }
  }
}

// Demo: RUL estimation
static void demo_rul_estimation(void) {
  if (!json_mode) {
    ascii_section("2. Remaining Useful Life (RUL)");
    printf("  Predicting time to failure\n\n");
  }

  eif_rul_estimator_t rul;
  eif_rul_init(&rul, 2.0f, 1.0f); // failure at 2.0, 1 day sampling

  // Simulate slow degradation over 30 days
  srand(42);
  for (int day = 0; day < 30; day++) {
    float base = 0.5f + (float)day * 0.03f; // Linear degradation
    float noise = 0.02f * ((float)rand() / RAND_MAX - 0.5f);
    eif_rul_update(&rul, base + noise);
  }

  float estimated_rul = eif_rul_estimate(&rul);

  if (!json_mode) {
    printf("  Degradation data collected: %d days\n", rul.history_count);
    printf("  Current degradation level: %.2f\n",
           rul.history[(rul.history_idx - 1 + EIF_PM_HISTORY_SIZE) %
                       EIF_PM_HISTORY_SIZE]);
    printf("  Failure threshold: %.2f\n", rul.failure_threshold);
    printf("\n  Estimated RUL: %.1f days\n", estimated_rul);

    if (estimated_rul > 0) {
      printf("\n  ⚠️ Schedule maintenance within %.0f days\n", estimated_rul);
    } else if (estimated_rul == 0) {
      printf("\n  🚨 CRITICAL: Failure imminent!\n");
    } else {
      printf("\n  ✓ Health stable or improving\n");
    }
  }
}

// Demo: Vibration analysis
static void demo_vibration_analysis(void) {
  if (!json_mode) {
    ascii_section("3. Vibration Analysis");
    printf("  Bearing health from vibration metrics\n\n");
  }

  // Generate sample vibration signals
  float healthy[256], worn[256], damaged[256];
  srand(42);

  for (int i = 0; i < 256; i++) {
    float t = (float)i / 256.0f;

    // Healthy: smooth rotation
    healthy[i] = 0.5f * sinf(2 * M_PI * 10 * t) +
                 0.1f * ((float)rand() / RAND_MAX - 0.5f);

    // Worn: increased amplitude, still smooth
    worn[i] = 1.2f * sinf(2 * M_PI * 10 * t) +
              0.3f * ((float)rand() / RAND_MAX - 0.5f);

    // Damaged: spiky impulses
    damaged[i] = 1.0f * sinf(2 * M_PI * 10 * t);
    if ((i % 25) == 0) { // Impact spikes
      damaged[i] += 4.0f * ((float)rand() / RAND_MAX);
    }
    damaged[i] += 0.5f * ((float)rand() / RAND_MAX - 0.5f);
  }

  // Analyze each signal
  const char *conditions[] = {"Healthy", "Worn", "Damaged"};
  float *signals[] = {healthy, worn, damaged};

  if (!json_mode) {
    printf("  %-10s  %-6s  %-6s  %-6s  %-7s  %s\n", "Condition", "RMS", "Peak",
           "Crest", "Kurt", "Diagnosis");
    printf("  %-10s  %-6s  %-6s  %-6s  %-7s  %s\n", "---------", "---", "----",
           "-----", "----", "---------");
  }

  for (int i = 0; i < 3; i++) {
    eif_vibration_metrics_t metrics;
    eif_vibration_analyze(signals[i], 256, &metrics);
    eif_health_state_t state = eif_vibration_diagnose(&metrics);

    if (!json_mode) {
      printf("  %-10s  %5.2f  %5.2f  %5.2f  %6.2f  %s\n", conditions[i],
             metrics.rms, metrics.peak, metrics.crest_factor, metrics.kurtosis,
             health_names[state]);
    }
  }
}

// Demo: Maintenance scheduling
static void demo_maintenance(void) {
  if (!json_mode) {
    ascii_section("4. Maintenance Scheduling");
    printf("  Generating actionable recommendations\n\n");
  }

  // Simulate different equipment states
  struct {
    const char *equipment;
    float baseline;
    float current;
    float degrade_rate;
  } equipment[] = {
      {"Pump A", 1.0f, 1.05f, 0.001f},  // Healthy
      {"Motor B", 1.0f, 1.25f, 0.005f}, // Degrading
      {"Fan C", 1.0f, 1.55f, 0.02f}     // Critical
  };

  for (int e = 0; e < 3; e++) {
    eif_health_indicator_t health;
    eif_health_init(&health, equipment[e].baseline, 0.2f, 0.5f);

    // Update with current reading
    for (int i = 0; i < 10; i++) {
      float value = equipment[e].baseline +
                    (equipment[e].current - equipment[e].baseline) * (i / 9.0f);
      eif_health_update(&health, value);
    }

    // RUL based on degradation rate
    eif_rul_estimator_t rul;
    eif_rul_init(&rul, 2.0f, 1.0f);
    for (int i = 0; i < 20; i++) {
      float value = equipment[e].current + i * equipment[e].degrade_rate;
      eif_rul_update(&rul, value);
    }

    eif_maintenance_rec_t rec;
    eif_maintenance_recommend(&health, &rul, &rec);

    if (!json_mode) {
      printf("  📊 %s\n", equipment[e].equipment);
      printf("     Health: %s (%.0f%%)\n", health_names[rec.health],
             eif_health_get_normalized(&health) * 100);
      printf("     RUL: %.1f days\n", rec.rul_estimate);
      printf("     Urgency: %d/5\n", rec.urgency);
      printf("     Action: %s\n\n", rec.action);
    }
  }
}

int main(int argc, char **argv) {
  demo_cli_result_t result =
      demo_parse_args(argc, argv, "predictive_maintenance_demo",
                      "Industrial IoT Predictive Maintenance");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Predictive Maintenance Demo");
    printf("  Condition monitoring for industrial equipment\n\n");
  }

  demo_health_indicator();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_rul_estimation();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_vibration_analysis();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_maintenance();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  Predictive Maintenance features:\n");
    printf("    • Health indicator with trend tracking\n");
    printf("    • RUL estimation via linear extrapolation\n");
    printf("    • Vibration analysis (RMS, crest, kurtosis)\n");
    printf("    • Automated maintenance recommendations\n");
    printf("    • Industrial IoT ready\n\n");
  }

  return 0;
}
