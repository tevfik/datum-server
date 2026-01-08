#!/usr/bin/env python3
"""
Train Activity Recognition Model for EIF

Trains a simple neural network for accelerometer-based activity recognition
and exports to EIF format.

Activities: walking, running, sitting, standing
Features: 3-axis accelerometer data (x, y, z)
"""

import numpy as np
from pathlib import Path
import struct

# Generate synthetic activity recognition data
def generate_activity_data(n_samples=1000):
    """Generate synthetic IMU data for 4 activities"""
    np.random.seed(42)
    
    data = []
    labels = []
    
    # Activity 0: Walking (periodic, moderate magnitude)
    for _ in range(n_samples // 4):
        t = np.random.rand() * 2 * np.pi
        x = np.sin(t) * 0.5 + np.random.randn() * 0.1
        y = np.cos(t) * 0.3 + np.random.randn() * 0.1
        z = 1.0 + np.sin(t) * 0.2 + np.random.randn() * 0.1
        data.append([x, y, z])
        labels.append(0)
    
    # Activity 1: Running (high frequency, high magnitude)
    for _ in range(n_samples // 4):
        t = np.random.rand() * 2 * np.pi
        x = np.sin(t * 2) * 1.0 + np.random.randn() * 0.2
        y = np.cos(t * 2) * 0.8 + np.random.randn() * 0.2
        z = 1.0 + np.sin(t * 2) * 0.5 + np.random.randn() * 0.2
        data.append([x, y, z])
        labels.append(1)
    
    # Activity 2: Sitting (static, low variance)
    for _ in range(n_samples // 4):
        x = 0.05 + np.random.randn() * 0.05
        y = 0.05 + np.random.randn() * 0.05
        z = 1.0 + np.random.randn() * 0.05
        data.append([x, y, z])
        labels.append(2)
    
    # Activity 3: Standing (static, slightly more variance)
    for _ in range(n_samples // 4):
        x = 0.1 + np.random.randn() * 0.08
        y = 0.1 + np.random.randn() * 0.08
        z = 1.0 + np.random.randn() * 0.08
        data.append([x, y, z])
        labels.append(3)
    
    return np.array(data, dtype=np.float32), np.array(labels, dtype=np.uint8)


def train_simple_nn(X, y):
    """Train simple 2-layer NN (no ML library needed)"""
    print("Training simple neural network...")
    
    # Network: 3 inputs -> 8 hidden (ReLU) -> 4 outputs (softmax)
    n_input = 3
    n_hidden = 8
    n_output = 4
    
    # Initialize weights (Xavier initialization)
    W1 = np.random.randn(n_input, n_hidden).astype(np.float32) * np.sqrt(2.0 / n_input)
    b1 = np.zeros(n_hidden, dtype=np.float32)
    W2 = np.random.randn(n_hidden, n_output).astype(np.float32) * np.sqrt(2.0 / n_hidden)
    b2 = np.zeros(n_output, dtype=np.float32)
    
    # Training loop (simplified gradient descent)
    learning_rate = 0.01
    epochs = 100
    
    for epoch in range(epochs):
        # Forward pass
        hidden = np.maximum(0, X @ W1 + b1)  # ReLU
        logits = hidden @ W2 + b2
        
        # Softmax
        exp_logits = np.exp(logits - np.max(logits, axis=1, keepdims=True))
        probs = exp_logits / np.sum(exp_logits, axis=1, keepdims=True)
        
        # Cross-entropy loss
        y_one_hot = np.zeros((len(y), n_output))
        y_one_hot[np.arange(len(y)), y] = 1
        loss = -np.mean(np.sum(y_one_hot * np.log(probs + 1e-8), axis=1))
        
        # Accuracy
        predictions = np.argmax(probs, axis=1)
        accuracy = np.mean(predictions == y)
        
        if epoch % 20 == 0:
            print(f"  Epoch {epoch}: Loss={loss:.4f}, Accuracy={accuracy:.2%}")
        
        # Backward pass (simplified)
        dL_dlogits = probs - y_one_hot
        dL_dW2 = hidden.T @ dL_dlogits / len(y)
        dL_db2 = np.mean(dL_dlogits, axis=0)
        
        dL_dhidden = dL_dlogits @ W2.T
        dL_dhidden[hidden <= 0] = 0  # ReLU derivative
        dL_dW1 = X.T @ dL_dhidden / len(y)
        dL_db1 = np.mean(dL_dhidden, axis=0)
        
        # Update weights
        W1 -= learning_rate * dL_dW1
        b1 -= learning_rate * dL_db1
        W2 -= learning_rate * dL_dW2
        b2 -= learning_rate * dL_db2
    
    print(f"  Final accuracy: {accuracy:.2%}")
    
    return {
        'W1': W1, 'b1': b1,
        'W2': W2, 'b2': b2,
        'architecture': [n_input, n_hidden, n_output]
    }


def export_to_eif_format(model, output_path):
    """Export model to simple EIF-compatible binary"""
    print(f"\nExporting model to {output_path}...")
    
    # Simple format: [magic][version][n_layers][layer_data...]
    MAGIC = b'EIFM'
    VERSION = struct.pack('I', 1)
    
    with open(output_path, 'wb') as f:
        # Header
        f.write(MAGIC)
        f.write(VERSION)
        
        # Layer count
        f.write(struct.pack('I', 2))  # 2 layers
        
        # Layer 1: Dense(3->8) + ReLU
        W1, b1 = model['W1'], model['b1']
        f.write(struct.pack('B', 0x01))  # LAYER_DENSE
        f.write(struct.pack('HH', 3, 8))  # in_features, out_features
        f.write(W1.tobytes())
        f.write(b1.tobytes())
        f.write(struct.pack('B', 1))  # has_activation (ReLU)
        
        # Layer 2: Dense(8->4) + Softmax
        W2, b2 = model['W2'], model['b2']
        f.write(struct.pack('B', 0x01))  # LAYER_DENSE
        f.write(struct.pack('HH', 8, 4))  # in_features, out_features
        f.write(W2.tobytes())
        f.write(b2.tobytes())
        f.write(struct.pack('B', 2))  # has_activation (Softmax)
    
    # Calculate model size
    model_size = (
        len(MAGIC) + 4 +  # header
        4 +  # layer count
        1 + 4 + W1.nbytes + b1.nbytes + 1 +  # layer 1
        1 + 4 + W2.nbytes + b2.nbytes + 1     # layer 2
    )
    
    print(f"  Model size: {model_size} bytes")
    print(f"  Parameters: {W1.size + b1.size + W2.size + b2.size}")
    
    # Save test data
    test_data_path = output_path.replace('.eif', '_test.npy')
    X_test = np.array([
        [0.4, 0.2, 1.1],  # Walking
        [1.2, 0.9, 1.3],  # Running
        [0.05, 0.05, 1.0],  # Sitting
        [0.1, 0.1, 1.0],   # Standing
    ], dtype=np.float32)
    np.save(test_data_path, X_test)
    print(f"  Test samples saved to: {test_data_path}")


def main():
    print("Activity Recognition Model Training")
    print("=" * 60)
    
    # Generate data
    print("Generating synthetic activity data...")
    X, y = generate_activity_data(n_samples=1000)
    print(f"  Dataset: {X.shape[0]} samples, {X.shape[1]} features")
    print(f"  Activities: walking(0), running(1), sitting(2), standing(3)")
    
    # Train model
    model = train_simple_nn(X, y)
    
    # Export
    output_path = Path(__file__).parent.parent / 'models' / 'activity_recognition.eif'
    output_path.parent.mkdir(exist_ok=True)
    export_to_eif_format(model, str(output_path))
    
    print("\n✓ Model training and export complete!")
    print(f"\nUsage:")
    print(f"  Load model: eif_model_load_from_file(\"{output_path.name}\")")
    print(f"  Input: [accel_x, accel_y, accel_z]")
    print(f"  Output: [prob_walk, prob_run, prob_sit, prob_stand]")


if __name__ == '__main__':
    main()
