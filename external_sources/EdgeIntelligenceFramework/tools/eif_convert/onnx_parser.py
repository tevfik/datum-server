"""
ONNX Model Parser for EIF Convert

Parses ONNX models and converts them to EIF layer representation.
Supports models from PyTorch, TensorFlow, and other ONNX-compatible frameworks.

Requirements:
    pip install onnx onnxruntime

Usage:
    from onnx_parser import ONNXParser
    
    parser = ONNXParser()
    layers = parser.parse('model.onnx')
"""

from typing import Dict, List, Any, Optional, Tuple
from pathlib import Path
import numpy as np

try:
    import onnx
    from onnx import numpy_helper
    HAS_ONNX = True
except ImportError:
    HAS_ONNX = False


# ONNX operator to EIF layer type mapping
ONNX_OP_MAP = {
    # Convolution
    'Conv': 'conv2d',
    'ConvTranspose': 'conv2d_transpose',
    
    # Pooling
    'MaxPool': 'maxpool2d',
    'AveragePool': 'avgpool2d',
    'GlobalAveragePool': 'global_avgpool',
    'GlobalMaxPool': 'global_maxpool',
    
    # Activations
    'Relu': 'relu',
    'LeakyRelu': 'leaky_relu',
    'Sigmoid': 'sigmoid',
    'Tanh': 'tanh',
    'Softmax': 'softmax',
    'Clip': 'relu6',  # Special case for ReLU6
    
    # Normalization
    'BatchNormalization': 'batchnorm',
    'LayerNormalization': 'layernorm',
    
    # Linear
    'Gemm': 'dense',
    'MatMul': 'matmul',
    
    # Shape operations
    'Flatten': 'flatten',
    'Reshape': 'reshape',
    'Squeeze': 'squeeze',
    'Unsqueeze': 'unsqueeze',
    'Transpose': 'transpose',
    
    # Merge operations
    'Add': 'add',
    'Concat': 'concat',
    'Mul': 'multiply',
    'Sub': 'subtract',
    
    # RNN
    'LSTM': 'lstm',
    'GRU': 'gru',
    'RNN': 'rnn',
    
    # Misc
    'Dropout': 'dropout',
    'Pad': 'pad',
    'Constant': 'constant',
}

# Operators that can be fused or skipped
FUSABLE_OPS = {'BatchNormalization', 'Relu', 'Clip'}
SKIP_OPS = {'Dropout', 'Identity', 'Constant', 'Shape', 'Gather', 'Cast'}


