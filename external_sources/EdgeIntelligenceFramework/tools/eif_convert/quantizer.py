"""
Quantizer - Convert floating-point weights to fixed-point

Supports Q15, Q7, INT8, and float formats.
"""

from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass
import numpy as np


@dataclass
class QuantizationConfig:
    """Quantization configuration."""
    method: str = 'q15'  # 'q15', 'q7', 'int8', 'float'
    per_channel: bool = False
    symmetric: bool = True
    clamp_range: float = 0.99  # Percentile for range clamping


class Quantizer:
    """
    Quantize model weights to fixed-point representation.
    
    Supported formats:
    - q15: Q1.15 signed fixed-point (-1.0 to ~1.0)
    - q7: Q1.7 signed fixed-point (-1.0 to ~1.0)  
    - int8: Symmetric INT8 with scale factor
    - float: No quantization (keep float32)
    """
    
    # Format specifications
    FORMATS = {
        'q15': {'bits': 16, 'frac_bits': 15, 'dtype': 'int16', 'max_val': 32767},
        'q7': {'bits': 8, 'frac_bits': 7, 'dtype': 'int8', 'max_val': 127},
        'int8': {'bits': 8, 'frac_bits': 0, 'dtype': 'int8', 'max_val': 127},
        'float': {'bits': 32, 'frac_bits': 0, 'dtype': 'float32', 'max_val': None}
    }
    
    def __init__(self, config: QuantizationConfig):
        self.config = config
        self.format = self.FORMATS.get(config.method, self.FORMATS['q15'])
        
    def quantize_model(self, 
                       layers: List[Dict[str, Any]],
                       calibration_data: Optional[np.ndarray] = None) -> Dict[str, Any]:
        """
        Quantize all model weights.
        
        Args:
            layers: Parsed layer list from KerasParser
            calibration_data: Optional data for activation range estimation
            
        Returns:
            Dictionary with quantized weights and metadata
        """
        result = {
            'layers': [],
            'format': self.config.method,
            'per_channel': self.config.per_channel
        }
        
        for layer in layers:
            q_layer = self._quantize_layer(layer)
            result['layers'].append(q_layer)
            
        return result
    
    def _quantize_layer(self, layer: Dict[str, Any]) -> Dict[str, Any]:
        """Quantize a single layer's weights."""
        q_layer = {
            'name': layer['name'],
            'type': layer['type'],
            'config': layer['config'].copy(),
            'input_shape': layer['input_shape'],
            'output_shape': layer['output_shape'],
            'weights': [],
            'shifts': [],
            'scales': []
        }
        
        if not layer['weights']:
            return q_layer
            
        # Quantize each weight tensor
        for i, w in enumerate(layer['weights']):
            is_bias = (i == 1)  # Usually second weight is bias
            
            if self.config.method == 'float':
                q_layer['weights'].append(w.astype(np.float32))
                q_layer['shifts'].append(0)
                q_layer['scales'].append(1.0)
            else:
                q_weights, shift, scale = self._quantize_weights(
                    w, 
                    layer['type'],
                    per_channel=self.config.per_channel and not is_bias,
                    is_bias=is_bias
                )
                q_layer['weights'].append(q_weights)
                q_layer['shifts'].append(shift)
                q_layer['scales'].append(scale)
                
        return q_layer
    
    def _quantize_weights(self, 
                          weights: np.ndarray,
                          layer_type: str,
                          per_channel: bool = False,
                          is_bias: bool = False) -> Tuple[np.ndarray, Any, Any]:
        """
        Quantize weight tensor.
        
        Args:
            weights: Float weight tensor
            layer_type: Type of layer (conv2d, dense, etc.)
            per_channel: Use per-channel quantization
            is_bias: Whether this is a bias tensor
            
        Returns:
            (quantized_weights, shift, scale)
        """
        fmt = self.format
        
        if per_channel and not is_bias:
            return self._quantize_per_channel(weights, layer_type)
        else:
            return self._quantize_per_tensor(weights)
    
    def _quantize_per_tensor(self, weights: np.ndarray) -> Tuple[np.ndarray, int, float]:
        """Per-tensor quantization."""
        fmt = self.format
        
        # Clamp outliers
        percentile = self.config.clamp_range * 100
        w_min = np.percentile(weights, 100 - percentile)
        w_max = np.percentile(weights, percentile)
        weights_clamped = np.clip(weights, w_min, w_max)
        
        # Find scale
        abs_max = np.max(np.abs(weights_clamped))
        if abs_max < 1e-10:
            abs_max = 1e-10
            
        if self.config.method in ['q15', 'q7']:
            # Q format: scale to [-1, 1) range
            scale = abs_max
            shift = fmt['frac_bits']
            q_weights = np.round(weights_clamped / scale * fmt['max_val'])
        else:
            # INT8: use max_val directly
            scale = abs_max / fmt['max_val']
            shift = 0
            q_weights = np.round(weights_clamped / scale)
            
        q_weights = np.clip(q_weights, -fmt['max_val'], fmt['max_val'])
        q_weights = q_weights.astype(np.dtype(fmt['dtype']))
        
        return q_weights, shift, scale
    
    def _quantize_per_channel(self, weights: np.ndarray, 
                               layer_type: str) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """Per-channel quantization for better accuracy."""
        fmt = self.format
        
        # Determine output channel axis
        if layer_type in ['conv2d', 'dwconv2d']:
            # Conv weights: [H, W, C_in, C_out] - output channel is last
            axis = -1
        elif layer_type == 'dense':
            # Dense weights: [in, out] - output channel is last
            axis = -1
        else:
            # Default to last axis
            axis = -1
            
        num_channels = weights.shape[axis]
        
        # Quantize each channel separately
        q_weights = np.zeros_like(weights)
        shifts = np.zeros(num_channels, dtype=np.int8)
        scales = np.zeros(num_channels, dtype=np.float32)
        
        for c in range(num_channels):
            # Extract channel slice
            slices = [slice(None)] * weights.ndim
            slices[axis] = c
            channel_weights = weights[tuple(slices)]
            
            # Quantize
            q_channel, shift, scale = self._quantize_per_tensor(channel_weights)
            
            # Store
            q_weights[tuple(slices)] = q_channel
            shifts[c] = shift
            scales[c] = scale
            
        q_weights = q_weights.astype(np.dtype(fmt['dtype']))
        return q_weights, shifts, scales
    
    def dequantize(self, q_weights: np.ndarray, shift: int, scale: float) -> np.ndarray:
        """Dequantize weights back to float for verification."""
        if self.config.method == 'float':
            return q_weights
            
        if self.config.method in ['q15', 'q7']:
            return q_weights.astype(np.float32) / self.format['max_val'] * scale
        else:
            return q_weights.astype(np.float32) * scale


def compute_output_shift(w_shift: int, input_shift: int, output_shift: int) -> int:
    """
    Compute the output shift for accumulator.
    
    For Q15 multiplication: result has 30 fractional bits
    To get back to Q15 output: shift right by (30 - 15) = 15
    
    If weights and inputs have different shifts, adjust accordingly.
    """
    acc_frac_bits = w_shift + input_shift
    return acc_frac_bits - output_shift
