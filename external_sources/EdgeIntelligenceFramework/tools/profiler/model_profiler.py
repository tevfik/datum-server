#!/usr/bin/env python3
"""
EIF Model Profiler - Neural Network Analysis Tool

Analyzes neural network models for:
- FLOPS (Floating Point Operations)
- Memory footprint (weights, activations)
- Layer-by-layer breakdown
- MAC (Multiply-Accumulate) operations
- Inference time estimation

Usage:
    python model_profiler.py model.json [--verbose]
    python model_profiler.py --demo    # Run with synthetic model
"""

import argparse
import json
import sys
from dataclasses import dataclass
from typing import List, Dict, Optional, Tuple
import math

# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class LayerProfile:
    """Profile data for a single layer"""
    name: str
    layer_type: str
    input_shape: Tuple[int, ...]
    output_shape: Tuple[int, ...]
    params: int
    weight_bytes: int
    activation_bytes: int
    flops: int
    macs: int
    percentage: float = 0.0


@dataclass
class ModelProfile:
    """Complete model profile"""
    name: str
    total_params: int
    total_weight_bytes: int
    total_activation_bytes: int
    total_memory_bytes: int
    total_flops: int
    total_macs: int
    layers: List[LayerProfile]
    input_shape: Tuple[int, ...]
    output_shape: Tuple[int, ...]

# =============================================================================
# FLOPS Calculation per Layer Type
# =============================================================================

