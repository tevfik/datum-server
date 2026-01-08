/**
 * @file eif_ml_rf.c
 * @brief Random Forest Classifier Implementation
 */

#include "eif_ml_rf.h"
#include <math.h>
#include <string.h>
#include <alloca.h>

// ============================================================================
// Internal Helper Functions
// ============================================================================

// Simple LCG random number generator for deterministic behavior
static uint32_t rf_random_state = 42;

static void rf_seed(uint32_t seed) { rf_random_state = seed; }

static uint32_t rf_rand(void) {
  rf_random_state = rf_random_state * 1664525u + 1013904223u;
  return rf_random_state;
}

static float32_t rf_randf(void) {
  return (float32_t)rf_rand() / (float32_t)UINT32_MAX;
}

// Fisher-Yates shuffle for bootstrap sampling
static void rf_shuffle(uint32_t *array, uint32_t n) {
  for (uint32_t i = n - 1; i > 0; i--) {
    uint32_t j = rf_rand() % (i + 1);
    uint32_t temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}

// Calculate Gini impurity
static float32_t rf_gini_impurity(const uint32_t *class_counts,
                                  uint16_t num_classes, uint32_t total) {
  if (total == 0)
    return 0.0f;

  float32_t gini = 1.0f;
  for (uint16_t i = 0; i < num_classes; i++) {
    float32_t p = (float32_t)class_counts[i] / (float32_t)total;
    gini -= p * p;
  }
  return gini;
}

// Find best split for a node
typedef struct {
  int16_t feature;
  float32_t value;
  float32_t gain;
} rf_split_t;

static rf_split_t rf_find_best_split(const float32_t *X, const uint16_t *y,
                                      const uint32_t *indices, uint32_t n_samples,
                                      uint16_t n_features, uint16_t n_classes,
                                      uint16_t max_features,
                                      eif_memory_pool_t *pool) {
  rf_split_t best = {-1, 0.0f, -1.0f};

  // Calculate parent impurity
  uint32_t *parent_counts =
      eif_memory_alloc(pool, n_classes * sizeof(uint32_t), 4);
  memset(parent_counts, 0, n_classes * sizeof(uint32_t));

  for (uint32_t i = 0; i < n_samples; i++) {
    parent_counts[y[indices[i]]]++;
  }
  float32_t parent_impurity = rf_gini_impurity(parent_counts, n_classes, n_samples);

  // Random feature selection
  uint32_t *feature_indices =
      eif_memory_alloc(pool, n_features * sizeof(uint32_t), 4);
  for (uint16_t i = 0; i < n_features; i++) {
    feature_indices[i] = i;
  }
  rf_shuffle(feature_indices, n_features);

  uint32_t *left_counts =
      eif_memory_alloc(pool, n_classes * sizeof(uint32_t), 4);
  uint32_t *right_counts =
      eif_memory_alloc(pool, n_classes * sizeof(uint32_t), 4);

  // Try max_features random features
  for (uint16_t f_idx = 0; f_idx < max_features && f_idx < n_features; f_idx++) {
    uint16_t feature = feature_indices[f_idx];

    // Collect unique values for this feature
    float32_t *values =
        eif_memory_alloc(pool, n_samples * sizeof(float32_t), 4);
    for (uint32_t i = 0; i < n_samples; i++) {
      values[i] = X[indices[i] * n_features + feature];
    }

    // Try different thresholds
    for (uint32_t i = 0; i < n_samples - 1; i++) {
      float32_t threshold = (values[i] + values[i + 1]) / 2.0f;

      // Calculate split statistics
      memset(left_counts, 0, n_classes * sizeof(uint32_t));
      memset(right_counts, 0, n_classes * sizeof(uint32_t));
      uint32_t n_left = 0, n_right = 0;

      for (uint32_t j = 0; j < n_samples; j++) {
        float32_t val = X[indices[j] * n_features + feature];
        if (val <= threshold) {
          left_counts[y[indices[j]]]++;
          n_left++;
        } else {
          right_counts[y[indices[j]]]++;
          n_right++;
        }
      }

      if (n_left == 0 || n_right == 0)
        continue;

      // Calculate information gain
      float32_t left_impurity = rf_gini_impurity(left_counts, n_classes, n_left);
      float32_t right_impurity =
          rf_gini_impurity(right_counts, n_classes, n_right);
      float32_t weighted_impurity = ((float32_t)n_left * left_impurity +
                                     (float32_t)n_right * right_impurity) /
                                    (float32_t)n_samples;
      float32_t gain = parent_impurity - weighted_impurity;

      if (gain > best.gain) {
        best.feature = feature;
        best.value = threshold;
        best.gain = gain;
      }
    }
  }

  return best;
}

// Recursively build decision tree
static uint32_t rf_build_tree_recursive(
    eif_rf_tree_t *tree, const float32_t *X, const uint16_t *y,
    const uint32_t *indices, uint32_t n_samples, uint16_t n_features,
    uint16_t n_classes, uint16_t max_features, uint16_t depth,
    uint16_t max_depth, uint16_t min_samples_split, uint16_t min_samples_leaf,
    eif_memory_pool_t *pool) {

  if (tree->num_nodes >= tree->max_nodes) {
    return UINT32_MAX; // Tree full
  }

  uint32_t node_idx = tree->num_nodes++;
  eif_rf_node_t *node = &tree->nodes[node_idx];

  // Calculate class distribution
  uint32_t *class_counts =
      eif_memory_alloc(pool, n_classes * sizeof(uint32_t), 4);
  memset(class_counts, 0, n_classes * sizeof(uint32_t));

  for (uint32_t i = 0; i < n_samples; i++) {
    class_counts[y[indices[i]]]++;
  }

  // Find majority class
  uint16_t majority_class = 0;
  uint32_t max_count = 0;
  for (uint16_t c = 0; c < n_classes; c++) {
    if (class_counts[c] > max_count) {
      max_count = class_counts[c];
      majority_class = c;
    }
  }

  node->class_id = majority_class;
  node->num_samples = n_samples;
  node->split_feature = -1;
  node->left = -1;
  node->right = -1;

  // Store class probabilities
  node->class_probs = eif_memory_alloc(pool, n_classes * sizeof(float32_t), 4);
  for (uint16_t c = 0; c < n_classes; c++) {
    node->class_probs[c] = (float32_t)class_counts[c] / (float32_t)n_samples;
  }

  // Check stopping criteria
  int all_same_class = (max_count == n_samples);
  if (depth >= max_depth || n_samples < min_samples_split || all_same_class) {
    return node_idx;
  }

  // Find best split
  rf_split_t split = rf_find_best_split(X, y, indices, n_samples, n_features,
                                         n_classes, max_features, pool);

  if (split.feature < 0 || split.gain <= 0.0f) {
    return node_idx; // No valid split found
  }

  // Partition samples
  uint32_t *left_indices =
      eif_memory_alloc(pool, n_samples * sizeof(uint32_t), 4);
  uint32_t *right_indices =
      eif_memory_alloc(pool, n_samples * sizeof(uint32_t), 4);
  uint32_t n_left = 0, n_right = 0;

  for (uint32_t i = 0; i < n_samples; i++) {
    float32_t val = X[indices[i] * n_features + split.feature];
    if (val <= split.value) {
      left_indices[n_left++] = indices[i];
    } else {
      right_indices[n_right++] = indices[i];
    }
  }

  if (n_left < min_samples_leaf || n_right < min_samples_leaf) {
    return node_idx; // Split doesn't meet minimum leaf size
  }

  // Update node with split information
  node->split_feature = split.feature;
  node->split_value = split.value;

  // Recursively build children
  node->left = rf_build_tree_recursive(
      tree, X, y, left_indices, n_left, n_features, n_classes, max_features,
      depth + 1, max_depth, min_samples_split, min_samples_leaf, pool);

  node->right = rf_build_tree_recursive(
      tree, X, y, right_indices, n_right, n_features, n_classes, max_features,
      depth + 1, max_depth, min_samples_split, min_samples_leaf, pool);

  return node_idx;
}

// ============================================================================
// Public API Implementation
// ============================================================================

eif_status_t eif_rf_init(eif_rf_t *rf, uint16_t num_trees, uint16_t max_depth,
                         uint16_t min_samples_split, uint16_t min_samples_leaf,
                         uint16_t num_features, uint16_t num_classes,
                         eif_memory_pool_t *pool) {
  if (!rf || !pool || num_trees == 0 || num_features == 0 || num_classes == 0) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  rf->num_trees = num_trees;
  rf->max_depth = (max_depth == 0) ? EIF_RF_MAX_DEPTH : max_depth;
  rf->min_samples_split =
      (min_samples_split == 0) ? EIF_RF_MIN_SAMPLES_SPLIT : min_samples_split;
  rf->min_samples_leaf =
      (min_samples_leaf == 0) ? EIF_RF_MIN_SAMPLES_LEAF : min_samples_leaf;
  rf->num_features = num_features;
  rf->num_classes = num_classes;
  rf->random_state = 42;
  rf->oob_score = 0.0f;

  // Max features = sqrt(n_features) for classification
  rf->max_features = (uint16_t)sqrtf((float32_t)num_features);

  // Allocate trees
  rf->trees = eif_memory_alloc(pool, num_trees * sizeof(eif_rf_tree_t), 4);
  if (!rf->trees) {
    return EIF_STATUS_OUT_OF_MEMORY;
  }

  // Initialize each tree
  uint32_t max_nodes = (1u << (rf->max_depth + 1)) - 1;
  for (uint16_t i = 0; i < num_trees; i++) {
    rf->trees[i].nodes =
        eif_memory_alloc(pool, max_nodes * sizeof(eif_rf_node_t), 4);
    rf->trees[i].max_nodes = max_nodes;
    rf->trees[i].num_nodes = 0;
    rf->trees[i].oob_samples = NULL;
    rf->trees[i].num_oob = 0;

    if (!rf->trees[i].nodes) {
      return EIF_STATUS_OUT_OF_MEMORY;
    }
  }

  // Feature importance array
  rf->feature_importance =
      eif_memory_alloc(pool, num_features * sizeof(float32_t), 4);
  if (!rf->feature_importance) {
    return EIF_STATUS_OUT_OF_MEMORY;
  }
  memset(rf->feature_importance, 0, num_features * sizeof(float32_t));

  return EIF_STATUS_OK;
}

eif_status_t eif_rf_fit(eif_rf_t *rf, const float32_t *X, const uint16_t *y,
                        uint32_t num_samples, eif_memory_pool_t *pool) {
  if (!rf || !X || !y || num_samples == 0) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  rf_seed(rf->random_state);

  // Build each tree with bootstrap sampling
  for (uint16_t t = 0; t < rf->num_trees; t++) {
    // Create bootstrap sample
    uint32_t *bootstrap =
        eif_memory_alloc(pool, num_samples * sizeof(uint32_t), 4);
    uint8_t *in_bag = eif_memory_alloc(pool, num_samples * sizeof(uint8_t), 1);
    memset(in_bag, 0, num_samples);

    // Bootstrap sampling with replacement
    for (uint32_t i = 0; i < num_samples; i++) {
      uint32_t idx = rf_rand() % num_samples;
      bootstrap[i] = idx;
      in_bag[idx] = 1;
    }

    // Track OOB samples for this tree
    uint32_t num_oob = 0;
    for (uint32_t i = 0; i < num_samples; i++) {
      if (!in_bag[i])
        num_oob++;
    }

    rf->trees[t].oob_samples =
        eif_memory_alloc(pool, num_oob * sizeof(uint32_t), 4);
    rf->trees[t].num_oob = num_oob;
    uint32_t oob_idx = 0;
    for (uint32_t i = 0; i < num_samples; i++) {
      if (!in_bag[i]) {
        rf->trees[t].oob_samples[oob_idx++] = i;
      }
    }

    // Build tree
    rf->trees[t].num_nodes = 0;
    rf_build_tree_recursive(&rf->trees[t], X, y, bootstrap, num_samples,
                            rf->num_features, rf->num_classes,
                            rf->max_features, 0, rf->max_depth,
                            rf->min_samples_split, rf->min_samples_leaf, pool);
  }

  return EIF_STATUS_OK;
}

eif_status_t eif_rf_predict(const eif_rf_t *rf, const float32_t *x,
                            uint16_t *class_out) {
  if (!rf || !x || !class_out) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  // Vote from all trees
  uint32_t *votes =
      alloca(rf->num_classes * sizeof(uint32_t)); // Stack allocation
  memset(votes, 0, rf->num_classes * sizeof(uint32_t));

  for (uint16_t t = 0; t < rf->num_trees; t++) {
    uint32_t node_idx = 0;
    eif_rf_tree_t *tree = &rf->trees[t];

    // Traverse tree
    while (node_idx < tree->num_nodes) {
      eif_rf_node_t *node = &tree->nodes[node_idx];

      if (node->split_feature < 0) {
        // Leaf node
        votes[node->class_id]++;
        break;
      }

      // Internal node
      float32_t val = x[node->split_feature];
      if (val <= node->split_value) {
        node_idx = node->left;
      } else {
        node_idx = node->right;
      }

      if (node_idx == (uint32_t)-1)
        break;
    }
  }

  // Find class with most votes
  uint16_t best_class = 0;
  uint32_t max_votes = 0;
  for (uint16_t c = 0; c < rf->num_classes; c++) {
    if (votes[c] > max_votes) {
      max_votes = votes[c];
      best_class = c;
    }
  }

  *class_out = best_class;
  return EIF_STATUS_OK;
}

eif_status_t eif_rf_predict_proba(const eif_rf_t *rf, const float32_t *x,
                                  float32_t *probs) {
  if (!rf || !x || !probs) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  memset(probs, 0, rf->num_classes * sizeof(float32_t));

  for (uint16_t t = 0; t < rf->num_trees; t++) {
    uint32_t node_idx = 0;
    eif_rf_tree_t *tree = &rf->trees[t];

    while (node_idx < tree->num_nodes) {
      eif_rf_node_t *node = &tree->nodes[node_idx];

      if (node->split_feature < 0) {
        // Leaf node - add class probabilities
        for (uint16_t c = 0; c < rf->num_classes; c++) {
          probs[c] += node->class_probs[c];
        }
        break;
      }

      float32_t val = x[node->split_feature];
      if (val <= node->split_value) {
        node_idx = node->left;
      } else {
        node_idx = node->right;
      }

      if (node_idx == (uint32_t)-1)
        break;
    }
  }

  // Normalize probabilities
  for (uint16_t c = 0; c < rf->num_classes; c++) {
    probs[c] /= (float32_t)rf->num_trees;
  }

  return EIF_STATUS_OK;
}

eif_status_t eif_rf_predict_batch(const eif_rf_t *rf, const float32_t *X,
                                  uint32_t num_samples, uint16_t *y_pred) {
  if (!rf || !X || !y_pred || num_samples == 0) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  for (uint32_t i = 0; i < num_samples; i++) {
    eif_status_t status =
        eif_rf_predict(rf, &X[i * rf->num_features], &y_pred[i]);
    (void)status;
  }

  return EIF_STATUS_OK;
}

eif_status_t eif_rf_get_feature_importance(const eif_rf_t *rf,
                                           float32_t *importance) {
  if (!rf || !importance) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  memcpy(importance, rf->feature_importance,
         rf->num_features * sizeof(float32_t));
  return EIF_STATUS_OK;
}

eif_status_t eif_rf_get_oob_score(const eif_rf_t *rf, float32_t *score) {
  if (!rf || !score) {
    return EIF_STATUS_INVALID_ARGUMENT;
  }

  *score = rf->oob_score;
  return EIF_STATUS_OK;
}

void eif_rf_cleanup(eif_rf_t *rf) {
  if (!rf)
    return;
  // Memory is managed by pool, no explicit free needed
  rf->trees = NULL;
  rf->feature_importance = NULL;
}
