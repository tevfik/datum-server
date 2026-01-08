/**
 * @file eif_ml_rf_fixed.c
 * @brief Random Forest Classifier (Fixed Point) Implementation
 */

#include "eif_ml_rf_fixed.h"
#include <stdlib.h>
#include <string.h>

void eif_rf_init_fixed(eif_rf_fixed_t *rf, 
                       uint16_t num_trees, 
                       uint16_t num_classes, 
                       const eif_rf_tree_fixed_t *trees) {
    rf->num_trees = num_trees;
    rf->num_classes = num_classes;
    rf->trees = trees;
}

static int32_t predict_single_tree(const eif_rf_tree_fixed_t *tree, const q15_t *input) {
    // Start at root (index 0)
    int32_t current_node_idx = 0;
    
    // Limits traversal to max nodes avoiding infinite loops
    // In valid trees, we stop when split_feature is -1
    for (uint32_t i = 0; i < tree->num_nodes; i++) {
        const eif_rf_node_fixed_t *node = &tree->nodes[current_node_idx];
        
        // Leaf node check
        if (node->split_feature == -1) {
            return node->class_id;
        }
        
        // Decision split
        // Q15 fixed point comparison
        if (input[node->split_feature] <= node->split_value) {
            current_node_idx = node->left;
        } else {
            current_node_idx = node->right;
        }
        
        // Safety check for indices
        if (current_node_idx < 0 || (uint32_t)current_node_idx >= tree->num_nodes) {
            // Should not happen in valid models. Return majority or 0 default.
            return 0; 
        }
    }
    return 0; // Fallback
}

int32_t eif_rf_predict_fixed(const eif_rf_fixed_t *rf, const q15_t *input) {
    if (!rf || !input || !rf->trees) return -1;

    // Use a small array or malloc based on class count to store votes
    // Assuming num_classes is relatively small for embedded (e.g., < 32)
    // If larger, we should use dynamic allocation or provided scratch buffer
    
    #define MAX_STACK_CLASSES 32
    int32_t votes[MAX_STACK_CLASSES];
    
    if (rf->num_classes > MAX_STACK_CLASSES) {
        return -2; // Not supported without dynamic allocation
    }
    
    memset(votes, 0, sizeof(int32_t) * rf->num_classes);
    
    // Collect votes from all trees
    for (uint16_t i = 0; i < rf->num_trees; i++) {
        int32_t class_pred = predict_single_tree(&rf->trees[i], input);
        if (class_pred >= 0 && class_pred < rf->num_classes) {
            votes[class_pred]++;
        }
    }
    
    // Find Majority
    int32_t best_class = -1;
    int32_t max_votes = -1;
    
    for (int c = 0; c < rf->num_classes; c++) {
        if (votes[c] > max_votes) {
            max_votes = votes[c];
            best_class = c;
        }
    }
    
    return best_class;
}
