/**
 * @file eif_ml_gradient_boost_fixed.h
 * @brief Gradient Boosting Classifier (Fixed Point Q15)
 */

#ifndef EIF_ML_GRADIENT_BOOST_FIXED_H
#define EIF_ML_GRADIENT_BOOST_FIXED_H

#include "eif_fixedpoint.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Typically GBM trees output "log odds" or similar scores which are additive.
// The leaf values can be small decimals or large. 
// We use Q15 for inputs, but Q15 might overflow for sum of many trees.
// Standard approach: Features are Q15/Q7/Q31 (depending on quantization)
// Leaf values are stored in Q15.
// Output accumulator is Q31.

typedef struct {
  int feature_idx;      ///< Feature to split on (-1 for leaf)
  q15_t threshold;      ///< Split threshold
  int left_child;       ///< Left child index
  int right_child;      ///< Right child index
  q15_t leaf_value;     ///< Leaf output value (Q15)
} eif_gbm_node_fixed_t;

typedef struct {
  const eif_gbm_node_fixed_t *nodes;
  int num_nodes;
  int root;
} eif_gbm_tree_fixed_t;

typedef struct {
  int num_trees;
  int num_classes;       ///< 2 for binary (score < 0 or > 0), >2 for multi-class
  const eif_gbm_tree_fixed_t *trees;
  q15_t base_score;      ///< Initial prediction bias (Q15)
  // For multi-class, trees usually grouped: [tree0_class0, tree0_class1, ... tree1_class0...]
} eif_gbm_fixed_t;

/**
 * @brief Initialize GBM (Fixed Point)
 */
void eif_gbm_init_fixed(eif_gbm_fixed_t *gbm, 
                        int num_trees, 
                        int num_classes, 
                        const eif_gbm_tree_fixed_t *trees,
                        q15_t base_score);

/**
 * @brief Predict GBM (Fixed Point) - Binary Classification
 * Returns probability-like score or raw log-odds in Q15
 */
q15_t eif_gbm_predict_fixed(const eif_gbm_fixed_t *gbm, const q15_t *input);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_GRADIENT_BOOST_FIXED_H
