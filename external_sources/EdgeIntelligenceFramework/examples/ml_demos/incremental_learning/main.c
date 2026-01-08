/**
 * @file main.c
 * @brief On-Device Learning Demo
 *
 * Demonstrates prototype-based learning for personalization.
 * The system learns new patterns from user feedback.
 *
 * Build: make incremental_learning_demo
 * Run:   ./bin/incremental_learning_demo
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EIF_HAS_PRINTF 1
#include "eif_learning.h"

// =============================================================================
// Simulated User Gesture Data
// =============================================================================

#define FEATURE_DIM 8
#define NUM_GESTURES 4

static const char *gesture_names[] = {"Wave", "Tap", "Circle", "Swipe"};

// Generate simulated gesture features
static void generate_gesture(int gesture_id, int16_t *features) {
  // Base patterns for each gesture
  static const int16_t patterns[4][8] = {
      {10000, -5000, 2000, 15000, -3000, 8000, 1000, -2000}, // Wave
      {2000, 3000, 20000, -500, 1000, -800, 25000, 1500},    // Tap
      {8000, 12000, 8000, 12000, 8000, 12000, 8000, 12000},  // Circle
      {25000, 20000, 15000, 10000, 5000, 2000, -500, -2000}, // Swipe
  };

  for (int i = 0; i < FEATURE_DIM; i++) {
    // Add noise to pattern
    int noise = (rand() % 4000) - 2000;
    features[i] = patterns[gesture_id][i] + noise;
  }
}

// =============================================================================
// Demo
// =============================================================================

static void print_header(void) {
  printf(
      "\n╔═══════════════════════════════════════════════════════════════╗\n");
  printf(
      "║          🎓 On-Device Learning Demo                            ║\n");
  printf("╠═══════════════════════════════════════════════════════════════╣\n");
  printf(
      "║  Prototype-based learning for gesture personalization          ║\n");
  printf(
      "╚═══════════════════════════════════════════════════════════════╝\n\n");
}

static void print_features(const int16_t *features, int dim) {
  printf("  Features: [");
  for (int i = 0; i < dim && i < 4; i++) {
    printf("%6d", features[i]);
    if (i < dim - 1 && i < 3)
      printf(", ");
  }
  printf(" ...]\n");
}

int main(void) {
  print_header();
  srand(42);

  // Initialize prototype classifier
  eif_proto_classifier_t classifier;
  eif_proto_init(&classifier, FEATURE_DIM);
  eif_proto_set_lr(&classifier, 0.2f);

  printf("📚 Phase 1: Learning from examples\n");
  printf("───────────────────────────────────────────────────────\n\n");

  // Training phase: show a few examples of each gesture
  int16_t features[FEATURE_DIM];
  int examples_per_class = 5;

  for (int g = 0; g < NUM_GESTURES; g++) {
    printf("  Learning '%s'...\n", gesture_names[g]);

    for (int i = 0; i < examples_per_class; i++) {
      generate_gesture(g, features);
      eif_proto_update(&classifier, features, g);
    }
  }

  printf("\n  ✅ Learned %d prototypes from %d examples\n\n",
         classifier.num_prototypes, NUM_GESTURES * examples_per_class);

  // Show prototypes
  printf("  Prototypes:\n");
  for (int i = 0; i < classifier.num_prototypes; i++) {
    eif_prototype_t *p = &classifier.prototypes[i];
    printf("    [%d] %s: samples=%ld, center=[%d, %d, ...]\n", i,
           gesture_names[p->label], (long)p->count, p->center[0], p->center[1]);
  }

  // Testing phase
  printf("\n📊 Phase 2: Testing recognition\n");
  printf("───────────────────────────────────────────────────────\n\n");

  int correct = 0;
  int total = 20;

  for (int i = 0; i < total; i++) {
    int true_gesture = i % NUM_GESTURES;
    generate_gesture(true_gesture, features);

    int predicted = eif_proto_predict(&classifier, features);

    if (predicted == true_gesture) {
      correct++;
      printf("  ✅ %s -> %s\n", gesture_names[true_gesture],
             gesture_names[predicted]);
    } else {
      printf("  ❌ %s -> %s\n", gesture_names[true_gesture],
             predicted >= 0 ? gesture_names[predicted] : "Unknown");
    }
  }

  printf("\n  Accuracy: %d/%d = %.1f%%\n\n", correct, total,
         100.0f * correct / total);

  // Personalization
  printf("🔧 Phase 3: Personalization with user feedback\n");
  printf("───────────────────────────────────────────────────────\n\n");

  // Simulate user with slightly different gesture style
  printf("  Simulating user's personal style...\n\n");

  for (int i = 0; i < 10; i++) {
    int gesture = i % NUM_GESTURES;
    generate_gesture(gesture, features);

    // User's style: shifted values
    for (int j = 0; j < FEATURE_DIM; j++) {
      features[j] += 1500;
    }

    // User provides feedback
    eif_proto_update(&classifier, features, gesture);
  }

  printf("  ✅ Adapted to user's style with 10 corrections\n\n");

  // Test again with user's style
  printf("📊 Phase 4: Post-adaptation testing\n");
  printf("───────────────────────────────────────────────────────\n\n");

  correct = 0;
  for (int i = 0; i < total; i++) {
    int true_gesture = i % NUM_GESTURES;
    generate_gesture(true_gesture, features);

    // User's style
    for (int j = 0; j < FEATURE_DIM; j++) {
      features[j] += 1500;
    }

    int predicted = eif_proto_predict(&classifier, features);

    if (predicted == true_gesture)
      correct++;
  }

  printf("  Post-adaptation accuracy: %d/%d = %.1f%%\n", correct, total,
         100.0f * correct / total);

  // Statistics demo
  printf("\n📈 Incremental Statistics Demo\n");
  printf("───────────────────────────────────────────────────────\n\n");

  eif_stats_t stats;
  eif_stats_init(&stats, FEATURE_DIM);

  // Feed samples
  for (int i = 0; i < 100; i++) {
    generate_gesture(i % NUM_GESTURES, features);
    eif_stats_update(&stats, features);
  }

  int16_t mean[FEATURE_DIM];
  int16_t var[FEATURE_DIM];
  eif_stats_get_mean(&stats, mean);
  eif_stats_get_variance(&stats, var);

  printf("  Samples: %ld\n", (long)stats.count);
  printf("  Mean: [%d, %d, %d, ...]\n", mean[0], mean[1], mean[2]);
  printf("  Var:  [%d, %d, %d, ...]\n", var[0], var[1], var[2]);

  printf("\n✅ Demo complete!\n\n");

  return 0;
}
