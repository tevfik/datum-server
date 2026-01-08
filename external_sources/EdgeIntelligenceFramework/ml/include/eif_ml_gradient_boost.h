/**
 * @file eif_ml_gradient_boost.h
 * @brief Gradient Boosting Classifier (Inference Only)
 *
 * Lightweight Gradient Boosting for embedded inference:
 * - Tree-based weak learners
 * - Additive model
 * - Multi-class support
 *
 * Trees are trained offline (e.g., XGBoost/LightGBM); this is inference-only.
 */

#ifndef EIF_ML_GRADIENT_BOOST_H
#define EIF_ML_GRADIENT_BOOST_H

#include <math.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum configuration
#define EIF_GBM_MAX_TREES 32
#define EIF_GBM_MAX_NODES 32
#define EIF_GBM_MAX_FEATURES 32
#define EIF_GBM_MAX_CLASSES 10

/**
 * @brief GBM tree node (simplified)
 */
typedef struct {
  int feature_idx;  ///< Feature to split on (-1 for leaf)
  float threshold;  ///< Split threshold
  int left_child;   ///< Left child index
  int right_child;  ///< Right child index
  float leaf_value; ///< Leaf output value
  bool is_leaf;
} eif_gbm_node_t;

/**
 * @brief Single boosted tree
 */
typedef struct {
  eif_gbm_node_t nodes[EIF_GBM_MAX_NODES];
  int num_nodes;
  int root;
} eif_gbm_tree_t;

/**
 * @brief Gradient Boosted classifier
 */
typedef struct {
  eif_gbm_tree_t trees[EIF_GBM_MAX_TREES];
  int num_trees;
  int num_features;
  int num_classes;
  float learning_rate;
  float base_score; ///< Initial prediction (bias)
} eif_gradient_boost_t;

/**
 * @brief Initialize gradient boosting model
 */
static inline void eif_gbm_init(eif_gradient_boost_t *gbm, int num_features,
                                int num_classes, float learning_rate) {
  gbm->num_trees = 0;
  gbm->num_features = num_features;
  gbm->num_classes = num_classes;
  gbm->learning_rate = learning_rate;
  gbm->base_score = 0.0f;
}

/**
 * @brief Add a tree to the model
 */
static inline int eif_gbm_add_tree(eif_gradient_boost_t *gbm) {
  if (gbm->num_trees >= EIF_GBM_MAX_TREES)
    return -1;

  int idx = gbm->num_trees;
  gbm->trees[idx].num_nodes = 0;
  gbm->trees[idx].root = 0;
  gbm->num_trees++;

  return idx;
}

/**
 * @brief Add node to a tree
 */
static inline int eif_gbm_add_node(eif_gbm_tree_t *tree, int feature_idx,
                                   float threshold, bool is_leaf,
                                   float leaf_value) {
  if (tree->num_nodes >= EIF_GBM_MAX_NODES)
    return -1;

  int idx = tree->num_nodes;
  tree->nodes[idx].feature_idx = feature_idx;
  tree->nodes[idx].threshold = threshold;
  tree->nodes[idx].is_leaf = is_leaf;
  tree->nodes[idx].leaf_value = leaf_value;
  tree->nodes[idx].left_child = -1;
  tree->nodes[idx].right_child = -1;
  tree->num_nodes++;

  return idx;
}

/**
 * @brief Set node children
 */
static inline void eif_gbm_set_children(eif_gbm_tree_t *tree, int node_idx,
                                        int left_child, int right_child) {
  tree->nodes[node_idx].left_child = left_child;
  tree->nodes[node_idx].right_child = right_child;
}

/**
 * @brief Get tree output
 */
static inline float eif_gbm_tree_predict(eif_gbm_tree_t *tree,
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

  return tree->nodes[node].leaf_value;
}

/**
 * @brief Predict raw score (for binary classification)
 */
static inline float eif_gbm_predict_raw(eif_gradient_boost_t *gbm,
                                        const float *features) {
  float score = gbm->base_score;

  for (int t = 0; t < gbm->num_trees; t++) {
    score +=
        gbm->learning_rate * eif_gbm_tree_predict(&gbm->trees[t], features);
  }

  return score;
}

/**
 * @brief Sigmoid function for probability
 */
static inline float gbm_sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }

/**
 * @brief Predict probability (binary classification)
 */
static inline float eif_gbm_predict_proba_binary(eif_gradient_boost_t *gbm,
                                                 const float *features) {
  float raw = eif_gbm_predict_raw(gbm, features);
  return gbm_sigmoid(raw);
}

/**
 * @brief Predict class (binary)
 */
static inline int eif_gbm_predict(eif_gradient_boost_t *gbm,
                                  const float *features) {
  return eif_gbm_predict_proba_binary(gbm, features) >= 0.5f ? 1 : 0;
}

/**
 * @brief Create a simple example model (for demo)
 */
static inline void eif_gbm_create_example(eif_gradient_boost_t *gbm,
                                          int num_features) {
  eif_gbm_init(gbm, num_features, 2, 0.1f);
  gbm->base_score = 0.0f;

  // Tree 1
  int t1 = eif_gbm_add_tree(gbm);
  int root1 = eif_gbm_add_node(&gbm->trees[t1], 0, 0.5f, false, 0.0f);
  int left1 = eif_gbm_add_node(&gbm->trees[t1], -1, 0.0f, true, -1.0f);
  int right1 = eif_gbm_add_node(&gbm->trees[t1], -1, 0.0f, true, 1.0f);
  eif_gbm_set_children(&gbm->trees[t1], root1, left1, right1);

  // Tree 2
  int t2 = eif_gbm_add_tree(gbm);
  int root2 = eif_gbm_add_node(&gbm->trees[t2], 1, 0.3f, false, 0.0f);
  int left2 = eif_gbm_add_node(&gbm->trees[t2], -1, 0.0f, true, -0.5f);
  int right2 = eif_gbm_add_node(&gbm->trees[t2], -1, 0.0f, true, 0.5f);
  eif_gbm_set_children(&gbm->trees[t2], root2, left2, right2);

  // Tree 3
  int t3 = eif_gbm_add_tree(gbm);
  int root3 = eif_gbm_add_node(&gbm->trees[t3], 0, 0.7f, false, 0.0f);
  int left3 = eif_gbm_add_node(&gbm->trees[t3], -1, 0.0f, true, -0.3f);
  int right3 = eif_gbm_add_node(&gbm->trees[t3], -1, 0.0f, true, 0.8f);
  eif_gbm_set_children(&gbm->trees[t3], root3, left3, right3);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_GRADIENT_BOOST_H
