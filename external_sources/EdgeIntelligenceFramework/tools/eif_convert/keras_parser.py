"""
Keras Model Parser

Parses Keras/TensorFlow models and extracts layer information.
"""

from typing import List, Dict, Any, Optional
from dataclasses import dataclass, field

try:
    import tensorflow as tf
    from tensorflow import keras
    HAS_TF = True
except ImportError:
    HAS_TF = False

import numpy as np


@dataclass
class LayerInfo:
    """Parsed layer information."""
    name: str
    type: str
    input_shape: tuple
    output_shape: tuple
    weights: List[np.ndarray] = field(default_factory=list)
    config: Dict[str, Any] = field(default_factory=dict)
    params: int = 0


# Supported layer types
SUPPORTED_LAYERS = {
    'InputLayer': 'input',
    'Conv2D': 'conv2d',
    'DepthwiseConv2D': 'dwconv2d',
    'SeparableConv2D': 'sepconv2d',
    'Dense': 'dense',
    'MaxPooling2D': 'maxpool2d',
    'AveragePooling2D': 'avgpool2d',
    'GlobalMaxPooling2D': 'global_maxpool2d',
    'GlobalAveragePooling2D': 'global_avgpool2d',
    'Flatten': 'flatten',
    'Reshape': 'reshape',
    'Dropout': 'dropout',  # Ignored in inference
    'BatchNormalization': 'batchnorm',
    'Activation': 'activation',
    'ReLU': 'relu',
    'LeakyReLU': 'leaky_relu',
    'Softmax': 'softmax',
    'Concatenate': 'concat',
    'Add': 'add',
    'Multiply': 'multiply',
    'ZeroPadding2D': 'zeropad2d',
}


