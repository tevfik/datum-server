"""
EIF Convert - Model Converter for Edge Intelligence Framework

Convert Keras/TensorFlow/ONNX models to optimized C code for embedded deployment.

Usage:
    import eif_convert as eif
    eif.convert(keras_model, 'model_weights.h')
    
    # Or from command line:
    python -m eif_convert model.h5 -o output/
    python -m eif_convert model.onnx -o output/
"""

from .converter import convert, EIFConverter
from .keras_parser import KerasParser
from .quantizer import Quantizer, QuantizationConfig
from .code_generator import CodeGenerator

# Optional ONNX support
try:
    from .onnx_parser import ONNXParser
except ImportError:
    ONNXParser = None

__version__ = "1.1.0"
__all__ = [
    "convert",
    "EIFConverter", 
    "KerasParser",
    "ONNXParser",
    "Quantizer",
    "QuantizationConfig",
    "CodeGenerator"
]

