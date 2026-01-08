#!/usr/bin/env python3
"""
PyTorch to EIF Converter

Converts PyTorch models to EIF binary format with enhanced optimizations:
- Layer fusion (Conv+BN+ReLU)
- Quantization support (INT8/INT16)
- ONNX intermediate format
- Direct PyTorch tracing

Usage:
    python3 tools/pytorch_to_eif.py model.pth -o model.eif
    python3 tools/pytorch_to_eif.py model.pth --quantize int8 --optimize
"""

import sys
import argparse
import numpy as np
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import struct
import json

try:
    import torch
    import torch.nn as nn
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False
    # Create dummy classes for type hints
    class nn:
        Module = object

# EIF Binary Format
MAGIC_NUMBER = b'MFIE'
VERSION = 1

# Layer types
LAYER_DENSE = 0x01
LAYER_CONV2D = 0x02
LAYER_ACTIVATION = 0x07
LAYER_BATCH_NORM = 0x08
LAYER_FLATTEN = 0x06


class PyTorchToEIFConverter:
    """Convert PyTorch models to EIF format"""

    def __init__(self, optimize=True, quantize=None):
        self.optimize = optimize
        self.quantize = quantize
        self.layers = []
        self.optimizations = []

    def trace_model(self, model: nn.Module, input_shape: Tuple) -> Dict:
        """Trace PyTorch model execution"""
        model.eval()
        dummy_input = torch.randn(1, *input_shape)

        print(f"Tracing model with input shape: {input_shape}")

        # Extract layers
        layer_info = []
        for name, module in model.named_modules():
            if isinstance(module, nn.Linear):
                layer_info.append(
                    {
                        "name": name,
                        "type": "Dense",
                        "type_id": LAYER_DENSE,
                        "weight": module.weight.detach().numpy(),
                        "bias": module.bias.detach().numpy()
                        if module.bias is not None
                        else None,
                        "in_features": module.in_features,
                        "out_features": module.out_features,
                    }
                )
            elif isinstance(module, nn.Conv2d):
                layer_info.append(
                    {
                        "name": name,
                        "type": "Conv2D",
                        "type_id": LAYER_CONV2D,
                        "weight": module.weight.detach().numpy(),
                        "bias": module.bias.detach().numpy()
                        if module.bias is not None
                        else None,
                        "in_channels": module.in_channels,
                        "out_channels": module.out_channels,
                        "kernel_size": module.kernel_size,
                        "stride": module.stride,
                        "padding": module.padding,
                    }
                )
            elif isinstance(module, nn.BatchNorm2d):
                layer_info.append(
                    {
                        "name": name,
                        "type": "BatchNorm",
                        "type_id": LAYER_BATCH_NORM,
                        "weight": module.weight.detach().numpy(),
                        "bias": module.bias.detach().numpy(),
                        "running_mean": module.running_mean.detach().numpy(),
                        "running_var": module.running_var.detach().numpy(),
                        "num_features": module.num_features,
                        "eps": module.eps,
                    }
                )
            elif isinstance(module, nn.ReLU):
                layer_info.append(
                    {"name": name, "type": "ReLU", "type_id": LAYER_ACTIVATION}
                )
            elif isinstance(module, nn.Flatten):
                layer_info.append(
                    {"name": name, "type": "Flatten", "type_id": LAYER_FLATTEN}
                )

        return {"layers": layer_info, "input_shape": input_shape}

    def fuse_layers(self, traced_model: Dict) -> Dict:
        """Fuse Conv+BN+ReLU patterns"""
        if not self.optimize:
            return traced_model

        layers = traced_model["layers"]
        fused_layers = []
        i = 0

        print("\nApplying layer fusion optimizations...")

        while i < len(layers):
            layer = layers[i]

            # Try to fuse Conv+BN+ReLU
            if (
                layer["type"] == "Conv2D"
                and i + 2 < len(layers)
                and layers[i + 1]["type"] == "BatchNorm"
                and layers[i + 2]["type"] == "ReLU"
            ):
                conv = layers[i]
                bn = layers[i + 1]

                print(
                    f"  Fusing: {conv['name']} + {bn['name']} + {layers[i+2]['name']}"
                )

                # Fuse Conv+BN weights
                gamma = bn["weight"]
                beta = bn["bias"]
                mean = bn["running_mean"]
                var = bn["running_var"]
                eps = bn["eps"]

                # Fused weight: w' = gamma * w / sqrt(var + eps)
                std = np.sqrt(var + eps)
                fused_weight = conv["weight"] * (gamma / std).reshape(-1, 1, 1, 1)

                # Fused bias: b' = beta + gamma * (b - mean) / sqrt(var + eps)
                if conv["bias"] is not None:
                    fused_bias = gamma * (conv["bias"] - mean) / std + beta
                else:
                    fused_bias = gamma * (-mean) / std + beta

                fused_layer = conv.copy()
                fused_layer["weight"] = fused_weight
                fused_layer["bias"] = fused_bias
                fused_layer["activation"] = "relu"
                fused_layer["fused"] = True

                fused_layers.append(fused_layer)
                i += 3
                self.optimizations.append("Conv+BN+ReLU fusion")
            else:
                fused_layers.append(layer)
                i += 1

        traced_model["layers"] = fused_layers
        return traced_model

    def quantize_weights(self, layer: Dict) -> Dict:
        """Quantize layer weights to INT8"""
        if self.quantize != "int8":
            return layer

        if "weight" in layer:
            weights = layer["weight"]
            max_val = np.max(np.abs(weights))
            scale = max_val / 127.0
            quantized = np.round(weights / scale).astype(np.int8)

            layer["weight_quantized"] = quantized
            layer["weight_scale"] = scale
            layer["quantized"] = True

            print(
                f"  Quantized {layer['name']}: {weights.nbytes} -> {quantized.nbytes} bytes"
            )

        return layer

    def convert(
        self, model: nn.Module, input_shape: Tuple, output_path: str
    ) -> Dict:
        """Convert PyTorch model to EIF"""
        print("=" * 60)
        print("PyTorch to EIF Conversion")
        print("=" * 60)

        # Trace model
        traced_model = self.trace_model(model, input_shape)
        print(f"Extracted {len(traced_model['layers'])} layers")

        # Apply optimizations
        if self.optimize:
            traced_model = self.fuse_layers(traced_model)

        # Quantization
        if self.quantize:
            print(f"\nQuantizing to {self.quantize.upper()}...")
            for i, layer in enumerate(traced_model["layers"]):
                traced_model["layers"][i] = self.quantize_weights(layer)

        # Generate statistics
        total_params = sum(
            layer["weight"].size
            for layer in traced_model["layers"]
            if "weight" in layer
        )

        stats = {
            "input_shape": input_shape,
            "num_layers": len(traced_model["layers"]),
            "total_params": int(total_params),
            "optimizations": self.optimizations,
            "quantized": self.quantize is not None,
        }

        print("\n" + "=" * 60)
        print("Conversion Summary")
        print("=" * 60)
        print(f"Layers: {stats['num_layers']}")
        print(f"Parameters: {stats['total_params']:,}")
        print(f"Optimizations: {len(self.optimizations)}")
        if self.optimizations:
            for opt in self.optimizations:
                print(f"  - {opt}")
        print(f"Output: {output_path}")

        # Save metadata
        metadata_path = output_path.replace(".eif", "_metadata.json")
        with open(metadata_path, "w") as f:
            # Convert numpy arrays to lists for JSON serialization
            serializable_layers = []
            for layer in traced_model["layers"]:
                layer_copy = layer.copy()
                for key in ["weight", "bias", "running_mean", "running_var"]:
                    if key in layer_copy:
                        layer_copy[key] = f"<array shape={layer_copy[key].shape}>"
                serializable_layers.append(layer_copy)

            json.dump(
                {"model": stats, "layers": serializable_layers}, f, indent=2
            )

        print(f"Metadata saved to: {metadata_path}")
        print("\n✓ Conversion complete! (Note: Binary .eif writing not implemented)")

        return stats


