#!/usr/bin/env python3
"""
EIF Model Compression Toolkit

Features:
- Post-Training Quantization (PTQ): INT8, INT4
- Weight Pruning: Magnitude-based, Structured
- Knowledge Distillation Helper
- Model Size Analysis

Usage:
    python model_compress.py --input model.onnx --output model_int8.h --quantize int8
    python model_compress.py --input model.onnx --prune 0.5 --output model_pruned.h
"""

import argparse
import numpy as np
import struct
import json
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass

# Optional ONNX support
try:
    import onnx
    from onnx import numpy_helper
    ONNX_AVAILABLE = True
except ImportError:
    ONNX_AVAILABLE = False


# =============================================================================
# Data Classes
# =============================================================================

@dataclass
class TensorInfo:
    name: str
    shape: Tuple[int, ...]
    dtype: str
    data: np.ndarray
    scale: float = 1.0
    zero_point: int = 0

@dataclass
class CompressionStats:
    original_size: int
    compressed_size: int
    compression_ratio: float
    num_params: int
    num_pruned: int
    quantization: str

# =============================================================================
# Quantization Functions
# =============================================================================

def quantize_tensor_int8(tensor: np.ndarray, symmetric: bool = True) -> Tuple[np.ndarray, float, int]:
    """
    Quantize a float tensor to INT8.
    
    Args:
        tensor: Float32 tensor
        symmetric: Use symmetric quantization (zero_point = 0)
    
    Returns:
        Tuple of (quantized_tensor, scale, zero_point)
    """
    if symmetric:
        max_abs = np.max(np.abs(tensor))
        scale = max_abs / 127.0 if max_abs > 0 else 1.0
        zero_point = 0
        quantized = np.round(tensor / scale).astype(np.int8)
    else:
        min_val, max_val = tensor.min(), tensor.max()
        scale = (max_val - min_val) / 255.0 if max_val > min_val else 1.0
        zero_point = int(np.round(-min_val / scale))
        zero_point = np.clip(zero_point, 0, 255)
        quantized = np.round(tensor / scale + zero_point).astype(np.int8)
    
    return quantized, scale, zero_point

