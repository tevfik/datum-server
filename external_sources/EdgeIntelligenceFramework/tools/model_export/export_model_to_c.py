#!/usr/bin/env python3
"""
export_model_to_c.py - Export trained models to C header files

Supports:
- Decision Trees (scikit-learn)
- Random Forest (scikit-learn)
- Simple Neural Networks (weights only)

Usage:
    python export_model_to_c.py --model model.pkl --output model_weights.h
    python export_model_to_c.py --demo  # Generate example
"""

import argparse
import pickle
import numpy as np
from pathlib import Path

# =============================================================================
# Decision Tree Export
# =============================================================================

def export_decision_tree(clf, output_path, class_names=None, feature_names=None):
    """Export scikit-learn DecisionTreeClassifier to C header."""
    from sklearn.tree import _tree
    
    tree_ = clf.tree_
    n_classes = tree_.n_classes[0]
    n_features = tree_.n_features
    
    if class_names is None:
        class_names = [f"class_{i}" for i in range(n_classes)]
    if feature_names is None:
        feature_names = [f"feature_{i}" for i in range(n_features)]
    
    with open(output_path, 'w') as f:
        _write_header(f, "Decision Tree Classifier")
        
        # Metadata
        f.write(f"#define MODEL_NUM_FEATURES {n_features}\n")
        f.write(f"#define MODEL_NUM_CLASSES {n_classes}\n")
        f.write(f"#define MODEL_TREE_DEPTH {clf.get_depth()}\n")
        f.write(f"#define MODEL_NUM_NODES {tree_.node_count}\n\n")
        
        # Class names
        f.write("static const char* const MODEL_CLASS_NAMES[] = {\n")
        for name in class_names:
            f.write(f'    "{name}",\n')
        f.write("};\n\n")
        
        # Feature indices
        for i, name in enumerate(feature_names):
            safe_name = name.upper().replace(' ', '_')
            f.write(f"#define FEAT_{safe_name} {i}\n")
        f.write("\n")
        
        # Predict function
        f.write("/**\n * @brief Predict class from features\n */\n")
        f.write("static inline int model_predict(const float* features) {\n")
        
        _write_tree_recursive(f, tree_, feature_names, class_names, 0, 1)
        
        f.write("}\n\n")
        _write_footer(f)
    
    print(f"Decision Tree exported to: {output_path}")

def _write_tree_recursive(f, tree_, feature_names, class_names, node_id, depth):
    """Recursively write decision tree as nested if-else."""
    from sklearn.tree import _tree
    
    indent = "    " * depth
    
    if tree_.feature[node_id] != _tree.TREE_UNDEFINED:
        feat_idx = tree_.feature[node_id]
        feat_name = feature_names[feat_idx].upper().replace(' ', '_')
        threshold = tree_.threshold[node_id]
        
        f.write(f"{indent}if (features[FEAT_{feat_name}] <= {threshold:.6f}f) {{\n")
        _write_tree_recursive(f, tree_, feature_names, class_names, tree_.children_left[node_id], depth + 1)
        f.write(f"{indent}}} else {{\n")
        _write_tree_recursive(f, tree_, feature_names, class_names, tree_.children_right[node_id], depth + 1)
        f.write(f"{indent}}}\n")
    else:
        # Leaf node
        value = tree_.value[node_id][0]
        class_id = np.argmax(value)
        f.write(f"{indent}return {class_id};  // {class_names[class_id]}\n")

# =============================================================================
# Neural Network Export
# =============================================================================

