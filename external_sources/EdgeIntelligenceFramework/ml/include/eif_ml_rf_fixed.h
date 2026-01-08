/**
 * @file eif_ml_rf_fixed.h
 * @brief Random Forest Classifier (Inference Only) - Fixed Point Q15
 */

#ifndef EIF_ML_RF_FIXED_H
#define EIF_ML_RF_FIXED_H

#include <stdint.h>
#include "eif_fixedpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Random Forest Node Structure (Fixed Point)
// ============================================================================

typedef struct {
  int16_t split_feature;   // Feature index for split (-1 for leaf)
  q15_t split_value;       // Threshold value for split (Q15)
  int32_t left;            // Left child node index
  int32_t right;           // Right child node index
  uint16_t class_id;       // Predicted class for leaf nodes (Majority)
  // Probabilities are omitted for pure inference to save space in fixed model
  // If needed, they could be distinct counts
} eif_rf_node_fixed_t;

// ============================================================================
// Random Forest Tree Structure (Fixed Point)
// ============================================================================

typedef struct {
  const eif_rf_node_fixed_t *nodes; // Array of nodes in Flash/ROM
  uint32_t num_nodes;         // Number of nodes
} eif_rf_tree_fixed_t;

// ============================================================================
// Random Forest Classifier Structure (Fixed Point)
// ============================================================================

typedef struct {
  uint16_t num_trees;           // Number of trees in forest
  uint16_t num_classes;         // Number of output classes
  const eif_rf_tree_fixed_t *trees;   // Array of trees in Flash/ROM
} eif_rf_fixed_t;

// ============================================================================
// Random Forest API
// ============================================================================

/**
 * @brief Initialize Random Forest classifier (Fixed Point)
 */
void eif_rf_init_fixed(eif_rf_fixed_t *rf, 
                       uint16_t num_trees, 
                       uint16_t num_classes, 
                       const eif_rf_tree_fixed_t *trees);

/**
 * @brief Predict class for Random Forest (Fixed Point)
 * Uses majority voting from all trees
 */
int32_t eif_rf_predict_fixed(const eif_rf_fixed_t *rf, const q15_t *input);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_RF_FIXED_H