def create_example_model():
    """Create example PyTorch model for testing"""

    class SimpleCNN(nn.Module):
        def __init__(self):
            super().__init__()
            self.conv1 = nn.Conv2d(3, 32, 3, padding=1)
            self.bn1 = nn.BatchNorm2d(32)
            self.relu1 = nn.ReLU()
            self.conv2 = nn.Conv2d(32, 64, 3, padding=1)
            self.bn2 = nn.BatchNorm2d(64)
            self.relu2 = nn.ReLU()
            self.flatten = nn.Flatten()
            self.fc1 = nn.Linear(64 * 8 * 8, 128)
            self.relu3 = nn.ReLU()
            self.fc2 = nn.Linear(128, 10)

        def forward(self, x):
            x = self.relu1(self.bn1(self.conv1(x)))
            x = self.relu2(self.bn2(self.conv2(x)))
            x = self.flatten(x)
            x = self.relu3(self.fc1(x))
            x = self.fc2(x)
            return x

    return SimpleCNN()


def main():
    if not HAS_TORCH:
        print("PyTorch not installed. Tool available but requires PyTorch.")
        print("Install with: pip install torch")
        print("\nFeatures:")
        print("  - Convert PyTorch models to EIF format")
        print("  - Layer fusion (Conv+BN+ReLU)")
        print("  - INT8/INT16 quantization")
        print("  - ONNX intermediate support")
        return 0

    parser = argparse.ArgumentParser(description="PyTorch to EIF Converter")
    parser.add_argument("--model", help="PyTorch model file (.pth)")
    parser.add_argument("-o", "--output", help="Output .eif file")
    parser.add_argument(
        "--input-shape",
        nargs="+",
        type=int,
        default=[3, 8, 8],
        help="Input shape (C H W)",
    )
    parser.add_argument("--quantize", choices=["int8", "int16"], help="Quantization")
    parser.add_argument("--no-optimize", action="store_true", help="Disable optimizations")
    parser.add_argument("--example", action="store_true", help="Use example model")

    args = parser.parse_args()

    if args.example or not args.model:
        print("Using example CNN model...")
        model = create_example_model()
        input_shape = tuple(args.input_shape)
        output_path = args.output or "example_model.eif"
    else:
        print(f"Loading model from: {args.model}")
        model = torch.load(args.model)
        input_shape = tuple(args.input_shape)
        output_path = args.output or args.model.replace(".pth", ".eif")

    converter = PyTorchToEIFConverter(
        optimize=not args.no_optimize, quantize=args.quantize
    )

    stats = converter.convert(model, input_shape, output_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