def calc_conv2d_flops(in_c: int, out_c: int, kh: int, kw: int, 
                      out_h: int, out_w: int, groups: int = 1) -> Tuple[int, int]:
    """Calculate FLOPS for Conv2D layer"""
    # MACs = output_elements * kernel_size * input_channels / groups
    macs = out_h * out_w * out_c * kh * kw * (in_c // groups)
    # FLOPS = 2 * MACs (multiply + add) + bias
    flops = 2 * macs + out_h * out_w * out_c  # bias addition
    return flops, macs


def calc_conv1d_flops(in_c: int, out_c: int, k: int, out_len: int, 
                      groups: int = 1) -> Tuple[int, int]:
    """Calculate FLOPS for Conv1D layer"""
    macs = out_len * out_c * k * (in_c // groups)
    flops = 2 * macs + out_len * out_c
    return flops, macs


def calc_dense_flops(in_features: int, out_features: int) -> Tuple[int, int]:
    """Calculate FLOPS for Dense/Linear layer"""
    macs = in_features * out_features
    flops = 2 * macs + out_features  # bias
    return flops, macs


def calc_batch_norm_flops(num_elements: int) -> Tuple[int, int]:
    """Calculate FLOPS for BatchNorm layer"""
    # (x - mean) / sqrt(var) * gamma + beta = 4 ops per element
    flops = 4 * num_elements
    return flops, 0


def calc_activation_flops(num_elements: int, act_type: str) -> Tuple[int, int]:
    """Calculate FLOPS for activation functions"""
    if act_type.lower() in ['relu', 'relu6']:
        flops = num_elements  # comparison
    elif act_type.lower() == 'sigmoid':
        flops = 4 * num_elements  # exp, add, div
    elif act_type.lower() == 'tanh':
        flops = 5 * num_elements
    elif act_type.lower() in ['softmax']:
        flops = 5 * num_elements  # exp + sum + div
    elif act_type.lower() == 'gelu':
        flops = 8 * num_elements
    else:
        flops = num_elements
    return flops, 0


def calc_pooling_flops(output_elements: int, pool_size: int) -> Tuple[int, int]:
    """Calculate FLOPS for pooling layer"""
    # MaxPool: pool_size - 1 comparisons per output
    # AvgPool: pool_size - 1 additions + 1 division per output
    flops = output_elements * pool_size
    return flops, 0


def calc_attention_flops(seq_len: int, d_model: int, n_heads: int) -> Tuple[int, int]:
    """Calculate FLOPS for self-attention"""
    d_k = d_model // n_heads
    
    # Q, K, V projections: 3 * seq_len * d_model * d_model
    qkv_macs = 3 * seq_len * d_model * d_model
    
    # Attention scores: seq_len * seq_len * d_k * n_heads
    attn_macs = seq_len * seq_len * d_k * n_heads
    
    # Softmax: 5 * seq_len * seq_len * n_heads
    softmax_flops = 5 * seq_len * seq_len * n_heads
    
    # Weighted sum: seq_len * seq_len * d_k * n_heads
    weighted_macs = seq_len * seq_len * d_k * n_heads
    
    # Output projection: seq_len * d_model * d_model
    out_macs = seq_len * d_model * d_model
    
    total_macs = qkv_macs + attn_macs + weighted_macs + out_macs
    total_flops = 2 * total_macs + softmax_flops
    
    return total_flops, total_macs

# =============================================================================
# Memory Calculation
# =============================================================================

def calc_tensor_bytes(shape: Tuple[int, ...], dtype: str = 'float32') -> int:
    """Calculate memory for a tensor"""
    dtype_sizes = {
        'float32': 4, 'float16': 2, 'int8': 1, 'int16': 2, 'int32': 4
    }
    elements = 1
    for dim in shape:
        elements *= dim
    return elements * dtype_sizes.get(dtype, 4)


def calc_layer_params(layer: dict) -> int:
    """Calculate number of parameters in a layer"""
    ltype = layer.get('type', '').lower()
    
    if ltype in ['conv2d', 'conv']:
        in_c = layer.get('in_channels', layer.get('input_channels', 1))
        out_c = layer.get('out_channels', layer.get('output_channels', 1))
        kh = layer.get('kernel_h', layer.get('kernel_size', 3))
        kw = layer.get('kernel_w', kh)
        groups = layer.get('groups', 1)
        bias = layer.get('bias', True)
        params = out_c * (in_c // groups) * kh * kw
        if bias:
            params += out_c
        return params
        
    elif ltype in ['conv1d']:
        in_c = layer.get('in_channels', 1)
        out_c = layer.get('out_channels', 1)
        k = layer.get('kernel_size', 3)
        groups = layer.get('groups', 1)
        bias = layer.get('bias', True)
        params = out_c * (in_c // groups) * k
        if bias:
            params += out_c
        return params
        
    elif ltype in ['dense', 'linear', 'fc']:
        in_f = layer.get('in_features', layer.get('input_size', 1))
        out_f = layer.get('out_features', layer.get('output_size', 1))
        bias = layer.get('bias', True)
        params = in_f * out_f
        if bias:
            params += out_f
        return params
        
    elif ltype in ['batchnorm', 'batch_norm', 'bn']:
        features = layer.get('num_features', layer.get('features', 1))
        return features * 4  # gamma, beta, running_mean, running_var
        
    elif ltype in ['embedding']:
        vocab = layer.get('num_embeddings', layer.get('vocab_size', 1))
        dim = layer.get('embedding_dim', layer.get('dim', 1))
        return vocab * dim
        
    elif ltype in ['attention', 'multi_head_attention']:
        d_model = layer.get('d_model', layer.get('embed_dim', 64))
        # Q, K, V projections + output projection
        return 4 * d_model * d_model
        
    else:
        return 0

# =============================================================================
# Profile Generation
# =============================================================================

def profile_layer(layer: dict, input_shape: Tuple[int, ...]) -> LayerProfile:
    """Profile a single layer"""
    ltype = layer.get('type', 'unknown').lower()
    name = layer.get('name', ltype)
    params = calc_layer_params(layer)
    
    # Calculate output shape
    output_shape = input_shape  # default
    flops, macs = 0, 0
    
    if ltype in ['conv2d', 'conv']:
        batch, in_c, in_h, in_w = input_shape if len(input_shape) == 4 else (1,) + input_shape
        out_c = layer.get('out_channels', layer.get('output_channels', in_c))
        kh = layer.get('kernel_h', layer.get('kernel_size', 3))
        kw = layer.get('kernel_w', kh)
        stride = layer.get('stride', 1)
        padding = layer.get('padding', 0)
        
        out_h = (in_h + 2 * padding - kh) // stride + 1
        out_w = (in_w + 2 * padding - kw) // stride + 1
        output_shape = (batch, out_c, out_h, out_w)
        
        flops, macs = calc_conv2d_flops(in_c, out_c, kh, kw, out_h, out_w,
                                         layer.get('groups', 1))
    
    elif ltype in ['conv1d']:
        batch, in_c, in_len = input_shape if len(input_shape) == 3 else (1,) + input_shape
        out_c = layer.get('out_channels', in_c)
        k = layer.get('kernel_size', 3)
        stride = layer.get('stride', 1)
        padding = layer.get('padding', 0)
        
        out_len = (in_len + 2 * padding - k) // stride + 1
        output_shape = (batch, out_c, out_len)
        
        flops, macs = calc_conv1d_flops(in_c, out_c, k, out_len,
                                         layer.get('groups', 1))
    
    elif ltype in ['dense', 'linear', 'fc']:
        in_f = layer.get('in_features', input_shape[-1])
        out_f = layer.get('out_features', layer.get('output_size', in_f))
        output_shape = input_shape[:-1] + (out_f,)
        flops, macs = calc_dense_flops(in_f, out_f)
    
    elif ltype in ['maxpool2d', 'avgpool2d', 'pool']:
        batch, c, h, w = input_shape if len(input_shape) == 4 else (1,) + input_shape
        pool_size = layer.get('kernel_size', layer.get('pool_size', 2))
        stride = layer.get('stride', pool_size)
        out_h = h // stride
        out_w = w // stride
        output_shape = (batch, c, out_h, out_w)
        flops, macs = calc_pooling_flops(batch * c * out_h * out_w, pool_size * pool_size)
    
    elif ltype in ['flatten']:
        output_shape = (input_shape[0], int(math.prod(input_shape[1:])))
    
    elif ltype in ['batchnorm', 'batch_norm', 'bn']:
        flops, macs = calc_batch_norm_flops(int(math.prod(input_shape)))
    
    elif ltype in ['relu', 'relu6', 'sigmoid', 'tanh', 'softmax', 'gelu']:
        flops, macs = calc_activation_flops(int(math.prod(input_shape)), ltype)
    
    elif ltype in ['attention', 'multi_head_attention']:
        seq_len = input_shape[1] if len(input_shape) > 1 else 1
        d_model = layer.get('d_model', input_shape[-1])
        n_heads = layer.get('num_heads', layer.get('n_heads', 1))
        flops, macs = calc_attention_flops(seq_len, d_model, n_heads)
    
    elif ltype in ['embedding']:
        output_shape = input_shape + (layer.get('embedding_dim', 64),)
    
    weight_bytes = params * 4  # float32
    activation_bytes = calc_tensor_bytes(output_shape)
    
    return LayerProfile(
        name=name,
        layer_type=ltype,
        input_shape=input_shape,
        output_shape=output_shape,
        params=params,
        weight_bytes=weight_bytes,
        activation_bytes=activation_bytes,
        flops=flops,
        macs=macs
    )


def profile_model(model: dict) -> ModelProfile:
    """Profile an entire model"""
    layers = model.get('layers', [])
    input_shape = tuple(model.get('input_shape', [1, 3, 224, 224]))
    model_name = model.get('name', 'unknown')
    
    layer_profiles = []
    current_shape = input_shape
    
    for layer in layers:
        profile = profile_layer(layer, current_shape)
        layer_profiles.append(profile)
        current_shape = profile.output_shape
    
    # Calculate totals
    total_params = sum(lp.params for lp in layer_profiles)
    total_weight_bytes = sum(lp.weight_bytes for lp in layer_profiles)
    total_activation_bytes = max(lp.activation_bytes for lp in layer_profiles) if layer_profiles else 0
    total_flops = sum(lp.flops for lp in layer_profiles)
    total_macs = sum(lp.macs for lp in layer_profiles)
    
    # Calculate percentages
    if total_flops > 0:
        for lp in layer_profiles:
            lp.percentage = (lp.flops / total_flops) * 100
    
    return ModelProfile(
        name=model_name,
        total_params=total_params,
        total_weight_bytes=total_weight_bytes,
        total_activation_bytes=total_activation_bytes,
        total_memory_bytes=total_weight_bytes + total_activation_bytes,
        total_flops=total_flops,
        total_macs=total_macs,
        layers=layer_profiles,
        input_shape=input_shape,
        output_shape=current_shape
    )

# =============================================================================
# Reporting
# =============================================================================

def format_bytes(b: int) -> str:
    """Format bytes to human readable"""
    if b < 1024:
        return f"{b} B"
    elif b < 1024 * 1024:
        return f"{b / 1024:.2f} KB"
    else:
        return f"{b / (1024 * 1024):.2f} MB"


def format_ops(ops: int) -> str:
    """Format operations to human readable"""
    if ops < 1000:
        return f"{ops}"
    elif ops < 1_000_000:
        return f"{ops / 1000:.2f}K"
    elif ops < 1_000_000_000:
        return f"{ops / 1_000_000:.2f}M"
    else:
        return f"{ops / 1_000_000_000:.2f}G"


def print_profile(profile: ModelProfile, verbose: bool = False):
    """Print model profile report"""
    print("\n" + "=" * 70)
    print(f"  MODEL PROFILE: {profile.name}")
    print("=" * 70)
    
    print(f"\n  Input Shape:  {profile.input_shape}")
    print(f"  Output Shape: {profile.output_shape}")
    print(f"  Layers:       {len(profile.layers)}")
    
    print("\n" + "-" * 70)
    print("  SUMMARY")
    print("-" * 70)
    print(f"  Total Parameters:    {profile.total_params:,} ({format_bytes(profile.total_weight_bytes)})")
    print(f"  Peak Activations:    {format_bytes(profile.total_activation_bytes)}")
    print(f"  Total Memory:        {format_bytes(profile.total_memory_bytes)}")
    print(f"  Total FLOPS:         {format_ops(profile.total_flops)}")
    print(f"  Total MACs:          {format_ops(profile.total_macs)}")
    
    # Estimate inference time (assuming 1 GFLOPS throughput)
    gflops = profile.total_flops / 1e9
    est_time_1gflops = gflops * 1000  # ms at 1 GFLOPS
    est_time_arm = gflops * 1000 / 0.1  # ARM Cortex-M7 ~100 MFLOPS
    print(f"\n  Est. Inference Time:")
    print(f"    - @ 1 GFLOPS:      {est_time_1gflops:.2f} ms")
    print(f"    - @ Cortex-M7:     {est_time_arm:.1f} ms")
    
    if verbose:
        print("\n" + "-" * 70)
        print("  LAYER-BY-LAYER BREAKDOWN")
        print("-" * 70)
        print(f"  {'Layer':<20} {'Type':<12} {'Output':<18} {'Params':>10} {'FLOPS':>10} {'%':>6}")
        print("  " + "-" * 66)
        
        for lp in profile.layers:
            shape_str = str(lp.output_shape)[:16]
            print(f"  {lp.name[:20]:<20} {lp.layer_type[:12]:<12} {shape_str:<18} "
                  f"{lp.params:>10,} {format_ops(lp.flops):>10} {lp.percentage:>5.1f}%")
    
    # FLOPS distribution
    print("\n" + "-" * 70)
    print("  FLOPS DISTRIBUTION")
    print("-" * 70)
    
    type_flops = {}
    for lp in profile.layers:
        type_flops[lp.layer_type] = type_flops.get(lp.layer_type, 0) + lp.flops
    
    sorted_types = sorted(type_flops.items(), key=lambda x: -x[1])
    for ltype, flops in sorted_types[:10]:
        pct = (flops / profile.total_flops * 100) if profile.total_flops > 0 else 0
        bar_len = int(pct / 2)
        bar = "█" * bar_len + "░" * (50 - bar_len)
        print(f"  {ltype:<15} {bar} {pct:5.1f}%")
    
    print("\n" + "=" * 70)


def create_demo_model() -> dict:
    """Create a synthetic demo model (MobileNet-like)"""
    return {
        "name": "DemoMobileNet",
        "input_shape": [1, 3, 224, 224],
        "layers": [
            {"name": "conv1", "type": "conv2d", "in_channels": 3, "out_channels": 32, "kernel_size": 3, "stride": 2, "padding": 1},
            {"name": "bn1", "type": "batchnorm", "num_features": 32},
            {"name": "relu1", "type": "relu"},
            {"name": "dw_conv2", "type": "conv2d", "in_channels": 32, "out_channels": 32, "kernel_size": 3, "stride": 1, "padding": 1, "groups": 32},
            {"name": "bn2", "type": "batchnorm", "num_features": 32},
            {"name": "relu2", "type": "relu"},
            {"name": "pw_conv2", "type": "conv2d", "in_channels": 32, "out_channels": 64, "kernel_size": 1},
            {"name": "bn3", "type": "batchnorm", "num_features": 64},
            {"name": "relu3", "type": "relu"},
            {"name": "dw_conv3", "type": "conv2d", "in_channels": 64, "out_channels": 64, "kernel_size": 3, "stride": 2, "padding": 1, "groups": 64},
            {"name": "bn4", "type": "batchnorm", "num_features": 64},
            {"name": "relu4", "type": "relu"},
            {"name": "pw_conv3", "type": "conv2d", "in_channels": 64, "out_channels": 128, "kernel_size": 1},
            {"name": "bn5", "type": "batchnorm", "num_features": 128},
            {"name": "relu5", "type": "relu"},
            {"name": "avgpool", "type": "avgpool2d", "kernel_size": 7},
            {"name": "flatten", "type": "flatten"},
            {"name": "fc", "type": "dense", "in_features": 128 * 7 * 7, "out_features": 1000},
            {"name": "softmax", "type": "softmax"}
        ]
    }

# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='EIF Model Profiler')
    parser.add_argument('model', nargs='?', help='Path to model JSON file')
    parser.add_argument('--verbose', '-v', action='store_true', help='Show layer-by-layer breakdown')
    parser.add_argument('--demo', action='store_true', help='Run with demo model')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    args = parser.parse_args()
    
    if args.demo:
        model = create_demo_model()
        print("Running demo with synthetic MobileNet-like model...")
    elif args.model:
        try:
            with open(args.model) as f:
                model = json.load(f)
        except Exception as e:
            print(f"Error loading model: {e}")
            sys.exit(1)
    else:
        parser.print_help()
        sys.exit(1)
    
    profile = profile_model(model)
    
    if args.json:
        import json
        result = {
            'name': profile.name,
            'total_params': profile.total_params,
            'total_weight_bytes': profile.total_weight_bytes,
            'total_activation_bytes': profile.total_activation_bytes,
            'total_memory_bytes': profile.total_memory_bytes,
            'total_flops': profile.total_flops,
            'total_macs': profile.total_macs,
            'input_shape': list(profile.input_shape),
            'output_shape': list(profile.output_shape),
            'layers': [
                {
                    'name': lp.name,
                    'type': lp.layer_type,
                    'params': lp.params,
                    'flops': lp.flops,
                    'percentage': lp.percentage
                }
                for lp in profile.layers
            ]
        }
        print(json.dumps(result, indent=2))
    else:
        print_profile(profile, verbose=args.verbose)


if __name__ == '__main__':
    main()
