#!/usr/bin/env python3
"""
train_model.py - Train Activity Recognition Model for EIF

This script:
1. Loads labeled accelerometer data
2. Extracts features
3. Trains a Decision Tree classifier
4. Exports to C header file

Usage:
    python train_model.py --data sample_data.csv --output model_weights.h
    python train_model.py --demo  # Use synthetic data for demo
"""

import argparse
import numpy as np
from pathlib import Path

# Try to import sklearn, provide fallback
try:
    from sklearn.tree import DecisionTreeClassifier
    from sklearn.model_selection import train_test_split
    from sklearn.metrics import classification_report, confusion_matrix
    HAS_SKLEARN = True
except ImportError:
    HAS_SKLEARN = False
    print("Warning: scikit-learn not found. Install with: pip install scikit-learn")

# =============================================================================
# Constants
# =============================================================================

ACTIVITIES = ['stationary', 'walking', 'running', 'stairs']
WINDOW_SIZE = 128
SAMPLE_RATE = 50

FEATURE_NAMES = [
    'mean_x', 'mean_y', 'mean_z',
    'std_x', 'std_y', 'std_z',
    'magnitude_mean', 'magnitude_std',
    'sma', 'energy', 'zero_crossings', 'peak_frequency'
]

# =============================================================================
# Feature Extraction (Matches C implementation)
# =============================================================================

def extract_features(window):
    """
    Extract features from accelerometer window.
    
    Args:
        window: numpy array of shape (N, 3) - [ax, ay, az]
    
    Returns:
        features: numpy array of shape (12,)
    """
    ax = window[:, 0]
    ay = window[:, 1]
    az = window[:, 2]
    
    # Magnitude
    magnitude = np.sqrt(ax**2 + ay**2 + az**2)
    
    # Statistical features
    features = [
        np.mean(ax),                    # mean_x
        np.mean(ay),                    # mean_y
        np.mean(az),                    # mean_z
        np.std(ax),                     # std_x
        np.std(ay),                     # std_y
        np.std(az),                     # std_z
        np.mean(magnitude),             # magnitude_mean
        np.std(magnitude),              # magnitude_std
        np.mean(np.abs(ax) + np.abs(ay) + np.abs(az)),  # sma
        np.sum(magnitude**2) / len(magnitude),          # energy
        np.sum(np.diff(np.sign(magnitude - np.mean(magnitude))) != 0),  # zero_crossings
        estimate_peak_frequency(magnitude, SAMPLE_RATE) # peak_frequency
    ]
    
    return np.array(features)

def estimate_peak_frequency(signal, sample_rate):
    """Estimate dominant frequency using FFT."""
    fft = np.abs(np.fft.rfft(signal - np.mean(signal)))
    freqs = np.fft.rfftfreq(len(signal), 1/sample_rate)
    
    # Ignore DC and very high frequencies
    valid = (freqs > 0.5) & (freqs < sample_rate/2)
    if np.sum(valid) == 0:
        return 0.0
    
    peak_idx = np.argmax(fft[valid])
    return freqs[valid][peak_idx]

# =============================================================================
# Data Generation (for demo)
# =============================================================================

def generate_synthetic_data(n_samples_per_class=100):
    """Generate synthetic training data for demo."""
    np.random.seed(42)
    
    X = []
    y = []
    
    for label, activity in enumerate(ACTIVITIES):
        for _ in range(n_samples_per_class):
            window = generate_activity_window(activity)
            features = extract_features(window)
            X.append(features)
            y.append(label)
    
    return np.array(X), np.array(y)