class ONNXParser:
    """
    Parse ONNX models and extract layer information.
    
    Features:
    - Full ONNX operator coverage for CNN/RNN models
    - Weight extraction and shape inference
    - BatchNorm folding with preceding Conv/Linear
    - Activation fusion
    """
    
    def __init__(self):
        if not HAS_ONNX:
            raise ImportError("ONNX not installed. Run: pip install onnx onnxruntime")
        
        self.model = None
        self.graph = None
        self.weights = {}
        self.shapes = {}
        self.layers = []
        
    def parse(self, model_path: str) -> List[Dict]:
        """
        Parse ONNX model and return layer configurations.
        
        Args:
            model_path: Path to .onnx file
            
        Returns:
            List of layer dictionaries
        """
        self.model = onnx.load(model_path)
        self.graph = self.model.graph
        
        # Extract initializers (weights)
        self._extract_weights()
        
        # Infer shapes
        self._infer_shapes()
        
        # Parse nodes
        self.layers = []
        skip_next = set()
        
        for i, node in enumerate(self.graph.node):
            if node.name in skip_next:
                continue
                
            if node.op_type in SKIP_OPS:
                continue
            
            layer = self._parse_node(node, i)
            if layer:
                # Check for fusion opportunities
                next_node = self._get_next_node(node)
                if next_node:
                    layer, fused = self._try_fuse(layer, next_node)
                    if fused:
                        skip_next.add(next_node.name)
                
                self.layers.append(layer)
        
        return self.layers
    
    def get_weights(self) -> Dict[str, np.ndarray]:
        """Get all extracted weights."""
        return self.weights
    
    def get_input_shape(self) -> Tuple:
        """Get model input shape."""
        if self.graph and self.graph.input:
            input_info = self.graph.input[0]
            dims = input_info.type.tensor_type.shape.dim
            return tuple(d.dim_value for d in dims)
        return (1,)
    
    def get_output_shape(self) -> Tuple:
        """Get model output shape."""
        if self.graph and self.graph.output:
            output_info = self.graph.output[0]
            dims = output_info.type.tensor_type.shape.dim
            return tuple(d.dim_value for d in dims)
        return (1,)
    
    def _extract_weights(self):
        """Extract weights from initializers."""
        self.weights = {}
        for init in self.graph.initializer:
            arr = numpy_helper.to_array(init)
            self.weights[init.name] = arr
    
    def _infer_shapes(self):
        """Infer intermediate tensor shapes."""
        try:
            from onnx import shape_inference
            inferred = shape_inference.infer_shapes(self.model)
            
            for vi in inferred.graph.value_info:
                dims = vi.type.tensor_type.shape.dim
                self.shapes[vi.name] = tuple(
                    d.dim_value if d.dim_value > 0 else -1 
                    for d in dims
                )
        except Exception:
            # Shape inference may fail, continue without
            pass
    
    def _get_attr(self, node, name: str, default=None):
        """Get attribute value from node."""
        for attr in node.attribute:
            if attr.name == name:
                if attr.type == onnx.AttributeProto.INT:
                    return attr.i
                elif attr.type == onnx.AttributeProto.INTS:
                    return list(attr.ints)
                elif attr.type == onnx.AttributeProto.FLOAT:
                    return attr.f
                elif attr.type == onnx.AttributeProto.FLOATS:
                    return list(attr.floats)
                elif attr.type == onnx.AttributeProto.STRING:
                    return attr.s.decode()
        return default
    
    def _parse_node(self, node, index: int) -> Optional[Dict]:
        """Parse a single ONNX node."""
        op_type = node.op_type
        
        if op_type not in ONNX_OP_MAP:
            print(f"Warning: Unsupported ONNX op '{op_type}', skipping")
            return None
        
        layer = {
            'name': node.name or f"layer_{index}",
            'type': ONNX_OP_MAP[op_type],
            'onnx_op': op_type,
            'inputs': list(node.input),
            'outputs': list(node.output),
            'config': {},
            'weights': [],
        }
        
        # Parse operation-specific parameters
        if op_type == 'Conv':
            layer = self._parse_conv(node, layer)
        elif op_type == 'Gemm':
            layer = self._parse_gemm(node, layer)
        elif op_type in ('MaxPool', 'AveragePool'):
            layer = self._parse_pool(node, layer)
        elif op_type == 'BatchNormalization':
            layer = self._parse_batchnorm(node, layer)
        elif op_type == 'LeakyRelu':
            layer['config']['alpha'] = self._get_attr(node, 'alpha', 0.01)
        elif op_type == 'Softmax':
            layer['config']['axis'] = self._get_attr(node, 'axis', -1)
        elif op_type in ('LSTM', 'GRU', 'RNN'):
            layer = self._parse_rnn(node, layer)
        elif op_type == 'Reshape':
            layer = self._parse_reshape(node, layer)
        
        # Extract weights for this layer
        self._extract_layer_weights(layer)
        
        return layer
    
    def _parse_conv(self, node, layer: Dict) -> Dict:
        """Parse Conv node attributes."""
        kernel_shape = self._get_attr(node, 'kernel_shape', [3, 3])
        strides = self._get_attr(node, 'strides', [1, 1])
        pads = self._get_attr(node, 'pads', [0, 0, 0, 0])
        dilations = self._get_attr(node, 'dilations', [1, 1])
        group = self._get_attr(node, 'group', 1)
        
        # Get number of filters from weight shape
        weight_name = node.input[1] if len(node.input) > 1 else None
        filters = 1
        if weight_name and weight_name in self.weights:
            filters = self.weights[weight_name].shape[0]
        
        layer['config'] = {
            'filters': filters,
            'kernel_size': tuple(kernel_shape),
            'strides': tuple(strides),
            'padding': 'same' if sum(pads) > 0 else 'valid',
            'dilation_rate': tuple(dilations),
            'groups': group,
        }
        
        # Depthwise conv
        if group > 1 and filters == group:
            layer['type'] = 'dwconv2d'
        
        return layer
    
    def _parse_gemm(self, node, layer: Dict) -> Dict:
        """Parse Gemm (fully connected) node."""
        transA = self._get_attr(node, 'transA', 0)
        transB = self._get_attr(node, 'transB', 0)
        alpha = self._get_attr(node, 'alpha', 1.0)
        beta = self._get_attr(node, 'beta', 1.0)
        
        # Get units from weight shape
        weight_name = node.input[1] if len(node.input) > 1 else None
        units = 1
        if weight_name and weight_name in self.weights:
            w = self.weights[weight_name]
            units = w.shape[0] if transB else w.shape[1]
        
        layer['config'] = {
            'units': units,
            'transA': transA,
            'transB': transB,
        }
        
        return layer
    
    def _parse_pool(self, node, layer: Dict) -> Dict:
        """Parse pooling node."""
        kernel_shape = self._get_attr(node, 'kernel_shape', [2, 2])
        strides = self._get_attr(node, 'strides', [2, 2])
        pads = self._get_attr(node, 'pads', [0, 0, 0, 0])
        
        layer['config'] = {
            'pool_size': tuple(kernel_shape),
            'strides': tuple(strides),
            'padding': 'same' if sum(pads) > 0 else 'valid',
        }
        
        return layer
    
    def _parse_batchnorm(self, node, layer: Dict) -> Dict:
        """Parse BatchNormalization node."""
        epsilon = self._get_attr(node, 'epsilon', 1e-5)
        momentum = self._get_attr(node, 'momentum', 0.9)
        
        layer['config'] = {
            'epsilon': epsilon,
            'momentum': momentum,
        }
        
        return layer
    
    def _parse_rnn(self, node, layer: Dict) -> Dict:
        """Parse RNN/LSTM/GRU node."""
        hidden_size = self._get_attr(node, 'hidden_size', 128)
        direction = self._get_attr(node, 'direction', 'forward')
        
        layer['config'] = {
            'hidden_size': hidden_size,
            'direction': direction,
            'bidirectional': direction == 'bidirectional',
        }
        
        return layer
    
    def _parse_reshape(self, node, layer: Dict) -> Dict:
        """Parse Reshape node."""
        shape_input = node.input[1] if len(node.input) > 1 else None
        if shape_input and shape_input in self.weights:
            target_shape = tuple(self.weights[shape_input].astype(int))
            layer['config']['target_shape'] = target_shape
        
        return layer
    
    def _extract_layer_weights(self, layer: Dict):
        """Extract weights for a layer."""
        for input_name in layer['inputs']:
            if input_name in self.weights:
                layer['weights'].append({
                    'name': input_name,
                    'data': self.weights[input_name],
                    'shape': self.weights[input_name].shape,
                })
    
    def _get_next_node(self, node):
        """Get the node that consumes this node's output."""
        if not node.output:
            return None
        
        output_name = node.output[0]
        
        for n in self.graph.node:
            if output_name in n.input:
                return n
        
        return None
    
    def _try_fuse(self, layer: Dict, next_node) -> Tuple[Dict, bool]:
        """Try to fuse activation or batchnorm into previous layer."""
        op_type = next_node.op_type
        
        # Fuse ReLU
        if op_type == 'Relu':
            layer['config']['activation'] = 'relu'
            return layer, True
        
        # Fuse LeakyReLU
        if op_type == 'LeakyRelu':
            alpha = self._get_attr(next_node, 'alpha', 0.01)
            layer['config']['activation'] = 'leaky_relu'
            layer['config']['alpha'] = alpha
            return layer, True
        
        # Fuse ReLU6 (Clip with min=0, max=6)
        if op_type == 'Clip':
            min_val = self._get_attr(next_node, 'min', 0.0)
            max_val = self._get_attr(next_node, 'max', 6.0)
            if min_val == 0.0 and max_val == 6.0:
                layer['config']['activation'] = 'relu6'
                return layer, True
        
        # Fuse BatchNorm (if layer is Conv or Dense)
        if op_type == 'BatchNormalization' and layer['type'] in ('conv2d', 'dense'):
            layer = self._fold_batchnorm(layer, next_node)
            return layer, True
        
        return layer, False
    
    def _fold_batchnorm(self, layer: Dict, bn_node) -> Dict:
        """Fold BatchNorm into preceding Conv/Dense."""
        epsilon = self._get_attr(bn_node, 'epsilon', 1e-5)
        
        # Get BN parameters: scale (gamma), bias (beta), mean, var
        bn_inputs = list(bn_node.input)
        if len(bn_inputs) >= 5:
            gamma = self.weights.get(bn_inputs[1])
            beta = self.weights.get(bn_inputs[2])
            mean = self.weights.get(bn_inputs[3])
            var = self.weights.get(bn_inputs[4])
            
            if all(x is not None for x in [gamma, beta, mean, var]):
                # Compute scale and bias for folded weights
                std = np.sqrt(var + epsilon)
                scale = gamma / std
                bias = beta - mean * scale
                
                # Update layer weights
                layer['bn_folded'] = True
                layer['bn_scale'] = scale
                layer['bn_bias'] = bias
        
        return layer


def parse_onnx(model_path: str) -> Tuple[List[Dict], Dict[str, np.ndarray]]:
    """
    Convenience function to parse ONNX model.
    
    Args:
        model_path: Path to .onnx file
        
    Returns:
        Tuple of (layers, weights)
    """
    parser = ONNXParser()
    layers = parser.parse(model_path)
    weights = parser.get_weights()
    return layers, weights
