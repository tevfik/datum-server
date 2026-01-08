"""
EIF Convert CLI

Command-line interface for model conversion.

Usage:
    python -m eif_convert model.h5 -o output/
    python -m eif_convert model.h5 --quantize q7 --per-channel
"""

import argparse
import sys
from pathlib import Path

from .converter import convert


def main():
    parser = argparse.ArgumentParser(
        description='Convert Keras/TensorFlow models to EIF C code',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic conversion
  python -m eif_convert model.h5 -o output/
  
  # With Q7 quantization
  python -m eif_convert model.h5 -o weights.h --quantize q7
  
  # Per-channel quantization
  python -m eif_convert model.h5 -o output/ --quantize q15 --per-channel
  
  # Keep float weights (no quantization)
  python -m eif_convert model.h5 -o output/ --quantize float
        """
    )
    
    parser.add_argument(
        'model',
        help='Input model file (.h5, .keras, or SavedModel directory)'
    )
    
    parser.add_argument(
        '-o', '--output',
        default='./eif_model/',
        help='Output directory or header file (default: ./eif_model/)'
    )
    
    parser.add_argument(
        '-q', '--quantize',
        choices=['q15', 'q7', 'int8', 'float'],
        default='q15',
        help='Quantization method (default: q15)'
    )
    
    parser.add_argument(
        '--per-channel',
        action='store_true',
        help='Use per-channel quantization for conv layers'
    )
    
    parser.add_argument(
        '-n', '--name',
        default='model',
        help='Model name for generated code (default: model)'
    )
    
    parser.add_argument(
        '--no-inference',
        action='store_true',
        help='Do not generate inference function'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Print detailed progress'
    )
    
    parser.add_argument(
        '--version',
        action='version',
        version='eif_convert 1.0.0'
    )
    
    args = parser.parse_args()
    
    # Check model exists
    model_path = Path(args.model)
    if not model_path.exists():
        print(f"Error: Model file not found: {args.model}", file=sys.stderr)
        sys.exit(1)
    
    # Run conversion
    try:
        files = convert(
            model=str(model_path),
            output=args.output,
            quantize=args.quantize,
            per_channel=args.per_channel,
            model_name=args.name,
            verbose=args.verbose
        )
        
        print(f"\n✅ Conversion complete! Generated {len(files)} files.")
        
    except ImportError as e:
        print(f"Error: {e}", file=sys.stderr)
        print("Install TensorFlow with: pip install tensorflow", file=sys.stderr)
        sys.exit(1)
        
    except Exception as e:
        print(f"Error during conversion: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