def export_dense_network(weights, biases, output_path, activation='relu'):
    """
    Export simple dense neural network to C.
    
    Args:
        weights: list of numpy arrays, one per layer
        biases: list of numpy arrays, one per layer
        output_path: output file path
        activation: 'relu' or 'sigmoid'
    """
    n_layers = len(weights)
    
    with open(output_path, 'w') as f:
        _write_header(f, "Dense Neural Network")
        
        # Metadata
        f.write(f"#define MODEL_NUM_LAYERS {n_layers}\n")
        f.write(f"#define MODEL_INPUT_SIZE {weights[0].shape[0]}\n")
        f.write(f"#define MODEL_OUTPUT_SIZE {weights[-1].shape[1]}\n\n")
        
        # Find max layer size for temp buffer
        max_size = max(max(w.shape[0], w.shape[1]) for w in weights)
        f.write(f"#define MODEL_MAX_LAYER_SIZE {max_size}\n\n")
        
        # Write weights and biases
        for i, (w, b) in enumerate(zip(weights, biases)):
            _write_float_array(f, f"layer{i}_weights", w.flatten(), 
                             f"Shape: {w.shape[0]}x{w.shape[1]}")
            _write_float_array(f, f"layer{i}_bias", b.flatten())
        
        # Layer sizes
        f.write("static const int layer_sizes[] = {\n    ")
        f.write(f"{weights[0].shape[0]}, ")  # Input
        for w in weights:
            f.write(f"{w.shape[1]}, ")
        f.write("\n};\n\n")
        
        # Activation function
        if activation == 'relu':
            f.write("static inline float activation(float x) {\n")
            f.write("    return x > 0 ? x : 0;\n")
            f.write("}\n\n")
        else:
            f.write("static inline float activation(float x) {\n")
            f.write("    return 1.0f / (1.0f + expf(-x));\n")
            f.write("}\n\n")
        
        # Softmax for output
        f.write("static inline void softmax(float* x, int n) {\n")
        f.write("    float max_val = x[0];\n")
        f.write("    for (int i = 1; i < n; i++) if (x[i] > max_val) max_val = x[i];\n")
        f.write("    float sum = 0;\n")
        f.write("    for (int i = 0; i < n; i++) { x[i] = expf(x[i] - max_val); sum += x[i]; }\n")
        f.write("    for (int i = 0; i < n; i++) x[i] /= sum;\n")
        f.write("}\n\n")
        
        # Forward pass
        f.write("/**\n * @brief Run inference\n")
        f.write(" * @param input Input features\n")
        f.write(" * @param output Output probabilities\n")
        f.write(" */\n")
        f.write("static inline void model_forward(const float* input, float* output) {\n")
        f.write("    float buffer1[MODEL_MAX_LAYER_SIZE];\n")
        f.write("    float buffer2[MODEL_MAX_LAYER_SIZE];\n")
        f.write("    float* in_buf = buffer1;\n")
        f.write("    float* out_buf = buffer2;\n\n")
        f.write("    // Copy input\n")
        f.write("    for (int i = 0; i < MODEL_INPUT_SIZE; i++) in_buf[i] = input[i];\n\n")
        
        for i, (w, b) in enumerate(zip(weights, biases)):
            in_size, out_size = w.shape
            is_last = (i == n_layers - 1)
            
            f.write(f"    // Layer {i}: {in_size} -> {out_size}\n")
            f.write(f"    for (int j = 0; j < {out_size}; j++) {{\n")
            f.write(f"        float sum = layer{i}_bias[j];\n")
            f.write(f"        for (int k = 0; k < {in_size}; k++) {{\n")
            f.write(f"            sum += in_buf[k] * layer{i}_weights[k * {out_size} + j];\n")
            f.write(f"        }}\n")
            if is_last:
                f.write(f"        out_buf[j] = sum;  // No activation on output\n")
            else:
                f.write(f"        out_buf[j] = activation(sum);\n")
            f.write(f"    }}\n")
            f.write(f"    {{ float* tmp = in_buf; in_buf = out_buf; out_buf = tmp; }}\n\n")
        
        f.write(f"    softmax(in_buf, MODEL_OUTPUT_SIZE);\n")
        f.write(f"    for (int i = 0; i < MODEL_OUTPUT_SIZE; i++) output[i] = in_buf[i];\n")
        f.write("}\n\n")
        
        # Predict function
        f.write("static inline int model_predict(const float* input) {\n")
        f.write("    float output[MODEL_OUTPUT_SIZE];\n")
        f.write("    model_forward(input, output);\n")
        f.write("    int best = 0;\n")
        f.write("    for (int i = 1; i < MODEL_OUTPUT_SIZE; i++)\n")
        f.write("        if (output[i] > output[best]) best = i;\n")
        f.write("    return best;\n")
        f.write("}\n\n")
        
        _write_footer(f)
    
    print(f"Neural Network exported to: {output_path}")