class KerasParser:
    """
    Parse Keras models to extract layer information.
    
    Example:
        parser = KerasParser()
        layers = parser.parse(keras_model)
        for layer in layers:
            print(f"{layer['name']}: {layer['type']}")
    """
    
    def __init__(self, fold_batchnorm: bool = True, fuse_activation: bool = True):
        """
        Args:
            fold_batchnorm: Fold BatchNorm into preceding Conv/Dense layers
            fuse_activation: Fuse activation into preceding layer
        """
        self.fold_batchnorm = fold_batchnorm
        self.fuse_activation = fuse_activation
        
    def parse(self, model: 'keras.Model') -> List[Dict[str, Any]]:
        """
        Parse a Keras model.
        
        Args:
            model: Keras model object
            
        Returns:
            List of layer dictionaries
        """
        if not HAS_TF:
            raise ImportError("TensorFlow is required")
            
        layers = []
        
        for keras_layer in model.layers:
            layer_info = self._parse_layer(keras_layer)
            if layer_info:
                layers.append(layer_info)
        
        # Post-processing
        if self.fold_batchnorm:
            layers = self._fold_batchnorm(layers)
        if self.fuse_activation:
            layers = self._fuse_activations(layers)
            
        return layers
    
    def _parse_layer(self, layer) -> Optional[Dict[str, Any]]:
        """Parse a single Keras layer."""
        layer_class = layer.__class__.__name__
        
        if layer_class not in SUPPORTED_LAYERS:
            print(f"Warning: Unsupported layer type '{layer_class}', skipping")
            return None
        
        layer_type = SUPPORTED_LAYERS[layer_class]
        
        # Skip dropout (not used in inference)
        if layer_type == 'dropout':
            return None
            
        # Get shapes
        try:
            input_shape = tuple(layer.input.shape.as_list())
            output_shape = tuple(layer.output.shape.as_list())
        except:
            input_shape = (None,)
            output_shape = (None,)
        
        # Get weights
        weights = [w.numpy() for w in layer.weights]
        
        # Calculate parameters
        params = sum(w.size for w in weights)
        
        # Get layer config
        config = self._get_layer_config(layer, layer_type)
        
        return {
            'name': layer.name,
            'type': layer_type,
            'input_shape': input_shape,
            'output_shape': output_shape,
            'weights': weights,
            'config': config,
            'params': params
        }
    
    def _get_layer_config(self, layer, layer_type: str) -> Dict[str, Any]:
        """Extract layer-specific configuration."""
        config = {}
        
        if layer_type == 'conv2d':
            config['filters'] = layer.filters
            config['kernel_size'] = layer.kernel_size
            config['strides'] = layer.strides
            config['padding'] = layer.padding
            config['use_bias'] = layer.use_bias
            config['activation'] = self._get_activation_name(layer.activation)
            
        elif layer_type == 'dwconv2d':
            config['depth_multiplier'] = layer.depth_multiplier
            config['kernel_size'] = layer.kernel_size
            config['strides'] = layer.strides
            config['padding'] = layer.padding
            config['use_bias'] = layer.use_bias
            config['activation'] = self._get_activation_name(layer.activation)
            
        elif layer_type == 'dense':
            config['units'] = layer.units
            config['use_bias'] = layer.use_bias
            config['activation'] = self._get_activation_name(layer.activation)
            
        elif layer_type in ['maxpool2d', 'avgpool2d']:
            config['pool_size'] = layer.pool_size
            config['strides'] = layer.strides
            config['padding'] = layer.padding
            
        elif layer_type == 'batchnorm':
            config['epsilon'] = layer.epsilon
            config['momentum'] = layer.momentum
            
        elif layer_type == 'leaky_relu':
            config['alpha'] = layer.alpha
            
        elif layer_type == 'activation':
            config['activation'] = self._get_activation_name(layer.activation)
            
        return config
    
    def _get_activation_name(self, activation) -> str:
        """Get activation function name."""
        if activation is None:
            return 'linear'
        if hasattr(activation, '__name__'):
            return activation.__name__
        return str(activation)
    
    def _fold_batchnorm(self, layers: List[Dict]) -> List[Dict]:
        """Fold BatchNorm into preceding Conv/Dense layers."""
        result = []
        i = 0
        
        while i < len(layers):
            layer = layers[i]
            
            # Check if next layer is BatchNorm
            if (i + 1 < len(layers) and 
                layers[i + 1]['type'] == 'batchnorm' and
                layer['type'] in ['conv2d', 'dwconv2d', 'dense']):
                
                bn_layer = layers[i + 1]
                folded = self._fold_bn_into_layer(layer, bn_layer)
                result.append(folded)
                i += 2  # Skip both layers
            else:
                if layer['type'] != 'batchnorm':
                    result.append(layer)
                i += 1
                
        return result
    
    def _fold_bn_into_layer(self, layer: Dict, bn_layer: Dict) -> Dict:
        """Fold BatchNorm parameters into layer weights."""
        weights = layer['weights']
        bn_weights = bn_layer['weights']
        
        # BN weights: [gamma, beta, moving_mean, moving_variance]
        if len(bn_weights) == 4:
            gamma = bn_weights[0]
            beta = bn_weights[1]
            mean = bn_weights[2]
            var = bn_weights[3]
        else:
            # BN without gamma/beta
            gamma = np.ones_like(bn_weights[0])
            beta = np.zeros_like(bn_weights[0])
            mean = bn_weights[0]
            var = bn_weights[1]
            
        epsilon = bn_layer['config'].get('epsilon', 1e-5)
        std = np.sqrt(var + epsilon)
        
        # Scale weights: W' = W * gamma / std
        # Scale bias: b' = (b - mean) * gamma / std + beta
        
        kernel = weights[0]
        if layer['type'] == 'conv2d':
            # Conv weights: [H, W, C_in, C_out]
            kernel = kernel * (gamma / std).reshape(1, 1, 1, -1)
        elif layer['type'] == 'dense':
            # Dense weights: [in, out]
            kernel = kernel * (gamma / std)
            
        if len(weights) > 1:
            bias = weights[1]
            bias = (bias - mean) * gamma / std + beta
        else:
            bias = -mean * gamma / std + beta
            
        layer['weights'] = [kernel, bias]
        layer['config']['use_bias'] = True
        layer['params'] = kernel.size + bias.size
        
        return layer
    
    def _fuse_activations(self, layers: List[Dict]) -> List[Dict]:
        """Fuse standalone activation layers into preceding layers."""
        result = []
        i = 0
        
        while i < len(layers):
            layer = layers[i]
            
            # Check if next layer is activation
            if (i + 1 < len(layers) and 
                layers[i + 1]['type'] in ['activation', 'relu', 'leaky_relu', 'softmax'] and
                layer['type'] in ['conv2d', 'dwconv2d', 'dense'] and
                layer['config'].get('activation', 'linear') == 'linear'):
                
                act_layer = layers[i + 1]
                act_name = act_layer.get('config', {}).get('activation', act_layer['type'])
                layer['config']['activation'] = act_name
                result.append(layer)
                i += 2  # Skip both layers
            else:
                result.append(layer)
                i += 1
                
        return result
    
    def get_supported_layers(self) -> List[str]:
        """Return list of supported Keras layer types."""
        return list(SUPPORTED_LAYERS.keys())
