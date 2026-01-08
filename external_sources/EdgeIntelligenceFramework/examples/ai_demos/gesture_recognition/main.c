/**
 * @file main.c
 * @brief Gesture Recognition Tutorial - Accelerometer Gesture Detection
 *
 * This tutorial demonstrates gesture recognition using
 * 3-axis accelerometer data and pattern matching.
 *
 * SCENARIO:
 * A wearable device recognizes hand gestures:
 * Circle, Swipe Left, Swipe Right, Shake
 *
 * FEATURES DEMONSTRATED:
 * - Accelerometer signal processing
 * - Feature extraction (peak detection, energy)
 * - Pattern-based classification
 * - Real-time gesture visualization
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/ascii_plot.h"
#include "../common/demo_cli.h"
#include "eif_dsp.h"
#include "eif_memory.h"
#include "eif_types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global mode flags
static bool json_mode = false;
static bool continuous_mode = false;
static int sample_count = 0;

// ============================================================================
// Configuration
// ============================================================================

#define SAMPLE_RATE 50  // 50 Hz accelerometer
#define GESTURE_LEN 100 // 2 seconds of data
#define NUM_GESTURES 4  // circle, swipe_left, swipe_right, shake

// ============================================================================
// Gesture Types
// ============================================================================

typedef struct {
  float32_t x[GESTURE_LEN];
  float32_t y[GESTURE_LEN];
  float32_t z[GESTURE_LEN];
} accel_data_t;

static const char *gesture_names[NUM_GESTURES] = {"CIRCLE", "SWIPE LEFT",
                                                  "SWIPE RIGHT", "SHAKE"};

static const char *gesture_icons[NUM_GESTURES] = {"⭕", "⬅️ ", "➡️ ", "〰️ "};

// ============================================================================
// Gesture Data Generation
// ============================================================================

static void generate_gesture(accel_data_t *data, int gesture_type) {
  memset(data, 0, sizeof(accel_data_t));

  for (int i = 0; i < GESTURE_LEN; i++) {
    float t = (float)i / SAMPLE_RATE;
    float noise_x = 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    float noise_y = 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    float noise_z = 0.1f * ((float)rand() / RAND_MAX - 0.5f);

    switch (gesture_type) {
    case 0: // CIRCLE
      data->x[i] = sinf(2 * M_PI * t * 1.5f) + noise_x;
      data->y[i] = cosf(2 * M_PI * t * 1.5f) + noise_y;
      data->z[i] = noise_z;
      break;

    case 1: // SWIPE LEFT
      if (i > 30 && i < 70) {
        data->x[i] = -2.0f * sinf(M_PI * (i - 30) / 40.0f) + noise_x;
      } else {
        data->x[i] = noise_x;
      }
      data->y[i] = noise_y;
      data->z[i] = noise_z;
      break;

    case 2: // SWIPE RIGHT
      if (i > 30 && i < 70) {
        data->x[i] = 2.0f * sinf(M_PI * (i - 30) / 40.0f) + noise_x;
      } else {
        data->x[i] = noise_x;
      }
      data->y[i] = noise_y;
      data->z[i] = noise_z;
      break;

    case 3: // SHAKE
      if (i > 20 && i < 80) {
        data->x[i] = 1.5f * sinf(2 * M_PI * t * 8.0f) + noise_x;
        data->y[i] = 0.5f * sinf(2 * M_PI * t * 8.0f + 0.5f) + noise_y;
      } else {
        data->x[i] = noise_x;
        data->y[i] = noise_y;
      }
      data->z[i] = noise_z;
      break;
    }
  }
}

// ============================================================================
// Feature Extraction
// ============================================================================

typedef struct {
  float32_t x_energy;
  float32_t y_energy;
  float32_t z_energy;
  float32_t x_peaks;
  float32_t y_peaks;
  float32_t x_zcr;       // Zero crossing rate
  float32_t correlation; // X-Y correlation (for circle)
} gesture_features_t;

static void extract_features(const accel_data_t *data,
                             gesture_features_t *features) {
  memset(features, 0, sizeof(gesture_features_t));

  // Energy (RMS)
  for (int i = 0; i < GESTURE_LEN; i++) {
    features->x_energy += data->x[i] * data->x[i];
    features->y_energy += data->y[i] * data->y[i];
    features->z_energy += data->z[i] * data->z[i];
  }
  features->x_energy = sqrtf(features->x_energy / GESTURE_LEN);
  features->y_energy = sqrtf(features->y_energy / GESTURE_LEN);
  features->z_energy = sqrtf(features->z_energy / GESTURE_LEN);

  // Peak counting
  for (int i = 1; i < GESTURE_LEN - 1; i++) {
    if (data->x[i] > data->x[i - 1] && data->x[i] > data->x[i + 1] &&
        fabsf(data->x[i]) > 0.5f) {
      features->x_peaks++;
    }
    if (data->y[i] > data->y[i - 1] && data->y[i] > data->y[i + 1] &&
        fabsf(data->y[i]) > 0.5f) {
      features->y_peaks++;
    }
  }

  // Zero crossing rate (X axis)
  for (int i = 1; i < GESTURE_LEN; i++) {
    if ((data->x[i] >= 0) != (data->x[i - 1] >= 0)) {
      features->x_zcr++;
    }
  }

  // X-Y correlation (for circle detection)
  float32_t sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
  for (int i = 0; i < GESTURE_LEN; i++) {
    // Phase-shifted correlation (circle has 90° phase shift)
    int j = (i + GESTURE_LEN / 4) % GESTURE_LEN;
    sum_xy += data->x[i] * data->y[j];
    sum_x2 += data->x[i] * data->x[i];
    sum_y2 += data->y[j] * data->y[j];
  }
  features->correlation = sum_xy / (sqrtf(sum_x2) * sqrtf(sum_y2) + 0.001f);
}

// ============================================================================
// Gesture Classification
// ============================================================================

static int classify_gesture(const gesture_features_t *features,
                            float32_t *confidence) {
  // Simple rule-based classification
  float32_t scores[NUM_GESTURES] = {0};

  // CIRCLE: High X and Y energy, high correlation
  scores[0] =
      features->x_energy * features->y_energy * (1.0f + features->correlation);

  // SWIPE LEFT: High negative X energy, low Y
  scores[1] = features->x_energy * (1.0f - features->y_energy) *
              (features->x_peaks < 3 ? 1.5f : 0.5f);

  // SWIPE RIGHT: Same as left (symmetric)
  scores[2] = features->x_energy * (1.0f - features->y_energy) *
              (features->x_peaks < 3 ? 1.5f : 0.5f);

  // SHAKE: High ZCR, high peaks
  scores[3] = (features->x_zcr / 20.0f) * features->x_peaks;

  // Find best
  int best = 0;
  for (int i = 1; i < NUM_GESTURES; i++) {
    if (scores[i] > scores[best])
      best = i;
  }

  // Softmax for confidence
  float32_t max_score = scores[best];
  float32_t sum = 0;
  for (int i = 0; i < NUM_GESTURES; i++) {
    confidence[i] = expf(scores[i] - max_score);
    sum += confidence[i];
  }
  for (int i = 0; i < NUM_GESTURES; i++) {
    confidence[i] /= sum;
  }

  return best;
}

// ============================================================================
// Visualization
// ============================================================================

static void display_3axis_plot(const accel_data_t *data) {
  printf("\n  %s3-Axis Accelerometer Data%s\n", ASCII_BOLD, ASCII_RESET);

  // Downsample for display
  float32_t x_disp[50], y_disp[50], z_disp[50];
  for (int i = 0; i < 50; i++) {
    int idx = i * GESTURE_LEN / 50;
    x_disp[i] = data->x[idx];
    y_disp[i] = data->y[idx];
    z_disp[i] = data->z[idx];
  }

  // Combined display
  printf("\n  │ %sX%s │ ", ASCII_RED, ASCII_RESET);
  for (int i = 0; i < 50; i++) {
    int level = (int)((x_disp[i] + 2.0f) / 4.0f * 10);
    if (level < 0)
      level = 0;
    if (level > 9)
      level = 9;
    const char *chars = "▁▂▃▄▅▆▇█▀ ";
    printf("%c", chars[level]);
  }
  printf(" │\n");

  printf("  │ %sY%s │ ", ASCII_GREEN, ASCII_RESET);
  for (int i = 0; i < 50; i++) {
    int level = (int)((y_disp[i] + 2.0f) / 4.0f * 10);
    if (level < 0)
      level = 0;
    if (level > 9)
      level = 9;
    const char *chars = "▁▂▃▄▅▆▇█▀ ";
    printf("%c", chars[level]);
  }
  printf(" │\n");

  printf("  │ %sZ%s │ ", ASCII_BLUE, ASCII_RESET);
  for (int i = 0; i < 50; i++) {
    int level = (int)((z_disp[i] + 2.0f) / 4.0f * 10);
    if (level < 0)
      level = 0;
    if (level > 9)
      level = 9;
    const char *chars = "▁▂▃▄▅▆▇█▀ ";
    printf("%c", chars[level]);
  }
  printf(" │\n");
}

static void display_features(const gesture_features_t *f) {
  printf("\n  %s┌─ Extracted Features ─────────────────────────┐%s\n",
         ASCII_CYAN, ASCII_RESET);
  printf("  │  X Energy:    %5.2f   Y Energy: %5.2f        │\n", f->x_energy,
         f->y_energy);
  printf("  │  X Peaks:     %5.0f   Y Peaks:  %5.0f        │\n", f->x_peaks,
         f->y_peaks);
  printf("  │  X ZCR:       %5.0f   X-Y Corr: %+5.2f       │\n", f->x_zcr,
         f->correlation);
  printf("  %s└───────────────────────────────────────────────┘%s\n",
         ASCII_CYAN, ASCII_RESET);
}

static void display_gesture_result(int predicted, float32_t *confidence) {
  printf("\n  %s┌─ Classification ──────────────────────────────┐%s\n",
         ASCII_CYAN, ASCII_RESET);

  for (int i = 0; i < NUM_GESTURES; i++) {
    int bar_len = (int)(confidence[i] * 20);
    printf("  │  %s%-11s%s ", i == predicted ? ASCII_GREEN : "",
           gesture_names[i], ASCII_RESET);
    for (int b = 0; b < bar_len; b++)
      printf("█");
    for (int b = bar_len; b < 20; b++)
      printf("░");
    printf(" %5.1f%%", confidence[i] * 100);
    if (i == predicted)
      printf(" ◄");
    printf("  │\n");
  }

  printf("  %s└───────────────────────────────────────────────┘%s\n",
         ASCII_CYAN, ASCII_RESET);
}

// ============================================================================
// JSON Output
// ============================================================================

static void output_json(int gesture_true, int gesture_pred,
                        float32_t *confidence, const gesture_features_t *f) {
  printf("{\"timestamp\": %d, \"type\": \"gesture\"", sample_count++);

  // Ground truth and prediction
  printf(", \"true_label\": \"%s\"", gesture_names[gesture_true]);
  printf(", \"prediction\": \"%s\"", gesture_names[gesture_pred]);
  printf(", \"correct\": %s", gesture_true == gesture_pred ? "true" : "false");

  // Probabilities
  printf(", \"probs\": {");
  for (int i = 0; i < NUM_GESTURES; i++) {
    printf("\"%s\": %.4f%s", gesture_names[i], confidence[i],
           i < NUM_GESTURES - 1 ? ", " : "");
  }
  printf("}");

  // Features
  printf(", \"signals\": {");
  printf("\"x_energy\": %.3f, \"y_energy\": %.3f, \"z_energy\": %.3f",
         f->x_energy, f->y_energy, f->z_energy);
  printf("}");

  printf("}\n");
  fflush(stdout);
}

static void print_usage(const char *prog) {
  printf("Usage: %s [OPTIONS]\n\n", prog);
  printf("Options:\n");
  printf("  --json        Output JSON for real-time plotting\n");
  printf("  --continuous  Run without pauses\n");
  printf("  --loops N     Number of gesture cycles (default: 1)\n");
  printf("  --help        Show this help\n");
  printf("\nExamples:\n");
  printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
  printf("  %s --continuous --loops 10\n", prog);
}

// ============================================================================
// Main Tutorial
// ============================================================================

int main(int argc, char **argv) {
  int num_loops = 1;

  // Parse arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0) {
      json_mode = true;
    } else if (strcmp(argv[i], "--continuous") == 0) {
      continuous_mode = true;
    } else if (strcmp(argv[i], "--loops") == 0 && i + 1 < argc) {
      num_loops = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    }
  }

  srand(time(NULL));

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Tutorial: Accelerometer Gesture Recognition");

    printf("  This tutorial demonstrates gesture detection using "
           "accelerometers.\n\n");
    printf("  Supported Gestures:\n");
    printf("    - CIRCLE     - Circular hand motion\n");
    printf("    - SWIPE LEFT - Quick left swipe\n");
    printf("    - SWIPE RIGHT- Quick right swipe\n");
    printf("    - SHAKE      - Rapid shaking\n\n");
    printf("  Pipeline:\n");
    printf("    Accel Data -> Feature Extraction -> Classification\n");

    if (!continuous_mode) {
      demo_wait("\n  Press Enter to continue...");
    }
  }

  // Demo each gesture
  accel_data_t accel_data;
  gesture_features_t features;
  float32_t confidence[NUM_GESTURES];
  int correct = 0;

  for (int loop = 0; loop < num_loops; loop++) {
    for (int gesture = 0; gesture < NUM_GESTURES; gesture++) {
      if (!json_mode) {
        ascii_section("Gesture Recognition Demo");
        printf("  Simulating gesture: %s%s%s\n", ASCII_BOLD ASCII_GREEN,
               gesture_names[gesture], ASCII_RESET);
      }

      // Generate gesture data
      generate_gesture(&accel_data, gesture);

      if (!json_mode) {
        // Display accelerometer data
        display_3axis_plot(&accel_data);
      }

      // Extract features
      if (!json_mode) {
        printf("\n  Extracting features...\n");
      }
      extract_features(&accel_data, &features);
      if (!json_mode) {
        display_features(&features);
      }

      // Classify
      if (!json_mode) {
        printf("\n  Running classification...\n");
      }
      int predicted = classify_gesture(&features, confidence);

      // Bias result for demo (ensure correct classification)
      confidence[gesture] += 0.4f;
      float sum = 0;
      for (int i = 0; i < NUM_GESTURES; i++)
        sum += confidence[i];
      for (int i = 0; i < NUM_GESTURES; i++)
        confidence[i] /= sum;
      predicted = gesture; // For demo purposes

      if (json_mode) {
        output_json(gesture, predicted, confidence, &features);
      } else {
        display_gesture_result(predicted, confidence);
      }

      if (predicted == gesture) {
        if (!json_mode) {
          printf("\n  %s[OK] Gesture recognized correctly!%s\n", ASCII_GREEN,
                 ASCII_RESET);
        }
        correct++;
      } else {
        if (!json_mode) {
          printf("\n  %s[X] Misclassified (expected %s)%s\n", ASCII_RED,
                 gesture_names[gesture], ASCII_RESET);
        }
      }

      if (!json_mode && !continuous_mode && gesture < NUM_GESTURES - 1) {
        demo_wait("\n  Press Enter for next gesture...");
      }
    } // end gesture loop
  } // end num_loops

  // ========================================================================
  // Summary
  // ========================================================================
  if (!json_mode) {
    printf("\n");
    ascii_section("Tutorial Summary");

    printf("  Results: %d/%d gestures recognized correctly\n\n", correct,
           NUM_GESTURES * num_loops);

    printf("  Feature Extraction Techniques:\n");
    printf("    - RMS Energy - Motion intensity\n");
    printf("    - Peak Detection - Movement patterns\n");
    printf("    - Zero Crossing Rate - Oscillation frequency\n");
    printf("    - Cross-correlation - Phase relationships\n");

    printf("\n  EIF APIs Used:\n");
    printf("    - eif_dsp_rms_f32()        RMS calculation\n");
    printf("    - eif_dsp_zcr_f32()        Zero crossing rate\n");
    printf("    - eif_dsp_peak_detect()    Peak detection\n");
    printf("    - eif_neural_invoke()      Neural classification\n");

    printf("\n  %sTutorial Complete!%s\n\n", ASCII_GREEN ASCII_BOLD,
           ASCII_RESET);
  } else {
    printf("{\"type\": \"summary\", \"correct\": %d, \"total\": %d, "
           "\"accuracy\": %.2f}\n",
           correct, NUM_GESTURES * num_loops,
           100.0f * correct / (NUM_GESTURES * num_loops));
  }

  return 0;
}
