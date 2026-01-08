/**
 * @file eif_ml_knn.c
 * @brief k-Nearest Neighbors Implementation
 */

#include "eif_ml_knn.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_K 20 // Sanity limit for stack allocation

typedef struct {
  float32_t dist_sq;
  int32_t label;
} neighbor_t;

void eif_ml_knn_init(eif_ml_knn_t *knn, const float32_t *train_data,
                     const int32_t *train_labels, int num_samples,
                     int num_features, int k) {
  if (!knn)
    return;
  knn->train_data = train_data;
  knn->train_labels = train_labels;
  knn->num_samples = num_samples;
  knn->num_features = num_features;
  knn->k = k;
}

int32_t eif_ml_knn_predict(const eif_ml_knn_t *knn, const float32_t *input) {
  if (!knn || !knn->train_data || knn->k > MAX_K)
    return -1;

  // We need to find TOP K smallest distances.
  // Instead of full sort, we keep a sorted list of size K.
  // Neighbors are stored as (distance, label)

  neighbor_t closest[MAX_K];
  for (int i = 0; i < knn->k; i++)
    closest[i].dist_sq = 3.4028235E38f; // FLT_MAX

  for (int i = 0; i < knn->num_samples; i++) {
    // Compute Euclidean distance squared
    float32_t dist_sq = 0.0f;
    const float32_t *sample = knn->train_data + i * knn->num_features;

    for (int j = 0; j < knn->num_features; j++) {
      float32_t d = input[j] - sample[j];
      dist_sq += d * d;
    }

    // Insertion sort into closest[k]
    if (dist_sq < closest[knn->k - 1].dist_sq) {
      // Find position
      int pos = knn->k - 1;
      while (pos > 0 && closest[pos - 1].dist_sq > dist_sq) {
        closest[pos] = closest[pos - 1];
        pos--;
      }
      closest[pos].dist_sq = dist_sq;
      closest[pos].label = knn->train_labels[i];
    }
  }

// Majority Vote
// Max classes assumption: 10 (arbitrary small limit for embedded)
#define MAX_CLASSES 10
  int counts[MAX_CLASSES] = {0};

  for (int i = 0; i < knn->k; i++) {
    int lbl = closest[i].label;
    if (lbl >= 0 && lbl < MAX_CLASSES) {
      counts[lbl]++;
    }
  }

  // Find max vote
  int max_votes = -1;
  int best_label = -1;

  for (int i = 0; i < MAX_CLASSES; i++) {
    if (counts[i] > max_votes) {
      max_votes = counts[i];
      best_label = i;
    }
  }

  return best_label;
}
