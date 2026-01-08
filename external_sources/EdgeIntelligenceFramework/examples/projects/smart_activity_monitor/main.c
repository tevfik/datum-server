/**
 * @file main.c
 * @brief Smart Activity Monitor - Complete End-to-End Demo
 *
 * This file demonstrates the complete EIF workflow:
 * 1. Sensor data acquisition (simulated)
 * 2. Feature extraction
 * 3. Classification
 * 4. Action/output
 *
 * Build with: cmake .. && make smart_activity_demo
 * Run with: ./bin/smart_activity_demo --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// EIF includes
#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"
#include "eif_activity.h"   // Activity recognition
#include "eif_dsp_smooth.h" // EMA smoothing

// =============================================================================
// Configuration
// =============================================================================

#define SAMPLE_RATE_HZ 50         // 50 Hz sampling
#define WINDOW_SIZE 128           // 2.56 seconds of data
#define WINDOW_HOP 32             // Classify every 0.64 seconds
#define CONFIDENCE_THRESHOLD 0.6f // Minimum confidence for prediction

// =============================================================================
// Simulated Sensor Data
// =============================================================================

// Simulate accelerometer data for different activities
static void simulate_activity(eif_activity_t activity, float *ax, float *ay,
                              float *az) {
  static unsigned int seed = 12345;
  seed = seed * 1103515245 + 12345;
  float noise = ((seed % 1000) / 1000.0f - 0.5f) * 0.3f;
  float t = (seed % 1000) / 100.0f;

  switch (activity) {
  case EIF_ACTIVITY_STATIONARY:
    *ax = 0.0f + noise * 0.1f;
    *ay = 0.0f + noise * 0.1f;
    *az = 9.81f + noise * 0.2f;
    break;

  case EIF_ACTIVITY_WALKING:
    *ax = 0.5f * sinf(t * 2.0f) + noise;
    *ay = 0.3f * sinf(t * 4.0f) + noise;
    *az = 9.81f + 1.0f * fabsf(sinf(t * 4.0f)) + noise;
    break;

  case EIF_ACTIVITY_RUNNING:
    *ax = 1.5f * sinf(t * 3.0f) + noise * 2;
    *ay = 1.0f * sinf(t * 6.0f) + noise * 2;
    *az = 9.81f + 3.0f * fabsf(sinf(t * 6.0f)) + noise * 2;
    break;

  case EIF_ACTIVITY_STAIRS_UP:
    *ax = 0.3f * sinf(t * 1.5f) + noise;
    *ay = 0.2f * sinf(t * 3.0f) + noise;
    *az = 9.81f + 1.5f * sinf(t * 1.5f) + noise;
    break;

  default:
    *ax = 0.0f;
    *ay = 0.0f;
    *az = 9.81f;
  }
}

// =============================================================================
// Statistics for Demo
// =============================================================================

typedef struct {
  int predictions[EIF_NUM_ACTIVITIES];
  int total;
  float confidence_sum;
} demo_stats_t;

static void stats_init(demo_stats_t *stats) {
  memset(stats, 0, sizeof(*stats));
}

static void stats_add(demo_stats_t *stats, eif_activity_t activity,
                      float confidence) {
  if (activity < EIF_NUM_ACTIVITIES) {
    stats->predictions[activity]++;
  }
  stats->total++;
  stats->confidence_sum += confidence;
}

static void stats_print(demo_stats_t *stats) {
  printf("\n  === Session Statistics ===\n\n");
  printf("  Activity         Count    Percent\n");
  printf("  ─────────────────────────────────\n");

  for (int i = 0; i < EIF_NUM_ACTIVITIES; i++) {
    if (stats->predictions[i] > 0) {
      float pct = 100.0f * stats->predictions[i] / stats->total;
      printf("  %-15s  %5d    %5.1f%%\n", eif_activity_names[i],
             stats->predictions[i], pct);
    }
  }

  printf("  ─────────────────────────────────\n");
  printf("  Total: %d predictions\n", stats->total);
  printf("  Average confidence: %.1f%%\n",
         100.0f * stats->confidence_sum / stats->total);
}

// =============================================================================
// Main Application
// =============================================================================

static void run_demo(bool batch_mode) {
  printf("\n");
  ascii_section("Smart Activity Monitor");
  printf("  Complete end-to-end activity recognition demo\n");
  printf("  Simulating accelerometer data → classification\n\n");

  // Initialize components
  eif_activity_window_t window;
  eif_activity_window_init(&window, WINDOW_HOP);

  eif_ema_t smooth_x, smooth_y, smooth_z;
  eif_ema_init(&smooth_x, 0.3f);
  eif_ema_init(&smooth_y, 0.3f);
  eif_ema_init(&smooth_z, 0.3f);

  demo_stats_t stats;
  stats_init(&stats);

  // Simulate different activities
  eif_activity_t activities[] = {
      EIF_ACTIVITY_STATIONARY, EIF_ACTIVITY_WALKING,   EIF_ACTIVITY_WALKING,
      EIF_ACTIVITY_RUNNING,    EIF_ACTIVITY_RUNNING,   EIF_ACTIVITY_STAIRS_UP,
      EIF_ACTIVITY_WALKING,    EIF_ACTIVITY_STATIONARY};
  int num_activities = sizeof(activities) / sizeof(activities[0]);

  printf("  Simulating %d activity transitions...\n\n", num_activities);
  printf("  Time(s)  Activity         Confidence  Features\n");
  printf("  ───────────────────────────────────────────────────────\n");

  int sample_count = 0;
  float sim_time = 0;

  for (int act_idx = 0; act_idx < num_activities; act_idx++) {
    eif_activity_t true_activity = activities[act_idx];

    // Simulate samples for this activity (3 seconds each)
    for (int s = 0; s < 150; s++) {
      float ax, ay, az;
      simulate_activity(true_activity, &ax, &ay, &az);

      // Apply smoothing
      ax = eif_ema_update(&smooth_x, ax);
      ay = eif_ema_update(&smooth_y, ay);
      az = eif_ema_update(&smooth_z, az);

      // Add to window
      if (eif_activity_window_add(&window, ax, ay, az)) {
        // Window full - classify!
        eif_accel_sample_t samples[EIF_ACTIVITY_WINDOW_SIZE];
        eif_activity_features_t features;

        eif_activity_window_get_samples(&window, samples);
        eif_activity_extract_features(samples, EIF_ACTIVITY_WINDOW_SIZE,
                                      &features);

        eif_activity_t predicted = eif_activity_classify_rules(&features);

        // Estimate confidence from feature clarity
        float confidence = 1.0f - (features.magnitude_std > 3.0f
                                       ? 0.5f
                                       : features.magnitude_std / 6.0f);
        if (confidence < 0.5f)
          confidence = 0.5f;

        bool correct = (predicted == true_activity);

        printf("  %6.1f   %-15s  %5.1f%%   mag_std=%.2f %s\n", sim_time,
               eif_activity_names[predicted], confidence * 100.0f,
               features.magnitude_std, correct ? "✓" : "✗");

        stats_add(&stats, predicted, confidence);
      }

      sample_count++;
      sim_time = sample_count / (float)SAMPLE_RATE_HZ;
    }
  }

  printf("  ───────────────────────────────────────────────────────\n");

  stats_print(&stats);

  // Print feature extraction details
  ascii_section("Feature Extraction Details");
  printf("  Window size: %d samples (%.1f seconds)\n", WINDOW_SIZE,
         WINDOW_SIZE / (float)SAMPLE_RATE_HZ);
  printf("  Window hop: %d samples (%.2f seconds)\n", WINDOW_HOP,
         WINDOW_HOP / (float)SAMPLE_RATE_HZ);
  printf(
      "  Features extracted: 14 (mean, std, magnitude, SMA, energy, etc.)\n");
  printf("  Classifier: Rule-based (fast, no training data required)\n\n");

  // Memory usage
  ascii_section("Memory Usage");
  printf("  Activity window: %zu bytes\n", sizeof(eif_activity_window_t));
  printf("  Features struct: %zu bytes\n", sizeof(eif_activity_features_t));
  printf("  EMA smoothers (3): %zu bytes\n", 3 * sizeof(eif_ema_t));
  printf("  Total RAM: %zu bytes\n\n", sizeof(eif_activity_window_t) +
                                           sizeof(eif_activity_features_t) +
                                           3 * sizeof(eif_ema_t));
}

// =============================================================================
// Entry Point
// =============================================================================

int main(int argc, char **argv) {
  demo_cli_result_t result = demo_parse_args(
      argc, argv, "smart_activity_demo", "Complete Activity Recognition Demo");

  if (result == DEMO_EXIT)
    return 0;

  run_demo(demo_is_batch_mode());

  ascii_section("Summary");
  printf("  This demo shows the complete EIF workflow:\n");
  printf("    1. Sensor data acquisition (simulated)\n");
  printf("    2. Signal smoothing (EMA filter)\n");
  printf("    3. Windowed feature extraction\n");
  printf("    4. Activity classification\n\n");
  printf("  To train a custom model:\n");
  printf("    python train_model.py --data your_data.csv\n\n");

  return 0;
}
