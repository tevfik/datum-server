/**
 * @file eif_ml_random_forest.h
 * @brief Random Forest Classifier (Inference Only)
 *
 * Lightweight Random Forest for embedded inference:
 * - Decision tree ensemble
 * - Majority voting
 * - Feature importance
 *
 * Trees are trained offline; this is inference-only.
 */

#ifndef EIF_ML_RANDOM_FOREST_H
#define EIF_ML_RANDOM_FOREST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum tree configuration
#define EIF_RF_MAX_TREES 16
#define EIF_RF_MAX_NODES 64
#define EIF_RF_MAX_FEATURES 32
#define EIF_RF_MAX_CLASSES 10

/**
 * @brief Decision tree node
 */
typedef struct {
  int feature_idx; ///< Feature to split on (-1 for leaf)
  float threshold; ///< Split threshold
  int left_child;  ///< Left child index (or class if leaf)
  int right_child; ///< Right child index
  int class_label; ///< Class label (for leaf nodes)
  bool is_leaf;
} eif_dt_node_t;

/**
 * @brief Single decision tree
 */
typedef struct {
  eif_dt_node_t nodes[EIF_RF_MAX_NODES];
  int num_nodes;
  int root;
} eif_decision_tree_t;

/**
 * @brief Random Forest classifier
 */
typedef struct {
  eif_decision_tree_t trees[EIF_RF_MAX_TREES];
  int num_trees;
  int num_features;
  int num_classes;
} eif_random_forest_t;

/**
 * @brief Initialize random forest
 */
static inline void eif_rf_init(eif_random_forest_t *rf, int num_features,
                               int num_classes) {
  rf->num_trees = 0;
  rf->num_features = num_features;
  rf->num_classes = num_classes;
}

/**
 * @brief Add a tree to the forest
 * @return Tree index or -1 if full
 */
static inline int eif_rf_add_tree(eif_random_forest_t *rf) {
  if (rf->num_trees >= EIF_RF_MAX_TREES)
    return -1;

  int idx = rf->num_trees;
  rf->trees[idx].num_nodes = 0;
  rf->trees[idx].root = 0;
  rf->num_trees++;

  return idx;
}

/**
 * @brief Add node to a tree
 */
static inline int eif_dt_add_node(eif_decision_tree_t *tree, int feature_idx,
                                  float threshold, bool is_leaf,
                                  int class_label) {
  if (tree->num_nodes >= EIF_RF_MAX_NODES)
    return -1;

  int idx = tree->num_nodes;
  tree->nodes[idx].feature_idx = feature_idx;
  tree->nodes[idx].threshold = threshold;
  tree->nodes[idx].is_leaf = is_leaf;
  tree->nodes[idx].class_label = class_label;
  tree->nodes[idx].left_child = -1;
  tree->nodes[idx].right_child = -1;
  tree->num_nodes++;

  return idx;
}

/**
 * @brief Set node children
 */
static inline void eif_dt_set_children(eif_decision_tree_t *tree, int node_idx,
                                       int left_child, int right_child) {
  tree->nodes[node_idx].left_child = left_child;
  tree->nodes[node_idx].right_child = right_child;
}

/**
 * @brief Predict with single decision tree
 */
static inline int eif_dt_predict(eif_decision_tree_t *tree,
                                 const float *features) {
  int node = tree->root;

  while (!tree->nodes[node].is_leaf) {
    int feat = tree->nodes[node].feature_idx;
    float thresh = tree->nodes[node].threshold;

    if (features[feat] <= thresh) {
      node = tree->nodes[node].left_child;
    } else {
      node = tree->nodes[node].right_child;
    }

    if (node < 0 || node >= tree->num_nodes)
      break;
  }

  return tree->nodes[node].class_label;
}

/**
 * @brief Predict with random forest (majority voting)
 */
static inline int eif_rf_predict(eif_random_forest_t *rf,
                                 const float *features) {
  int votes[EIF_RF_MAX_CLASSES] = {0};

  // Collect votes from all trees
  for (int t = 0; t < rf->num_trees; t++) {
    int pred = eif_dt_predict(&rf->trees[t], features);
    if (pred >= 0 && pred < rf->num_classes) {
      votes[pred]++;
    }
  }

  // Find majority
  int best_class = 0;
  int best_votes = votes[0];
  for (int c = 1; c < rf->num_classes; c++) {
    if (votes[c] > best_votes) {
      best_votes = votes[c];
      best_class = c;
    }
  }

  return best_class;
}

/**
 * @brief Get class probabilities
 */
static inline void eif_rf_predict_proba(eif_random_forest_t *rf,
                                        const float *features, float *probs) {
  int votes[EIF_RF_MAX_CLASSES] = {0};

  for (int t = 0; t < rf->num_trees; t++) {
    int pred = eif_dt_predict(&rf->trees[t], features);
    if (pred >= 0 && pred < rf->num_classes) {
      votes[pred]++;
    }
  }

  for (int c = 0; c < rf->num_classes; c++) {
    probs[c] = (float)votes[c] / rf->num_trees;
  }
}

/**
 * @brief Create a simple example forest (for demo)
 */
static inline void eif_rf_create_example(eif_random_forest_t *rf,
                                         int num_features, int num_classes) {
  eif_rf_init(rf, num_features, num_classes);

  // Tree 1: Simple 2-level tree
  int t1 = eif_rf_add_tree(rf);
  int root1 = eif_dt_add_node(&rf->trees[t1], 0, 0.5f, false, -1);
  int left1 = eif_dt_add_node(&rf->trees[t1], -1, 0.0f, true, 0);
  int right1 = eif_dt_add_node(&rf->trees[t1], -1, 0.0f, true, 1);
  eif_dt_set_children(&rf->trees[t1], root1, left1, right1);

  // Tree 2: Another simple tree
  int t2 = eif_rf_add_tree(rf);
  int root2 = eif_dt_add_node(&rf->trees[t2], 1, 0.3f, false, -1);
  int left2 = eif_dt_add_node(&rf->trees[t2], -1, 0.0f, true, 0);
  int right2 = eif_dt_add_node(&rf->trees[t2], -1, 0.0f, true, 1);
  eif_dt_set_children(&rf->trees[t2], root2, left2, right2);

  // Tree 3
  int t3 = eif_rf_add_tree(rf);
  int root3 = eif_dt_add_node(&rf->trees[t3], 0, 0.6f, false, -1);
  int left3 = eif_dt_add_node(&rf->trees[t3], -1, 0.0f, true, 0);
  int right3 = eif_dt_add_node(&rf->trees[t3], -1, 0.0f, true, 1);
  eif_dt_set_children(&rf->trees[t3], root3, left3, right3);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_RANDOM_FOREST_H
