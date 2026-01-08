"""
EIF Converter - Main conversion API

Provides the main convert() function and EIFConverter class.
"""

import os
from typing import Optional, Union, List
from pathlib import Path

# Try to import TensorFlow/Keras
try:
    import tensorflow as tf
    from tensorflow import keras
    HAS_TF = True
except ImportError:
    HAS_TF = False
    
try:
    import numpy as np
except ImportError:
    raise ImportError("NumPy is required. Install with: pip install numpy")

from .keras_parser import KerasParser
from .quantizer import Quantizer, QuantizationConfig
from .code_generator import CodeGenerator

# Try to import ONNX
try:
    from .onnx_parser import ONNXParser, HAS_ONNX
except ImportError:
    HAS_ONNX = False


class EIFConverter:
    """
    Convert Keras/TensorFlow/ONNX models to EIF C code.
    
    Example:
        converter = EIFConverter()
        converter.load_keras_model('model.h5')
        # or: converter.load_onnx_model('model.onnx')
        converter.quantize(method='q15', per_channel=True)
        converter.generate('output/')
    """
    
    def __init__(self):
        self.model = None
        self.parsed_layers = []
        self.quantized_weights = {}
        self.config = QuantizationConfig()
        self.model_type = None  # 'keras' or 'onnx'
        
    def load_keras_model(self, model_or_path: Union[str, 'keras.Model']) -> 'EIFConverter':
        """Load a Keras model from file or model object."""
        if not HAS_TF:
            raise ImportError("TensorFlow/Keras is required. Install with: pip install tensorflow")
            
        if isinstance(model_or_path, str):
            self.model = keras.models.load_model(model_or_path)
        else:
            self.model = model_or_path
            
        # Parse the model
        parser = KerasParser()
        self.parsed_layers = parser.parse(self.model)
        self.model_type = 'keras'
        
        return self
    
    def load_onnx_model(self, model_path: str) -> 'EIFConverter':
        """Load an ONNX model from file."""
        if not HAS_ONNX:
            raise ImportError("ONNX is required. Install with: pip install onnx")
        
        parser = ONNXParser()
        self.parsed_layers = parser.parse(model_path)
        self.model_type = 'onnx'
        
        # Store ONNX weights for quantization
        self._onnx_weights = parser.get_weights()
        
        return self
    
    def quantize(self, 
                 method: str = 'q15',
                 per_channel: bool = False,
                 calibration_data: Optional[np.ndarray] = None) -> 'EIFConverter':
        """
        Quantize model weights.
        
        Args:
            method: 'q15', 'q7', 'int8', or 'float'
            per_channel: Use per-channel quantization for convolutions
            calibration_data: Optional calibration dataset for activation ranges
        """
        self.config.method = method
        self.config.per_channel = per_channel
        
        quantizer = Quantizer(self.config)
        self.quantized_weights = quantizer.quantize_model(
            self.parsed_layers, 
            calibration_data
        )
        
        return self
    
    def generate(self, 
                 output_path: str,
                 model_name: str = 'model',
                 include_inference: bool = True) -> List[str]:
        """
        Generate C code files.
        
        Args:
            output_path: Directory to write output files
            model_name: Base name for generated files
            include_inference: Generate inference function
            
        Returns:
            List of generated file paths
        """
        output_dir = Path(output_path)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        generator = CodeGenerator(
            model_name=model_name,
            config=self.config
        )
        
        files = generator.generate(
            layers=self.parsed_layers,
            weights=self.quantized_weights,
            output_dir=output_dir,
            include_inference=include_inference
        )
        
        return files
    
    def summary(self) -> str:
        """Print model summary."""
        lines = []
        lines.append(f"EIF Model Summary")
        lines.append("=" * 60)
        
        total_params = 0
        for layer in self.parsed_layers:
            params = layer.get('params', 0)
            total_params += params
            lines.append(f"  {layer['name']:20} {layer['type']:15} {params:>10} params")
        
        lines.append("=" * 60)
        lines.append(f"  Total parameters: {total_params:,}")
        lines.append(f"  Quantization: {self.config.method}")
        lines.append(f"  Per-channel: {self.config.per_channel}")
        
        return "\n".join(lines)


def convert(model, 
            output: str,
            quantize: str = 'q15',
            per_channel: bool = False,
            calibration_data: Optional[np.ndarray] = None,
            model_name: str = 'model',
            verbose: bool = True) -> List[str]:
    """
    One-line model conversion.
    
    Args:
        model: Keras model or path to .h5 file
        output: Output directory or header file path
        quantize: Quantization method ('q15', 'q7', 'int8', 'float')
        per_channel: Use per-channel quantization
        calibration_data: Optional calibration dataset
        model_name: Name for the generated model
        verbose: Print progress information
        
    Returns:
        List of generated file paths
        
    Example:
        import eif_convert as eif
        eif.convert('model.h5', 'weights.h')
    """
    converter = EIFConverter()
    
    if verbose:
        print(f"Loading model...")
    converter.load_keras_model(model)
    
    if verbose:
        print(f"Quantizing with {quantize}...")
    converter.quantize(
        method=quantize,
        per_channel=per_channel,
        calibration_data=calibration_data
    )
    
    if verbose:
        print(f"Generating code to {output}...")
    
    # Determine output path
    output_path = Path(output)
    if output_path.suffix in ['.h', '.c']:
        output_dir = output_path.parent
        model_name = output_path.stem
    else:
        output_dir = output_path
        
    files = converter.generate(
        output_path=str(output_dir),
        model_name=model_name
    )
    
    if verbose:
        print(converter.summary())
        print(f"\nGenerated files:")
        for f in files:
            print(f"  {f}")
    
    return files