def generate_activity_window(activity):
    """Generate synthetic accelerometer window for activity."""
    t = np.linspace(0, WINDOW_SIZE / SAMPLE_RATE, WINDOW_SIZE)
    noise = np.random.randn(WINDOW_SIZE, 3) * 0.1
    
    if activity == 'stationary':
        ax = np.zeros(WINDOW_SIZE) + noise[:, 0] * 0.1
        ay = np.zeros(WINDOW_SIZE) + noise[:, 1] * 0.1
        az = np.ones(WINDOW_SIZE) * 9.81 + noise[:, 2] * 0.2
        
    elif activity == 'walking':
        freq = 2.0 + np.random.randn() * 0.2
        ax = 0.5 * np.sin(2 * np.pi * freq * t) + noise[:, 0]
        ay = 0.3 * np.sin(4 * np.pi * freq * t) + noise[:, 1]
        az = 9.81 + 1.0 * np.abs(np.sin(4 * np.pi * freq * t)) + noise[:, 2]
        
    elif activity == 'running':
        freq = 3.0 + np.random.randn() * 0.3
        ax = 1.5 * np.sin(2 * np.pi * freq * t) + noise[:, 0] * 2
        ay = 1.0 * np.sin(4 * np.pi * freq * t) + noise[:, 1] * 2
        az = 9.81 + 3.0 * np.abs(np.sin(4 * np.pi * freq * t)) + noise[:, 2] * 2
        
    elif activity == 'stairs':
        freq = 1.5 + np.random.randn() * 0.1
        ax = 0.3 * np.sin(2 * np.pi * freq * t) + noise[:, 0]
        ay = 0.2 * np.sin(2 * np.pi * freq * t) + noise[:, 1]
        az = 9.81 + 1.5 * np.sin(2 * np.pi * freq * t) + noise[:, 2]
    
    return np.column_stack([ax, ay, az])

# =============================================================================
# Data Loading
# =============================================================================

def load_csv_data(filepath):
    """Load training data from CSV file."""
    import csv
    
    windows = {}  # label -> list of windows
    current_window = []
    current_label = None
    
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            ax = float(row['ax'])
            ay = float(row['ay'])
            az = float(row['az'])
            label = row['label'].strip().lower()
            
            if label != current_label and len(current_window) >= WINDOW_SIZE:
                # Save completed window
                if current_label is not None:
                    if current_label not in windows:
                        windows[current_label] = []
                    windows[current_label].append(
                        np.array(current_window[:WINDOW_SIZE])
                    )
                current_window = []
            
            current_label = label
            current_window.append([ax, ay, az])
    
    # Process windows to features
    X = []
    y = []
    for label, window_list in windows.items():
        label_idx = ACTIVITIES.index(label) if label in ACTIVITIES else -1
        if label_idx < 0:
            print(f"Warning: Unknown label '{label}', skipping")
            continue
            
        for window in window_list:
            features = extract_features(window)
            X.append(features)
            y.append(label_idx)
    
    return np.array(X), np.array(y)

# =============================================================================
# Model Export
# =============================================================================

