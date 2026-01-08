/**
 * @file main.c
 * @brief Activity Recognition Demo
 *
 * Demonstrates Human Activity Recognition:
 * - Feature extraction from IMU data
 * - Rule-based classification
 * - Sliding window processing
 *
 * Usage:
 *   ./activity_recognition_demo --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"
#include "eif_activity.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool json_mode = false;

// Generate simulated IMU data for activity
static void generate_activity_data(eif_activity_t activity,
                                   eif_accel_sample_t *samples, int count) {
  for (int i = 0; i < count; i++) {
    float t = (float)i / 50.0f; // 50 Hz

    switch (activity) {
    case EIF_ACTIVITY_STATIONARY:
      samples[i].x = 0.1f * ((float)rand() / RAND_MAX - 0.5f);
      samples[i].y = 0.1f * ((float)rand() / RAND_MAX - 0.5f);
      samples[i].z = 9.8f + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
      break;

    case EIF_ACTIVITY_WALKING:
      samples[i].x = 0.5f * sinf(2.0f * M_PI * 2.0f * t);
      samples[i].y = 0.3f * sinf(2.0f * M_PI * 2.0f * t + 1.0f);
      samples[i].z = 9.8f + 1.5f * sinf(2.0f * M_PI * 2.0f * t);
      break;

    case EIF_ACTIVITY_RUNNING:
      samples[i].x = 2.0f * sinf(2.0f * M_PI * 3.5f * t);
      samples[i].y = 1.5f * sinf(2.0f * M_PI * 3.5f * t + 1.0f);
      samples[i].z = 9.8f + 4.0f * sinf(2.0f * M_PI * 3.5f * t);
      break;

    case EIF_ACTIVITY_STAIRS_UP:
      samples[i].x = 0.4f * sinf(2.0f * M_PI * 1.5f * t);
      samples[i].y = 0.3f * sinf(2.0f * M_PI * 1.5f * t);
      samples[i].z = 10.5f + 0.8f * sinf(2.0f * M_PI * 1.5f * t);
      break;

    case EIF_ACTIVITY_CYCLING:
      samples[i].x = 0.3f * sinf(2.0f * M_PI * 1.2f * t);
      samples[i].y = 0.8f * sinf(2.0f * M_PI * 1.2f * t);
      samples[i].z = 9.8f + 0.5f * sinf(2.0f * M_PI * 1.2f * t);
      break;

    default:
      samples[i].x = ((float)rand() / RAND_MAX - 0.5f);
      samples[i].y = ((float)rand() / RAND_MAX - 0.5f);
      samples[i].z = 9.8f;
    }

    // Add noise
    samples[i].x += 0.05f * ((float)rand() / RAND_MAX - 0.5f);
    samples[i].y += 0.05f * ((float)rand() / RAND_MAX - 0.5f);
    samples[i].z += 0.05f * ((float)rand() / RAND_MAX - 0.5f);
  }
}

// Demo: Feature extraction
static void demo_feature_extraction(void) {
  if (!json_mode) {
    ascii_section("1. Feature Extraction");
    printf("  Extracting time-domain features from IMU\n\n");
  }

  eif_activity_t activities[] = {EIF_ACTIVITY_STATIONARY, EIF_ACTIVITY_WALKING,
                                 EIF_ACTIVITY_RUNNING};

  if (!json_mode) {
    printf("  %-12s  %-8s  %-8s  %-8s  %-8s\n", "Activity", "Mag Mean",
           "Mag Std", "SMA", "ZCR");
    printf("  %-12s  %-8s  %-8s  %-8s  %-8s\n", "--------", "--------",
           "-------", "---", "---");
  }

  for (int a = 0; a < 3; a++) {
    eif_accel_sample_t samples[EIF_ACTIVITY_WINDOW_SIZE];
    generate_activity_data(activities[a], samples, EIF_ACTIVITY_WINDOW_SIZE);

    eif_activity_features_t features;
    eif_activity_extract_features(samples, EIF_ACTIVITY_WINDOW_SIZE, &features);

    if (!json_mode) {
      printf("  %-12s  %8.2f  %8.2f  %8.2f  %8.3f\n",
             eif_activity_names[activities[a]], features.magnitude_mean,
             features.magnitude_std, features.sma, features.zero_crossings);
    }
  }
}

// Demo: Classification
static void demo_classification(void) {
  if (!json_mode) {
    ascii_section("2. Activity Classification");
    printf("  Rule-based classification from features\n\n");
  }

  eif_activity_t test_activities[] = {
      EIF_ACTIVITY_STATIONARY, EIF_ACTIVITY_WALKING, EIF_ACTIVITY_RUNNING,
      EIF_ACTIVITY_STAIRS_UP, EIF_ACTIVITY_CYCLING};

  if (!json_mode) {
    printf("  %-15s  %-15s  %s\n", "True Activity", "Predicted", "Match");
    printf("  %-15s  %-15s  %s\n", "-------------", "---------", "-----");
  }

  int correct = 0;
  for (int a = 0; a < 5; a++) {
    eif_accel_sample_t samples[EIF_ACTIVITY_WINDOW_SIZE];
    generate_activity_data(test_activities[a], samples,
                           EIF_ACTIVITY_WINDOW_SIZE);

    eif_activity_features_t features;
    eif_activity_extract_features(samples, EIF_ACTIVITY_WINDOW_SIZE, &features);

    eif_activity_t predicted = eif_activity_classify_rules(&features);
    bool match = (predicted == test_activities[a]);
    if (match)
      correct++;

    if (!json_mode) {
      printf("  %-15s  %-15s  %s\n", eif_activity_names[test_activities[a]],
             eif_activity_names[predicted], match ? "✓" : "✗");
    }
  }

  if (!json_mode) {
    printf("\n  Accuracy: %d/5 (%.0f%%)\n", correct, correct * 20.0f);
  }
}

// Demo: Streaming classification
static void demo_streaming(void) {
  if (!json_mode) {
    ascii_section("3. Streaming Classification");
    printf("  Continuous activity recognition\n\n");
  }

  eif_activity_window_t win;
  eif_activity_window_init(&win, 64); // Hop size = 64 samples

  // Simulate activity sequence: stationary -> walking -> running -> walking
  eif_activity_t sequence[] = {EIF_ACTIVITY_STATIONARY, EIF_ACTIVITY_WALKING,
                               EIF_ACTIVITY_RUNNING, EIF_ACTIVITY_WALKING};

  if (!json_mode) {
    printf("  Simulating 10 seconds at 50 Hz...\n\n");
    printf("  Time(s)  Activity Detected\n");
    printf("  -------  -----------------\n");
  }

  int sample_count = 0;
  int seq_idx = 0;

  for (int sec = 0; sec < 10; sec++) {
    // Change activity every 2.5 seconds
    if (sec >= (seq_idx + 1) * 2.5f && seq_idx < 3) {
      seq_idx++;
    }

    eif_activity_t current = sequence[seq_idx];

    for (int i = 0; i < 50; i++) { // 50 samples per second
      float t = (float)(sec * 50 + i) / 50.0f;
      float x, y, z;

      switch (current) {
      case EIF_ACTIVITY_STATIONARY:
        x = 0.1f * ((float)rand() / RAND_MAX - 0.5f);
        y = 0.1f * ((float)rand() / RAND_MAX - 0.5f);
        z = 9.8f;
        break;
      case EIF_ACTIVITY_WALKING:
        x = 0.5f * sinf(2.0f * M_PI * 2.0f * t);
        y = 0.3f * sinf(2.0f * M_PI * 2.0f * t + 1.0f);
        z = 9.8f + 1.5f * sinf(2.0f * M_PI * 2.0f * t);
        break;
      case EIF_ACTIVITY_RUNNING:
        x = 2.0f * sinf(2.0f * M_PI * 3.5f * t);
        y = 1.5f * sinf(2.0f * M_PI * 3.5f * t + 1.0f);
        z = 9.8f + 4.0f * sinf(2.0f * M_PI * 3.5f * t);
        break;
      default:
        x = y = 0;
        z = 9.8f;
      }

      if (eif_activity_window_add(&win, x, y, z)) {
        // Window ready - classify
        eif_accel_sample_t ordered[EIF_ACTIVITY_WINDOW_SIZE];
        eif_activity_window_get_samples(&win, ordered);

        eif_activity_features_t features;
        eif_activity_extract_features(ordered, EIF_ACTIVITY_WINDOW_SIZE,
                                      &features);

        eif_activity_t detected = eif_activity_classify_rules(&features);

        float time_sec = (float)sample_count / 50.0f;
        if (!json_mode) {
          printf("  %6.2f   %s\n", time_sec, eif_activity_names[detected]);
        }
      }
      sample_count++;
    }
  }
}

int main(int argc, char **argv) {
  demo_cli_result_t result =
      demo_parse_args(argc, argv, "activity_recognition_demo",
                      "Human Activity Recognition from IMU data");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  srand(42);

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Activity Recognition Demo");
    printf("  IMU-based Human Activity Recognition\n\n");
  }

  demo_feature_extraction();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_classification();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_streaming();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  Activity Recognition features:\n");
    printf("    • 15 time-domain features\n");
    printf("    • Rule-based classifier (no training needed)\n");
    printf("    • Sliding window with configurable hop\n");
    printf("    • Suitable for wearables and fitness trackers\n\n");
  }

  return 0;
}
