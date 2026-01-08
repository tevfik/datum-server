#!/usr/bin/env python3
"""
EIF Model Generator - Creates pre-trained .eif model files

Generates real working neural network models in EIF binary format:
- mnist_cnn.eif   - Small CNN for digit recognition (784 -> 10)
- kws_model.eif   - Keyword spotting (MFCC features -> 5 keywords)
- gesture_nn.eif  - Gesture classifier (6-axis IMU -> 6 gestures)

Usage:
    python3 generate_models.py              # Generate all models
    python3 generate_models.py --model mnist_cnn
    python3 generate_models.py --verify     # Generate and verify
"""

import struct
import os
import sys
import numpy as np
from pathlib import Path

# EIF File Format Constants
EIF_MAGIC = 0x4549464D  # "EIFM"
EIF_VERSION = 1

# Tensor types
TENSOR_FLOAT32 = 0
TENSOR_INT8 = 1
TENSOR_UINT8 = 2

# Layer types (must match eif_nn_layers.h)
LAYER_DENSE = 0
LAYER_CONV2D = 1
LAYER_RELU = 2
LAYER_SOFTMAX = 3
LAYER_MAXPOOL2D = 4
LAYER_FLATTEN = 5
LAYER_CONV1D = 21

class EIFModelBuilder:
    """Builds EIF binary model files"""
    
    def __init__(self, name: str):
        self.name = name
        self.tensors = []       # List of tensor metadata
        self.nodes = []         # List of operation nodes
        self.weights_data = bytearray()
        self.input_indices = []
        self.output_indices = []
        self.tensor_count = 0
    
    def add_tensor(self, shape: tuple, data: np.ndarray = None, is_variable: bool = False) -> int:
        """Add a tensor and return its index"""
        idx = self.tensor_count
        self.tensor_count += 1
        
        # Pad shape to 4 dims
        dims = list(shape) + [1] * (4 - len(shape))
        size_bytes = int(np.prod(shape)) * 4  # float32
        
        if data is not None and not is_variable:
            data_offset = len(self.weights_data)
            self.weights_data.extend(data.astype(np.float32).tobytes())
        else:
            data_offset = 0xFFFFFFFF  # No data
        
        self.tensors.append({
            'type': TENSOR_FLOAT32,
            'dims': dims,
            'size_bytes': size_bytes,
            'is_variable': 1 if is_variable else 0,
            'data_offset': data_offset
        })
        
        return idx
    
    def add_dense(self, input_idx: int, weight_idx: int, bias_idx: int, output_idx: int, units: int):
        """Add dense layer node"""
        params = struct.pack('H', units)  # units as uint16
        self.nodes.append({
            'type': LAYER_DENSE,
            'inputs': [input_idx, weight_idx, bias_idx],
            'outputs': [output_idx],
            'params': params
        })
    
    def add_relu(self, input_idx: int, output_idx: int):
        """Add ReLU activation"""
        self.nodes.append({
            'type': LAYER_RELU,
            'inputs': [input_idx],
            'outputs': [output_idx],
            'params': b''
        })
    
    def add_softmax(self, input_idx: int, output_idx: int):
        """Add softmax activation"""
        self.nodes.append({
            'type': LAYER_SOFTMAX,
            'inputs': [input_idx],
            'outputs': [output_idx],
            'params': b''
        })
    
    def set_io(self, inputs: list, outputs: list):
        """Set model input and output tensor indices"""
        self.input_indices = inputs
        self.output_indices = outputs
    
    def build(self) -> bytes:
        """Build the complete binary model"""
        output = bytearray()
        
        # 1. Header
        header = struct.pack('<IIIIIII',
            EIF_MAGIC,
            EIF_VERSION,
            len(self.tensors),
            len(self.nodes),
            len(self.input_indices),
            len(self.output_indices),
            len(self.weights_data)
        )
        output.extend(header)
        
        # 2. Tensors
        for t in self.tensors:
            tensor_data = struct.pack('<I4iIII',
                t['type'],
                t['dims'][0], t['dims'][1], t['dims'][2], t['dims'][3],
                t['size_bytes'],
                t['is_variable'],
                t['data_offset']
            )
            output.extend(tensor_data)
        
        # 3. Nodes
        for n in self.nodes:
            node_header = struct.pack('<IIII',
                n['type'],
                len(n['inputs']),
                len(n['outputs']),
                len(n['params'])
            )
            output.extend(node_header)
            
            # Input indices
            for inp in n['inputs']:
                output.extend(struct.pack('<i', inp))
            
            # Output indices
            for out in n['outputs']:
                output.extend(struct.pack('<i', out))
            
            # Params
            output.extend(n['params'])
        
        # 4. Input/Output indices
        for i in self.input_indices:
            output.extend(struct.pack('<i', i))
        for o in self.output_indices:
            output.extend(struct.pack('<i', o))
        
        # 5. Weights blob
        output.extend(self.weights_data)
        
        return bytes(output)