def export_decision_tree_to_c(clf, output_path, feature_names):
    """Export decision tree to C header file."""
    from sklearn.tree import _tree
    
    tree_ = clf.tree_
    feature_name = feature_names
    
    with open(output_path, 'w') as f:
        f.write("/**\n")
        f.write(" * @file model_weights.h\n")
        f.write(" * @brief Auto-generated Decision Tree for Activity Recognition\n")
        f.write(" * \n")
        f.write(" * Generated by train_model.py\n")
        f.write(" * DO NOT EDIT MANUALLY\n")
        f.write(" */\n\n")
        f.write("#ifndef MODEL_WEIGHTS_H\n")
        f.write("#define MODEL_WEIGHTS_H\n\n")
        f.write("#include <stdint.h>\n\n")
        
        # Activity names
        f.write("// Activity labels\n")
        f.write("static const char* const MODEL_ACTIVITY_NAMES[] = {\n")
        for act in ACTIVITIES:
            f.write(f'    "{act}",\n')
        f.write("};\n")
        f.write(f"#define MODEL_NUM_ACTIVITIES {len(ACTIVITIES)}\n\n")
        
        # Feature indices
        f.write("// Feature indices\n")
        for i, name in enumerate(feature_names):
            f.write(f"#define FEAT_{name.upper()} {i}\n")
        f.write(f"#define MODEL_NUM_FEATURES {len(feature_names)}\n\n")
        
        # Generate decision function
        f.write("/**\n")
        f.write(" * @brief Classify activity from features\n")
        f.write(" * @param features Array of extracted features\n")
        f.write(" * @return Activity class index\n")
        f.write(" */\n")
        f.write("static inline int model_predict(const float* features) {\n")
        
        # Recursive tree traversal
        def recurse(node, depth):
            indent = "    " * (depth + 1)
            
            if tree_.feature[node] != _tree.TREE_UNDEFINED:
                # Decision node
                feat = feature_name[tree_.feature[node]]
                threshold = tree_.threshold[node]
                
                f.write(f"{indent}if (features[FEAT_{feat.upper()}] <= {threshold:.6f}f) {{\n")
                recurse(tree_.children_left[node], depth + 1)
                f.write(f"{indent}}} else {{\n")
                recurse(tree_.children_right[node], depth + 1)
                f.write(f"{indent}}}\n")
            else:
                # Leaf node
                value = tree_.value[node][0]
                class_id = np.argmax(value)
                f.write(f"{indent}return {class_id};  // {ACTIVITIES[class_id]}\n")
        
        recurse(0, 0)
        f.write("}\n\n")
        
        # Confidence estimation
        f.write("/**\n")
        f.write(" * @brief Get prediction confidence (simplified)\n")
        f.write(" * @param features Array of extracted features\n")
        f.write(" * @return Confidence 0.0 to 1.0\n")
        f.write(" */\n")
        f.write("static inline float model_confidence(const float* features) {\n")
        f.write("    // Simplified: higher magnitude_std = lower confidence\n")
        f.write("    float mag_std = features[FEAT_MAGNITUDE_STD];\n")
        f.write("    float conf = 1.0f - (mag_std > 5.0f ? 0.5f : mag_std / 10.0f);\n")
        f.write("    return conf < 0.5f ? 0.5f : conf;\n")
        f.write("}\n\n")
        
        f.write("#endif // MODEL_WEIGHTS_H\n")
    
    print(f"Model exported to: {output_path}")

# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='Train Activity Recognition Model')
    parser.add_argument('--data', type=str, help='Path to training CSV')
    parser.add_argument('--output', type=str, default='model_weights.h',
                        help='Output C header file')
    parser.add_argument('--demo', action='store_true',
                        help='Use synthetic demo data')
    parser.add_argument('--max-depth', type=int, default=8,
                        help='Maximum tree depth')
    args = parser.parse_args()
    
    if not HAS_SKLEARN:
        print("Error: scikit-learn is required. Install with: pip install scikit-learn")
        return 1
    
    print("=" * 60)
    print("EIF Activity Recognition Model Training")
    print("=" * 60)
    
    # Load or generate data
    if args.demo or args.data is None:
        print("\nGenerating synthetic training data...")
        X, y = generate_synthetic_data(n_samples_per_class=100)
    else:
        print(f"\nLoading data from: {args.data}")
        X, y = load_csv_data(args.data)
    
    print(f"Dataset: {len(X)} samples, {len(FEATURE_NAMES)} features")
    print(f"Classes: {ACTIVITIES}")
    
    # Split data
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )
    print(f"Train/Test split: {len(X_train)}/{len(X_test)}")
    
    # Train
    print(f"\nTraining Decision Tree (max_depth={args.max_depth})...")
    clf = DecisionTreeClassifier(
        max_depth=args.max_depth,
        random_state=42,
        min_samples_leaf=5
    )
    clf.fit(X_train, y_train)
    
    # Evaluate
    y_pred = clf.predict(X_test)
    accuracy = np.mean(y_pred == y_test)
    print(f"\nTest Accuracy: {accuracy * 100:.1f}%")
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred, target_names=ACTIVITIES))
    
    # Tree stats
    print(f"Tree depth: {clf.get_depth()}")
    print(f"Number of leaves: {clf.get_n_leaves()}")
    
    # Export
    print(f"\nExporting model to: {args.output}")
    export_decision_tree_to_c(clf, args.output, FEATURE_NAMES)
    
    # Estimate size
    n_nodes = clf.tree_.node_count
    estimated_size = n_nodes * 20  # ~20 bytes per node in C
    print(f"Estimated model size: ~{estimated_size} bytes")
    
    print("\n" + "=" * 60)
    print("Training complete!")
    print(f"Use {args.output} in your C/Arduino project")
    print("=" * 60)
    
    return 0

if __name__ == '__main__':
    exit(main())