def quantize_tensor_int4(tensor: np.ndarray) -> Tuple[np.ndarray, float, int]:
    """
    Quantize a float tensor to INT4 (packed into INT8).
    
    Args:
        tensor: Float32 tensor
    
    Returns:
        Tuple of (quantized_tensor, scale, zero_point)
    """
    max_abs = np.max(np.abs(tensor))
    scale = max_abs / 7.0 if max_abs > 0 else 1.0
    
    # Quantize to int4 range [-8, 7]
    quantized = np.round(tensor / scale).astype(np.int8)
    quantized = np.clip(quantized, -8, 7)
    
    # Pack two int4 values into one int8
    flat = quantized.flatten()
    if len(flat) % 2 != 0:
        flat = np.append(flat, 0)
    
    packed = np.zeros(len(flat) // 2, dtype=np.uint8)
    for i in range(len(packed)):
        low = flat[2*i] & 0x0F
        high = (flat[2*i + 1] & 0x0F) << 4
        packed[i] = low | high
    
    return packed, scale, 0

def dequantize_tensor(quantized: np.ndarray, scale: float, zero_point: int) -> np.ndarray:
    """Dequantize a quantized tensor back to float."""
    return (quantized.astype(np.float32) - zero_point) * scale

# =============================================================================
# Pruning Functions
# =============================================================================

def prune_magnitude(tensor: np.ndarray, sparsity: float) -> Tuple[np.ndarray, np.ndarray]:
    """
    Prune weights by magnitude (unstructured).
    
    Args:
        tensor: Weight tensor
        sparsity: Target sparsity (0.0 to 1.0)
    
    Returns:
        Tuple of (pruned_tensor, mask)
    """
    threshold = np.percentile(np.abs(tensor), sparsity * 100)
    mask = np.abs(tensor) >= threshold
    pruned = tensor * mask
    return pruned, mask

def prune_structured(tensor: np.ndarray, sparsity: float, granularity: str = 'channel') -> np.ndarray:
    """
    Prune weights with structure (channels, filters).
    
    Args:
        tensor: Weight tensor [out, in, h, w] or [out, in]
        sparsity: Target sparsity
        granularity: 'channel' or 'filter'
    
    Returns:
        Pruned tensor with zeroed channels/filters
    """
    if len(tensor.shape) < 2:
        return tensor
    
    if granularity == 'channel':
        # Prune input channels
        channel_norms = np.linalg.norm(tensor, axis=tuple(range(1, len(tensor.shape))))
        num_to_prune = int(sparsity * len(channel_norms))
        indices = np.argsort(channel_norms)[:num_to_prune]
        tensor[indices] = 0
    elif granularity == 'filter':
        # Prune output filters
        filter_norms = np.linalg.norm(tensor.reshape(tensor.shape[0], -1), axis=1)
        num_to_prune = int(sparsity * len(filter_norms))
        indices = np.argsort(filter_norms)[:num_to_prune]
        tensor[indices] = 0
    
    return tensor

# =============================================================================
# Export Functions
# =============================================================================

def export_to_c_header(tensors: List[TensorInfo], output_path: str, model_name: str = "model"):
    """Export quantized tensors to C header file."""
    
    with open(output_path, 'w') as f:
        f.write(f"// Auto-generated by EIF Model Compression Toolkit\n")
        f.write(f"// Model: {model_name}\n\n")
        f.write(f"#ifndef {model_name.upper()}_WEIGHTS_H\n")
        f.write(f"#define {model_name.upper()}_WEIGHTS_H\n\n")
        f.write("#include <stdint.h>\n\n")
        
        for tensor in tensors:
            # Write metadata
            shape_str = " x ".join(map(str, tensor.shape))
            f.write(f"// Tensor: {tensor.name}\n")
            f.write(f"// Shape: {shape_str}\n")
            f.write(f"// Scale: {tensor.scale:.8f}\n")
            f.write(f"// Zero Point: {tensor.zero_point}\n")
            
            # Determine C type
            if tensor.dtype == 'int8':
                c_type = 'int8_t'
            elif tensor.dtype == 'int4':
                c_type = 'uint8_t'
            else:
                c_type = 'float'
            
            # Write data
            name = tensor.name.replace('.', '_').replace('/', '_')
            size = tensor.data.size
            f.write(f"static const {c_type} {name}[{size}] = {{\n    ")
            
            flat_data = tensor.data.flatten()
            for i, val in enumerate(flat_data):
                if tensor.dtype in ['int8', 'int4']:
                    f.write(f"{int(val)}, ")
                else:
                    f.write(f"{val:.6f}f, ")
                if (i + 1) % 16 == 0:
                    f.write("\n    ")
            
            f.write("\n};\n\n")
            
            # Write scale and zero point
            f.write(f"static const float {name}_scale = {tensor.scale:.8f}f;\n")
            f.write(f"static const int32_t {name}_zero_point = {tensor.zero_point};\n\n")
        
        f.write(f"#endif // {model_name.upper()}_WEIGHTS_H\n")

def export_to_binary(tensors: List[TensorInfo], output_path: str):
    """Export quantized tensors to binary file."""
    
    with open(output_path, 'wb') as f:
        # Magic number
        f.write(b'EIF1')
        
        # Number of tensors
        f.write(struct.pack('<I', len(tensors)))
        
        for tensor in tensors:
            # Name length and name
            name_bytes = tensor.name.encode('utf-8')
            f.write(struct.pack('<I', len(name_bytes)))
            f.write(name_bytes)
            
            # Shape
            f.write(struct.pack('<I', len(tensor.shape)))
            for dim in tensor.shape:
                f.write(struct.pack('<I', dim))
            
            # Dtype (0=float32, 1=int8, 2=int4)
            dtype_map = {'float32': 0, 'int8': 1, 'int4': 2}
            f.write(struct.pack('<B', dtype_map.get(tensor.dtype, 0)))
            
            # Scale and zero point
            f.write(struct.pack('<f', tensor.scale))
            f.write(struct.pack('<i', tensor.zero_point))
            
            # Data
            data_bytes = tensor.data.tobytes()
            f.write(struct.pack('<I', len(data_bytes)))
            f.write(data_bytes)

# =============================================================================
# Analysis Functions
# =============================================================================

def analyze_model(tensors: List[TensorInfo]) -> CompressionStats:
    """Analyze model size and compression stats."""
    
    original_size = 0
    compressed_size = 0
    num_params = 0
    num_pruned = 0
    
    for tensor in tensors:
        size = np.prod(tensor.shape)
        num_params += size
        original_size += size * 4  # Assuming float32 original
        
        if tensor.dtype == 'int8':
            compressed_size += size
        elif tensor.dtype == 'int4':
            compressed_size += size // 2
        else:
            compressed_size += size * 4
        
        # Count zeros (pruned)
        num_pruned += np.sum(tensor.data == 0)
    
    return CompressionStats(
        original_size=original_size,
        compressed_size=compressed_size,
        compression_ratio=original_size / max(compressed_size, 1),
        num_params=num_params,
        num_pruned=int(num_pruned),
        quantization=tensors[0].dtype if tensors else 'none'
    )

def print_stats(stats: CompressionStats):
    """Print compression statistics."""
    print("\n" + "=" * 50)
    print("Model Compression Statistics")
    print("=" * 50)
    print(f"Original Size:     {stats.original_size / 1024:.2f} KB")
    print(f"Compressed Size:   {stats.compressed_size / 1024:.2f} KB")
    print(f"Compression Ratio: {stats.compression_ratio:.2f}x")
    print(f"Total Parameters:  {stats.num_params:,}")
    print(f"Pruned Parameters: {stats.num_pruned:,} ({100*stats.num_pruned/max(stats.num_params,1):.1f}%)")
    print(f"Quantization:      {stats.quantization}")
    print("=" * 50 + "\n")

# =============================================================================
# Demo / Test Functions
# =============================================================================

def demo_compression():
    """Demonstrate compression on synthetic weights."""
    
    print("EIF Model Compression Toolkit - Demo")
    print("=" * 50)
    
    # Create synthetic model weights
    np.random.seed(42)
    
    # Simulate a small neural network
    weights = [
        TensorInfo("layer1.weights", (128, 64), "float32", 
                   np.random.randn(128, 64).astype(np.float32) * 0.1),
        TensorInfo("layer1.bias", (128,), "float32",
                   np.random.randn(128).astype(np.float32) * 0.01),
        TensorInfo("layer2.weights", (64, 128), "float32",
                   np.random.randn(64, 128).astype(np.float32) * 0.1),
        TensorInfo("layer2.bias", (64,), "float32",
                   np.random.randn(64).astype(np.float32) * 0.01),
    ]
    
    print("\n--- Original Model ---")
    stats = analyze_model(weights)
    print_stats(stats)
    
    # INT8 Quantization
    print("--- Applying INT8 Quantization ---")
    quantized_weights = []
    for tensor in weights:
        q_data, scale, zp = quantize_tensor_int8(tensor.data)
        quantized_weights.append(TensorInfo(
            tensor.name, tensor.shape, "int8", q_data, scale, zp
        ))
    
    stats = analyze_model(quantized_weights)
    print_stats(stats)
    
    # INT4 Quantization
    print("--- Applying INT4 Quantization ---")
    int4_weights = []
    for tensor in weights:
        q_data, scale, zp = quantize_tensor_int4(tensor.data)
        int4_weights.append(TensorInfo(
            tensor.name, tensor.shape, "int4", q_data, scale, zp
        ))
    
    # Note: INT4 packed size
    print(f"INT4 packed - 2 values per byte")
    
    # Pruning
    print("\n--- Applying 50% Magnitude Pruning ---")
    pruned_weights = []
    for tensor in weights:
        pruned, mask = prune_magnitude(tensor.data, 0.5)
        pruned_weights.append(TensorInfo(
            tensor.name, tensor.shape, "float32", pruned
        ))
    
    stats = analyze_model(pruned_weights)
    print_stats(stats)
    
    # Combined: Prune + Quantize
    print("--- Combined: 30% Pruning + INT8 Quantization ---")
    combined_weights = []
    for tensor in weights:
        pruned, _ = prune_magnitude(tensor.data, 0.3)
        q_data, scale, zp = quantize_tensor_int8(pruned)
        combined_weights.append(TensorInfo(
            tensor.name, tensor.shape, "int8", q_data, scale, zp
        ))
    
    stats = analyze_model(combined_weights)
    print_stats(stats)
    
    # Export demo
    print("--- Exporting to C Header ---")
    export_to_c_header(quantized_weights, "demo_model_int8.h", "demo")
    print("Exported: demo_model_int8.h")
    
    print("\nDemo complete!")

# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="EIF Model Compression Toolkit")
    parser.add_argument("--demo", action="store_true", help="Run demo on synthetic data")
    parser.add_argument("--input", type=str, help="Input model file (ONNX or numpy)")
    parser.add_argument("--output", type=str, help="Output file path")
    parser.add_argument("--quantize", type=str, choices=["int8", "int4"], help="Quantization type")
    parser.add_argument("--prune", type=float, help="Pruning sparsity (0.0-1.0)")
    parser.add_argument("--format", type=str, choices=["header", "binary"], default="header",
                        help="Output format")
    parser.add_argument("--info", action="store_true", help="Show model info only")
    
    args = parser.parse_args()
    
    if args.demo:
        demo_compression()
        return
    
    if not args.input:
        print("Use --demo for demonstration or --input to process a model")
        parser.print_help()
        return
    
    # Load ONNX model
    input_path = Path(args.input)
    
    if input_path.suffix.lower() == '.onnx':
        if not ONNX_AVAILABLE:
            print("ERROR: onnx package not installed. Run: pip install onnx")
            return
        
        print(f"Loading ONNX model: {input_path}")
        tensors, model_info = load_onnx_model(str(input_path))
        
        if args.info:
            print("\n" + "=" * 50)
            print("Model Information")
            print("=" * 50)
            print(f"  Inputs:  {model_info['inputs']}")
            print(f"  Outputs: {model_info['outputs']}")
            print(f"  Layers:  {len(model_info['layers'])}")
            print(f"  Tensors: {len(tensors)}")
            
            print("\nLayer Details:")
            for layer in model_info['layers']:
                print(f"  {layer['name']}: {layer['type']}")
            
            print("\nWeight Tensors:")
            total_params = 0
            for t in tensors:
                num_params = np.prod(t.shape)
                total_params += num_params
                print(f"  {t.name}: {t.shape} ({num_params:,} params)")
            
            print(f"\nTotal Parameters: {total_params:,}")
            print(f"Total Size: {total_params * 4 / 1024:.2f} KB (float32)")
            return
        
    elif input_path.suffix.lower() == '.npy':
        # Load numpy weights
        data = np.load(str(input_path), allow_pickle=True)
        if isinstance(data, np.ndarray) and data.dtype == object:
            # Dictionary-like
            tensors = []
            for i, (name, arr) in enumerate(data.item().items()):
                tensors.append(TensorInfo(name, arr.shape, "float32", arr))
        else:
            tensors = [TensorInfo("weights", data.shape, "float32", data)]
    else:
        print(f"Unsupported format: {input_path.suffix}")
        print("Supported: .onnx, .npy")
        return
    
    print(f"Loaded {len(tensors)} tensors")
    
    # Apply pruning
    if args.prune:
        print(f"\nApplying {args.prune*100:.0f}% magnitude pruning...")
        pruned_tensors = []
        for t in tensors:
            if len(t.shape) >= 2:  # Only prune weight matrices
                pruned, mask = prune_magnitude(t.data, args.prune)
                pruned_tensors.append(TensorInfo(t.name, t.shape, t.dtype, pruned))
            else:
                pruned_tensors.append(t)
        tensors = pruned_tensors
    
    # Apply quantization
    if args.quantize:
        print(f"\nApplying {args.quantize.upper()} quantization...")
        quantized_tensors = []
        
        for t in tensors:
            if args.quantize == "int8":
                q_data, scale, zp = quantize_tensor_int8(t.data)
                quantized_tensors.append(TensorInfo(
                    t.name, t.shape, "int8", q_data, scale, zp
                ))
            elif args.quantize == "int4":
                q_data, scale, zp = quantize_tensor_int4(t.data)
                quantized_tensors.append(TensorInfo(
                    t.name, t.shape, "int4", q_data, scale, zp
                ))
        tensors = quantized_tensors
    
    # Analyze
    stats = analyze_model(tensors)
    print_stats(stats)
    
    # Export
    if args.output:
        output_path = Path(args.output)
        model_name = output_path.stem
        
        if args.format == "header":
            export_to_c_header(tensors, str(output_path), model_name)
            print(f"Exported C header: {output_path}")
        else:
            export_to_binary(tensors, str(output_path))
            print(f"Exported binary: {output_path}")
    else:
        print("No output specified. Use --output to save.")


# =============================================================================
# ONNX Loading
# =============================================================================

def load_onnx_model(path: str) -> Tuple[List[TensorInfo], Dict]:
    """
    Load an ONNX model and extract weights.
    
    Args:
        path: Path to .onnx file
    
    Returns:
        Tuple of (tensors, model_info)
    """
    if not ONNX_AVAILABLE:
        raise ImportError("onnx package not installed")
    
    model = onnx.load(path)
    onnx.checker.check_model(model)
    
    # Extract model info
    model_info = {
        'inputs': [(inp.name, [d.dim_value for d in inp.type.tensor_type.shape.dim]) 
                   for inp in model.graph.input],
        'outputs': [(out.name, [d.dim_value for d in out.type.tensor_type.shape.dim]) 
                    for out in model.graph.output],
        'layers': []
    }
    
    # Extract layer info
    for node in model.graph.node:
        model_info['layers'].append({
            'name': node.name or node.output[0],
            'type': node.op_type,
            'inputs': list(node.input),
            'outputs': list(node.output)
        })
    
    # Extract weight tensors (initializers)
    tensors = []
    for init in model.graph.initializer:
        arr = numpy_helper.to_array(init)
        tensors.append(TensorInfo(
            name=init.name,
            shape=tuple(arr.shape),
            dtype="float32",
            data=arr.astype(np.float32)
        ))
    
    return tensors, model_info


def create_sample_onnx():
    """Create a sample ONNX model for testing."""
    if not ONNX_AVAILABLE:
        print("onnx package not installed")
        return
    
    from onnx import helper, TensorProto
    
    # Simple model: Input -> Dense(64) -> ReLU -> Dense(10)
    input_dim = 128
    hidden_dim = 64
    output_dim = 10
    
    # Create weight initializers
    w1 = np.random.randn(input_dim, hidden_dim).astype(np.float32) * 0.1
    b1 = np.zeros(hidden_dim, dtype=np.float32)
    w2 = np.random.randn(hidden_dim, output_dim).astype(np.float32) * 0.1
    b2 = np.zeros(output_dim, dtype=np.float32)
    
    w1_init = numpy_helper.from_array(w1, "dense1_weight")
    b1_init = numpy_helper.from_array(b1, "dense1_bias")
    w2_init = numpy_helper.from_array(w2, "dense2_weight")
    b2_init = numpy_helper.from_array(b2, "dense2_bias")
    
    # Create nodes
    dense1 = helper.make_node('MatMul', ['input', 'dense1_weight'], ['dense1_out'], name='dense1')
    add1 = helper.make_node('Add', ['dense1_out', 'dense1_bias'], ['add1_out'], name='add1')
    relu = helper.make_node('Relu', ['add1_out'], ['relu_out'], name='relu')
    dense2 = helper.make_node('MatMul', ['relu_out', 'dense2_weight'], ['dense2_out'], name='dense2')
    add2 = helper.make_node('Add', ['dense2_out', 'dense2_bias'], ['output'], name='add2')
    
    # Create graph
    graph = helper.make_graph(
        [dense1, add1, relu, dense2, add2],
        "sample_model",
        [helper.make_tensor_value_info('input', TensorProto.FLOAT, [1, input_dim])],
        [helper.make_tensor_value_info('output', TensorProto.FLOAT, [1, output_dim])],
        [w1_init, b1_init, w2_init, b2_init]
    )
    
    model = helper.make_model(graph)
    onnx.save(model, "sample_model.onnx")
    print("Created sample_model.onnx")


if __name__ == "__main__":
    main()

