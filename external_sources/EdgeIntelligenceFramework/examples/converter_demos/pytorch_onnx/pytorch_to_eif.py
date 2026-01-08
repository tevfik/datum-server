"""
PyTorch to EIF Conversion Demo

This script demonstrates:
1. Create a simple CNN in PyTorch
2. Export to ONNX format
3. Convert to EIF C code

Requirements:
    pip install torch onnx numpy

Usage:
    python pytorch_to_eif.py
"""

import os
import sys

# Check dependencies
try:
    import torch
    import torch.nn as nn
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False
    print("PyTorch not installed. Install with: pip install torch")

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False
    print("NumPy not installed. Install with: pip install numpy")

# Add parent directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../..'))


# =============================================================================
# Step 1: Define PyTorch Model
# =============================================================================

class SimpleCNN(nn.Module):
    """Simple CNN for image classification."""
    
    def __init__(self, num_classes=10):
        super(SimpleCNN, self).__init__()
        
        self.features = nn.Sequential(
            # Conv block 1
            nn.Conv2d(1, 16, kernel_size=3, padding=1),
            nn.BatchNorm2d(16),
            nn.ReLU(),
            nn.MaxPool2d(2),
            
            # Conv block 2
            nn.Conv2d(16, 32, kernel_size=3, padding=1),
            nn.BatchNorm2d(32),
            nn.ReLU(),
            nn.MaxPool2d(2),
        )
        
        self.classifier = nn.Sequential(
            nn.AdaptiveAvgPool2d(1),
            nn.Flatten(),
            nn.Linear(32, num_classes),
        )
    
    def forward(self, x):
        x = self.features(x)
        x = self.classifier(x)
        return x


def create_model():
    """Create and initialize the model."""
    model = SimpleCNN(num_classes=10)
    
    # Initialize with random weights (in practice, load trained weights)
    for m in model.modules():
        if isinstance(m, nn.Conv2d):
            nn.init.kaiming_normal_(m.weight, mode='fan_out', nonlinearity='relu')
            if m.bias is not None:
                nn.init.constant_(m.bias, 0)
        elif isinstance(m, nn.BatchNorm2d):
            nn.init.constant_(m.weight, 1)
            nn.init.constant_(m.bias, 0)
        elif isinstance(m, nn.Linear):
            nn.init.normal_(m.weight, 0, 0.01)
            nn.init.constant_(m.bias, 0)
    
    return model


# =============================================================================
# Step 2: Export to ONNX
# =============================================================================

def export_to_onnx(model, output_path, input_shape=(1, 1, 28, 28)):
    """Export PyTorch model to ONNX format."""
    print(f"\n📦 Exporting to ONNX: {output_path}")
    
    # Set model to eval mode
    model.eval()
    
    # Create dummy input
    dummy_input = torch.randn(*input_shape)
    
    # Export
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        export_params=True,
        opset_version=11,
        do_constant_folding=True,
        input_names=['input'],
        output_names=['output'],
        dynamic_axes={
            'input': {0: 'batch_size'},
            'output': {0: 'batch_size'}
        }
    )
    
    print(f"   ✅ Exported successfully!")
    print(f"   Input shape: {input_shape}")
    
    return output_path


# =============================================================================
# Step 3: Convert to EIF
# =============================================================================

def convert_to_eif(onnx_path, output_dir):
    """Convert ONNX model to EIF C code."""
    print(f"\n⚙️  Converting to EIF: {output_dir}")
    
    try:
        from tools.eif_convert import EIFConverter
        
        converter = EIFConverter()
        converter.load_onnx_model(onnx_path)
        
        print(f"   Parsed {len(converter.parsed_layers)} layers")
        
        # Print layer summary
        print("\n   Layer Summary:")
        print("   " + "-" * 50)
        for i, layer in enumerate(converter.parsed_layers[:10]):
            print(f"   {i:2}. {layer['type']:15} {layer['name'][:25]}")
        if len(converter.parsed_layers) > 10:
            print(f"   ... and {len(converter.parsed_layers) - 10} more layers")
        
        # Note: Full quantization and code generation would happen here
        # converter.quantize(method='q15', per_channel=True)
        # converter.generate(output_dir, model_name='pytorch_cnn')
        
        print(f"\n   ✅ ONNX parsing successful!")
        return True
        
    except ImportError as e:
        print(f"   ⚠️  ONNX support not available: {e}")
        print("   Install with: pip install onnx")
        return False
    except Exception as e:
        print(f"   ❌ Conversion failed: {e}")
        return False


# =============================================================================
# Main
# =============================================================================

def main():
    print("=" * 60)
    print("PyTorch to EIF Conversion Demo")
    print("=" * 60)
    
    if not HAS_TORCH:
        print("\n❌ PyTorch required. Install with: pip install torch")
        return 1
    
    # Create output directory
    os.makedirs('output', exist_ok=True)
    
    # Step 1: Create model
    print("\n🏗️  Creating PyTorch model...")
    model = create_model()
    print("   Model architecture:")
    print(f"   {model}")
    
    # Count parameters
    total_params = sum(p.numel() for p in model.parameters())
    print(f"\n   Total parameters: {total_params:,}")
    
    # Step 2: Export to ONNX
    onnx_path = 'output/pytorch_model.onnx'
    export_to_onnx(model, onnx_path)
    
    # Step 3: Convert to EIF
    convert_to_eif(onnx_path, 'output')
    
    print("\n" + "=" * 60)
    print("Demo complete!")
    print("=" * 60)
    print("\nGenerated files:")
    print(f"  • {onnx_path}")
    print("\nNext steps:")
    print("  1. Train your model with real data")
    print("  2. Export trained weights to ONNX")
    print("  3. Run full conversion: python -m eif_convert model.onnx -o output/")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
