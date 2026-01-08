/**
 * @file eif_ml_rf.h
 * @brief Random Forest Classifier for Embedded Systems
 *
 * Memory-efficient Random Forest implementation optimized for
 * resource-constrained devices. Supports both classification
 * and out-of-bag error estimation.
 */

#ifndef EIF_ML_RF_H
#define EIF_ML_RF_H

#include "eif_memory.h"
#include "eif_status.h"
#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Random Forest Configuration
// ============================================================================

#define EIF_RF_MAX_DEPTH 12
#define EIF_RF_MIN_SAMPLES_SPLIT 2
#define EIF_RF_MIN_SAMPLES_LEAF 1

// ============================================================================
// Random Forest Node Structure
// ============================================================================

typedef struct {
  int16_t split_feature;   // Feature index for split (-1 for leaf)
  float32_t split_value;   // Threshold value for split
  int32_t left;            // Left child node index
  int32_t right;           // Right child node index
  uint16_t class_id;       // Predicted class for leaf nodes
  uint16_t num_samples;    // Number of samples in node
  float32_t *class_probs;  // Class probability distribution (num_classes)
} eif_rf_node_t;

// ============================================================================
// Random Forest Tree Structure
// ============================================================================

typedef struct {
  eif_rf_node_t *nodes;    // Array of nodes
  uint32_t num_nodes;      // Current number of nodes
  uint32_t max_nodes;      // Maximum number of nodes
  uint32_t *oob_samples;   // Out-of-bag sample indices
  uint32_t num_oob;        // Number of OOB samples
} eif_rf_tree_t;

// ============================================================================
// Random Forest Classifier Structure
// ============================================================================

typedef struct {
  uint16_t num_trees;           // Number of trees in forest
  uint16_t max_depth;           // Maximum tree depth
  uint16_t min_samples_split;   // Minimum samples to split node
  uint16_t min_samples_leaf;    // Minimum samples in leaf node
  uint16_t num_features;        // Number of input features
  uint16_t num_classes;         // Number of output classes
  uint16_t max_features;        // Max features per split (sqrt(n_features))
  eif_rf_tree_t *trees;         // Array of decision trees
  float32_t *feature_importance; // Feature importance scores
  uint32_t random_state;        // Random seed
  float32_t oob_score;          // Out-of-bag accuracy
} eif_rf_t;

// ============================================================================
// Random Forest API
// ============================================================================

/**
 * @brief Initialize Random Forest classifier
 *
 * @param rf Pointer to Random Forest structure
 * @param num_trees Number of decision trees
 * @param max_depth Maximum depth of each tree
 * @param min_samples_split Minimum samples required to split node
 * @param min_samples_leaf Minimum samples required in leaf node
 * @param num_features Number of input features
 * @param num_classes Number of output classes
 * @param pool Memory pool for allocations
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_rf_init(eif_rf_t *rf, uint16_t num_trees, uint16_t max_depth,
                         uint16_t min_samples_split, uint16_t min_samples_leaf,
                         uint16_t num_features, uint16_t num_classes,
                         eif_memory_pool_t *pool);

/**
 * @brief Train Random Forest on dataset
 *
 * @param rf Pointer to Random Forest structure
 * @param X Training data [num_samples x num_features]
 * @param y Training labels [num_samples]
 * @param num_samples Number of training samples
 * @param pool Memory pool for training
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_rf_fit(eif_rf_t *rf, const float32_t *X, const uint16_t *y,
                        uint32_t num_samples, eif_memory_pool_t *pool);

/**
 * @brief Predict class for single sample
 *
 * @param rf Pointer to trained Random Forest
 * @param x Input sample [num_features]
 * @param class_out Output class prediction
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_rf_predict(const eif_rf_t *rf, const float32_t *x,
                            uint16_t *class_out);

/**
 * @brief Predict class probabilities for single sample
 *
 * @param rf Pointer to trained Random Forest
 * @param x Input sample [num_features]
 * @param probs Output class probabilities [num_classes]
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_rf_predict_proba(const eif_rf_t *rf, const float32_t *x,
                                  float32_t *probs);

/**
 * @brief Predict classes for multiple samples
 *
 * @param rf Pointer to trained Random Forest
 * @param X Input samples [num_samples x num_features]
 * @param num_samples Number of samples to predict
 * @param y_pred Output predictions [num_samples]
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_rf_predict_batch(const eif_rf_t *rf, const float32_t *X,
                                  uint32_t num_samples, uint16_t *y_pred);

/**
 * @brief Get feature importance scores
 *
 * @param rf Pointer to trained Random Forest
 * @param importance Output array [num_features]
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_rf_get_feature_importance(const eif_rf_t *rf,
                                           float32_t *importance);

/**
 * @brief Get out-of-bag score
 *
 * @param rf Pointer to trained Random Forest
 * @param score Output OOB accuracy score
 * @return EIF_STATUS_OK on success
 */
eif_status_t eif_rf_get_oob_score(const eif_rf_t *rf, float32_t *score);

/**
 * @brief Clean up Random Forest resources
 *
 * @param rf Pointer to Random Forest structure
 */
void eif_rf_cleanup(eif_rf_t *rf);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_RF_H
