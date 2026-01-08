/**
 * @file eif_ml_gradient_boost_fixed.c
 * @brief Gradient Boosting (Fixed Point) Implementation
 */

#include "eif_ml_gradient_boost_fixed.h"
#include <stdlib.h>

void eif_gbm_init_fixed(eif_gbm_fixed_t *gbm, 
                        int num_trees, 
                        int num_classes, 
                        const eif_gbm_tree_fixed_t *trees,
                        q15_t base_score) {
    gbm->num_trees = num_trees;
    gbm->num_classes = num_classes;
    gbm->trees = trees;
    gbm->base_score = base_score;
}

static q15_t predict_tree(const eif_gbm_tree_fixed_t *tree, const q15_t *input) {
    int current_idx = tree->root;
    
    // Safety limit to prevent infinite loops in malformed trees
    int max_depth = 32; 
    
    while (max_depth-- > 0) {
        if (current_idx < 0 || current_idx >= tree->num_nodes) return 0; // Error
        
        const eif_gbm_node_fixed_t *node = &tree->nodes[current_idx];
        
        // Leaf check
        if (node->feature_idx == -1) {
            return node->leaf_value;
        }
        
        // Split
        if (input[node->feature_idx] <= node->threshold) {
            current_idx = node->left_child;
        } else {
            current_idx = node->right_child;
        }
    }
    return 0;
}

q15_t eif_gbm_predict_fixed(const eif_gbm_fixed_t *gbm, const q15_t *input) {
    if (!gbm || !input) return 0;
    
    // GBM is additive. 
    // Start with base score (often 0 or 0.5 converted to log-odds)
    // We use Q31 accumulator to prevent overflow during summation
    int32_t sum = gbm->base_score; 
    
    // For binary classification, we just sum up the trees
    // Multi-class would require loop over classes
    
    // Simplification: Binary classification only implemented here
    for (int i = 0; i < gbm->num_trees; i++) {
        sum += predict_tree(&gbm->trees[i], input);
    }
    
    // Saturation logic for Q15 return
    if (sum > 32767) sum = 32767;
    if (sum < -32768) sum = -32768;
    
    return (q15_t)sum;
}