# =============================================================================
# Utilities
# =============================================================================

def _write_header(f, model_type):
    f.write("/**\n")
    f.write(f" * @file Auto-generated {model_type}\n")
    f.write(" * @brief Machine learning model for embedded inference\n")
    f.write(" * \n")
    f.write(" * Generated by export_model_to_c.py\n")
    f.write(" * DO NOT EDIT MANUALLY\n")
    f.write(" */\n\n")
    f.write("#ifndef MODEL_WEIGHTS_H\n")
    f.write("#define MODEL_WEIGHTS_H\n\n")
    f.write("#include <stdint.h>\n")
    f.write("#include <math.h>\n\n")

def _write_footer(f):
    f.write("#endif // MODEL_WEIGHTS_H\n")

def _write_float_array(f, name, data, comment=None):
    if comment:
        f.write(f"// {comment}\n")
    f.write(f"static const float {name}[{len(data)}] = {{\n    ")
    for i, val in enumerate(data):
        f.write(f"{val:.8f}f")
        if i < len(data) - 1:
            f.write(", ")
        if (i + 1) % 8 == 0 and i < len(data) - 1:
            f.write("\n    ")
    f.write("\n};\n\n")

# =============================================================================
# Demo
# =============================================================================

def generate_demo():
    """Generate demo model for testing."""
    print("Generating demo model...")
    
    try:
        from sklearn.tree import DecisionTreeClassifier
        from sklearn.datasets import make_classification
        
        # Generate synthetic data
        X, y = make_classification(n_samples=200, n_features=5, 
                                  n_classes=3, n_informative=3,
                                  random_state=42)
        
        # Train
        clf = DecisionTreeClassifier(max_depth=4, random_state=42)
        clf.fit(X, y)
        
        # Export
        export_decision_tree(
            clf, 
            'demo_model.h',
            class_names=['class_A', 'class_B', 'class_C'],
            feature_names=['feature_1', 'feature_2', 'feature_3', 'feature_4', 'feature_5']
        )
        
        print("Demo model saved to: demo_model.h")
        
    except ImportError:
        print("scikit-learn not found. Generating neural network demo instead...")
        
        # Simple 5->10->3 network
        np.random.seed(42)
        weights = [
            np.random.randn(5, 10).astype(np.float32) * 0.1,
            np.random.randn(10, 3).astype(np.float32) * 0.1
        ]
        biases = [
            np.zeros(10, dtype=np.float32),
            np.zeros(3, dtype=np.float32)
        ]
        
        export_dense_network(weights, biases, 'demo_model.h')
        print("Demo neural network saved to: demo_model.h")

# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='Export ML models to C')
    parser.add_argument('--model', type=str, help='Path to pickled model')
    parser.add_argument('--output', type=str, default='model_weights.h',
                        help='Output C header file')
    parser.add_argument('--demo', action='store_true',
                        help='Generate demo model')
    args = parser.parse_args()
    
    if args.demo:
        generate_demo()
        return 0
    
    if args.model is None:
        print("Error: --model required (or use --demo)")
        return 1
    
    # Load model
    with open(args.model, 'rb') as f:
        model = pickle.load(f)
    
    # Detect model type
    model_type = type(model).__name__
    print(f"Model type: {model_type}")
    
    if 'DecisionTree' in model_type:
        export_decision_tree(model, args.output)
    elif 'RandomForest' in model_type:
        print("Random Forest: Exporting first tree as representative")
        export_decision_tree(model.estimators_[0], args.output)
    else:
        print(f"Unsupported model type: {model_type}")
        return 1
    
    return 0

if __name__ == '__main__':
    exit(main())