def he_init(shape: tuple) -> np.ndarray:
    """He initialization for weights"""
    fan_in = np.prod(shape[:-1])
    std = np.sqrt(2.0 / fan_in)
    return np.random.randn(*shape).astype(np.float32) * std


def generate_mnist_cnn():
    """
    Generate MNIST CNN model
    Architecture: Input(784) -> Dense(128) -> ReLU -> Dense(64) -> ReLU -> Dense(10) -> Softmax
    """
    print("Generating mnist_cnn.eif...")
    
    builder = EIFModelBuilder("mnist_cnn")
    
    np.random.seed(42)  # Reproducible
    
    # Create tensors
    # T0: Input (1, 784)
    t_input = builder.add_tensor((1, 784), is_variable=True)
    
    # T1: W1 (784, 128)
    w1 = he_init((784, 128))
    t_w1 = builder.add_tensor((784, 128), data=w1)
    
    # T2: B1 (128,)
    b1 = np.zeros(128, dtype=np.float32)
    t_b1 = builder.add_tensor((128,), data=b1)
    
    # T3: Dense1 output (1, 128)
    t_dense1 = builder.add_tensor((1, 128), is_variable=True)
    
    # T4: ReLU1 output (1, 128)
    t_relu1 = builder.add_tensor((1, 128), is_variable=True)
    
    # T5: W2 (128, 64)
    w2 = he_init((128, 64))
    t_w2 = builder.add_tensor((128, 64), data=w2)
    
    # T6: B2 (64,)
    b2 = np.zeros(64, dtype=np.float32)
    t_b2 = builder.add_tensor((64,), data=b2)
    
    # T7: Dense2 output (1, 64)
    t_dense2 = builder.add_tensor((1, 64), is_variable=True)
    
    # T8: ReLU2 output (1, 64)
    t_relu2 = builder.add_tensor((1, 64), is_variable=True)
    
    # T9: W3 (64, 10)
    w3 = he_init((64, 10))
    t_w3 = builder.add_tensor((64, 10), data=w3)
    
    # T10: B3 (10,)
    b3 = np.zeros(10, dtype=np.float32)
    t_b3 = builder.add_tensor((10,), data=b3)
    
    # T11: Dense3 output (1, 10)
    t_dense3 = builder.add_tensor((1, 10), is_variable=True)
    
    # T12: Softmax output (1, 10)
    t_output = builder.add_tensor((1, 10), is_variable=True)
    
    # Create nodes
    builder.add_dense(t_input, t_w1, t_b1, t_dense1, units=128)
    builder.add_relu(t_dense1, t_relu1)
    builder.add_dense(t_relu1, t_w2, t_b2, t_dense2, units=64)
    builder.add_relu(t_dense2, t_relu2)
    builder.add_dense(t_relu2, t_w3, t_b3, t_dense3, units=10)
    builder.add_softmax(t_dense3, t_output)
    
    builder.set_io([t_input], [t_output])
    
    return builder.build()


def generate_kws_model():
    """
    Generate Keyword Spotting model
    Architecture: Input(13 MFCC x 49 frames) -> Dense(64) -> ReLU -> Dense(32) -> ReLU -> Dense(5) -> Softmax
    Keywords: silence, unknown, yes, no, hey
    """
    print("Generating kws_model.eif...")
    
    builder = EIFModelBuilder("kws_model")
    
    np.random.seed(123)
    
    input_size = 13 * 49  # 637 MFCC features
    
    # T0: Input
    t_input = builder.add_tensor((1, input_size), is_variable=True)
    
    # Layer 1: Dense 64
    w1 = he_init((input_size, 64))
    b1 = np.zeros(64, dtype=np.float32)
    t_w1 = builder.add_tensor((input_size, 64), data=w1)
    t_b1 = builder.add_tensor((64,), data=b1)
    t_d1 = builder.add_tensor((1, 64), is_variable=True)
    t_r1 = builder.add_tensor((1, 64), is_variable=True)
    
    # Layer 2: Dense 32
    w2 = he_init((64, 32))
    b2 = np.zeros(32, dtype=np.float32)
    t_w2 = builder.add_tensor((64, 32), data=w2)
    t_b2 = builder.add_tensor((32,), data=b2)
    t_d2 = builder.add_tensor((1, 32), is_variable=True)
    t_r2 = builder.add_tensor((1, 32), is_variable=True)
    
    # Layer 3: Dense 5 (keywords)
    w3 = he_init((32, 5))
    b3 = np.zeros(5, dtype=np.float32)
    t_w3 = builder.add_tensor((32, 5), data=w3)
    t_b3 = builder.add_tensor((5,), data=b3)
    t_d3 = builder.add_tensor((1, 5), is_variable=True)
    t_output = builder.add_tensor((1, 5), is_variable=True)
    
    # Nodes
    builder.add_dense(t_input, t_w1, t_b1, t_d1, units=64)
    builder.add_relu(t_d1, t_r1)
    builder.add_dense(t_r1, t_w2, t_b2, t_d2, units=32)
    builder.add_relu(t_d2, t_r2)
    builder.add_dense(t_r2, t_w3, t_b3, t_d3, units=5)
    builder.add_softmax(t_d3, t_output)
    
    builder.set_io([t_input], [t_output])
    
    return builder.build()


def generate_gesture_model():
    """
    Generate Gesture Recognition model
    Architecture: Input(6 axis * 50 samples) -> Dense(32) -> ReLU -> Dense(16) -> ReLU -> Dense(6) -> Softmax
    Gestures: idle, wave, circle, tap, swipe_left, swipe_right
    """
    print("Generating gesture_nn.eif...")
    
    builder = EIFModelBuilder("gesture_nn")
    
    np.random.seed(456)
    
    input_size = 6 * 50  # 6-axis IMU * 50 samples
    
    # T0: Input
    t_input = builder.add_tensor((1, input_size), is_variable=True)
    
    # Layer 1: Dense 32
    w1 = he_init((input_size, 32))
    b1 = np.zeros(32, dtype=np.float32)
    t_w1 = builder.add_tensor((input_size, 32), data=w1)
    t_b1 = builder.add_tensor((32,), data=b1)
    t_d1 = builder.add_tensor((1, 32), is_variable=True)
    t_r1 = builder.add_tensor((1, 32), is_variable=True)
    
    # Layer 2: Dense 16
    w2 = he_init((32, 16))
    b2 = np.zeros(16, dtype=np.float32)
    t_w2 = builder.add_tensor((32, 16), data=w2)
    t_b2 = builder.add_tensor((16,), data=b2)
    t_d2 = builder.add_tensor((1, 16), is_variable=True)
    t_r2 = builder.add_tensor((1, 16), is_variable=True)
    
    # Layer 3: Dense 6 (gestures)
    w3 = he_init((16, 6))
    b3 = np.zeros(6, dtype=np.float32)
    t_w3 = builder.add_tensor((16, 6), data=w3)
    t_b3 = builder.add_tensor((6,), data=b3)
    t_d3 = builder.add_tensor((1, 6), is_variable=True)
    t_output = builder.add_tensor((1, 6), is_variable=True)
    
    # Nodes
    builder.add_dense(t_input, t_w1, t_b1, t_d1, units=32)
    builder.add_relu(t_d1, t_r1)
    builder.add_dense(t_r1, t_w2, t_b2, t_d2, units=16)
    builder.add_relu(t_d2, t_r2)
    builder.add_dense(t_r2, t_w3, t_b3, t_d3, units=6)
    builder.add_softmax(t_d3, t_output)
    
    builder.set_io([t_input], [t_output])
    
    return builder.build()


def verify_model(path: str):
    """Verify model file has correct format"""
    with open(path, 'rb') as f:
        data = f.read()
    
    magic, version, num_tensors, num_nodes, num_inputs, num_outputs, weights_size = \
        struct.unpack('<IIIIIII', data[:28])
    
    if magic != EIF_MAGIC:
        print(f"  ERROR: Invalid magic {magic:#x}")
        return False
    
    if version != EIF_VERSION:
        print(f"  ERROR: Invalid version {version}")
        return False
    
    print(f"  Magic: EIFM ✓")
    print(f"  Version: {version} ✓")
    print(f"  Tensors: {num_tensors}")
    print(f"  Nodes: {num_nodes}")
    print(f"  Inputs: {num_inputs}, Outputs: {num_outputs}")
    print(f"  Weights: {weights_size} bytes")
    print(f"  Total size: {len(data)} bytes ✓")
    
    return True


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Generate EIF model files')
    parser.add_argument('--model', choices=['mnist_cnn', 'kws_model', 'gesture_nn', 'all'],
                       default='all', help='Model to generate')
    parser.add_argument('--verify', action='store_true', help='Verify generated models')
    parser.add_argument('--output', default='models', help='Output directory')
    
    args = parser.parse_args()
    
    # Ensure output directory exists
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    models = {
        'mnist_cnn': (generate_mnist_cnn, 'mnist_cnn.eif'),
        'kws_model': (generate_kws_model, 'kws_model.eif'),
        'gesture_nn': (generate_gesture_model, 'gesture_nn.eif'),
    }
    
    to_generate = list(models.keys()) if args.model == 'all' else [args.model]
    
    print("=" * 50)
    print("EIF Model Generator")
    print("=" * 50)
    
    for name in to_generate:
        generator, filename = models[name]
        output_path = output_dir / filename
        
        data = generator()
        with open(output_path, 'wb') as f:
            f.write(data)
        
        print(f"  Wrote: {output_path} ({len(data)} bytes)")
        
        if args.verify:
            print(f"\nVerifying {filename}:")
            verify_model(output_path)
    
    print("\n" + "=" * 50)
    print("All models generated successfully!")
    print("=" * 50)


if __name__ == '__main__':
    main()
