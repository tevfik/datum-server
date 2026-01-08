import argparse
import struct
import numpy as np
import os

# Try importing tflite runtime or tensorflow
try:
    import tflite
    from tflite.Model import Model
except ImportError:
    print("Error: 'tflite' package not found. Please install it via 'pip install tflite'.")
    exit(1)

# EIF Constants
EIF_MAGIC = 0x4549464D
EIF_VERSION = 1

# Tensor Types
EIF_TENSOR_FLOAT32 = 0
EIF_TENSOR_INT8 = 1
EIF_TENSOR_UINT8 = 2
EIF_TENSOR_INT16 = 3
EIF_TENSOR_INT32 = 4

# Layer Types (Must match eif_nn_layers.h)
EIF_LAYER_DENSE = 0
EIF_LAYER_CONV2D = 1
EIF_LAYER_RELU = 2
EIF_LAYER_SOFTMAX = 3
EIF_LAYER_MAXPOOL2D = 4
EIF_LAYER_FLATTEN = 5
EIF_LAYER_DEPTHWISE_CONV2D = 6
EIF_LAYER_AVGPOOL2D = 7
EIF_LAYER_ADD = 8
# 9 was RESHAPE in old, now unused or different
EIF_LAYER_SIGMOID = 10
EIF_LAYER_TANH = 11
EIF_LAYER_GLOBAL_AVGPOOL2D = 12
# 13 was GRU
EIF_LAYER_MULTIPLY = 14
EIF_LAYER_RESHAPE = 15
# 16 was EMBEDDING
EIF_LAYER_LEAKY_RELU = 17
# 18 LAYER_NORM
# 19 BATCH_NORM
# 20 TRANSPOSE_CONV
EIF_LAYER_CONV1D = 21
EIF_LAYER_TRANSPOSE_CONV2D = 22
# 23 CLIP
# 24 RESIZE
EIF_LAYER_RESIZE = 25
EIF_LAYER_RELU6 = 30

# Op Code Mapping
OP_MAPPING = {
    'FULLY_CONNECTED': EIF_LAYER_DENSE,
    'CONV_2D': EIF_LAYER_CONV2D,
    'DEPTHWISE_CONV_2D': EIF_LAYER_DEPTHWISE_CONV2D,
    'MAX_POOL_2D': EIF_LAYER_MAXPOOL2D,
    'AVERAGE_POOL_2D': EIF_LAYER_AVGPOOL2D,
    'SOFTMAX': EIF_LAYER_SOFTMAX,
    'RELU': EIF_LAYER_RELU,
    'RELU6': EIF_LAYER_RELU6,
    'ADD': EIF_LAYER_ADD,
    'MUL': EIF_LAYER_MULTIPLY,
    'RESHAPE': EIF_LAYER_RESHAPE,
    'LOGISTIC': EIF_LAYER_SIGMOID,
    'TANH': EIF_LAYER_TANH,
    'TRANSPOSE_CONV': EIF_LAYER_TRANSPOSE_CONV2D,
    'MEAN': EIF_LAYER_GLOBAL_AVGPOOL2D, 
    'LEAKY_RELU': EIF_LAYER_LEAKY_RELU,
    'RESIZE_BILINEAR': EIF_LAYER_RESIZE,
    'RESIZE_NEAREST_NEIGHBOR': EIF_LAYER_RESIZE,
    'QUANTIZE': 23, # EIF_LAYER_QUANTIZE
    'DEQUANTIZE': 24, # EIF_LAYER_DEQUANTIZE
}

def get_op_code_str(model, opcode_idx):
    op_codes = model.OperatorCodes(opcode_idx)
    builtin_code = op_codes.BuiltinCode()
    # Map builtin code to string (this is tricky without the enum map, assuming tflite package has it)
    # Actually, we can just use the integer if we had the mapping.
    # For now, let's rely on tflite.BuiltinOperator enum names if available
    import tflite.BuiltinOperator as BO
    for name, value in BO.__dict__.items():
        if value == builtin_code:
            return name
    return "UNKNOWN"

def quantize_multiplier(double_multiplier):
    if double_multiplier == 0.:
        return 0, 0
    
    q = np.frexp(double_multiplier)
    significand = q[0]
    exponent = q[1]
    
    # significand is in [0.5, 1.0)
    # we want it in [0.5, 1.0) mapped to int32
    # TFLite uses a specific format where significand is doubled effectively?
    # Let's follow gemmlowp/TFLite standard:
    # significand_q31 = round(significand * 2^31)
    
    significand_q31 = int(round(significand * (1 << 31)))
    
    # If significand is exactly 0.5, frexp returns 0.5. 0.5 * 2^31 = 2^30.
    # If significand is almost 1.0, it maps to almost 2^31.
    # If it is 1.0 (not possible for frexp result < 1), it would overflow.
    
    if significand_q31 == (1 << 31):
        significand_q31 /= 2
        exponent += 1
        
    return significand_q31, exponent

def get_quantization_params(model, subgraph, op):
    # Default values
    input_offset = 0
    output_offset = 0
    output_multiplier = 0
    output_shift = 0
    quantized_activation_min = -128
    quantized_activation_max = 127
    
    inputs = op.InputsAsNumpy()
    outputs = op.OutputsAsNumpy()
    
    if len(inputs) > 0:
        in_tensor = subgraph.Tensors(inputs[0])
        in_quant = in_tensor.Quantization()
        if in_quant and in_quant.ZeroPointLength() > 0:
            input_offset = -in_quant.ZeroPoint(0) # Input offset is -ZeroPoint
            
    if len(outputs) > 0:
        out_tensor = subgraph.Tensors(outputs[0])
        out_quant = out_tensor.Quantization()
        if out_quant and out_quant.ZeroPointLength() > 0:
            output_offset = out_quant.ZeroPoint(0)
            
        # Calculate Multiplier
        # Real Multiplier = (InputScale * WeightScale) / OutputScale
        if len(inputs) > 1: # Has weights
            w_tensor = subgraph.Tensors(inputs[1])
            w_quant = w_tensor.Quantization()
            
            if in_quant and in_quant.ScaleLength() > 0 and \
               out_quant and out_quant.ScaleLength() > 0 and \
               w_quant and w_quant.ScaleLength() > 0:
                   
                in_scale = in_quant.Scale(0)
                out_scale = out_quant.Scale(0)
                w_scale = w_quant.Scale(0) # Assuming per-tensor quantization for now
                
                real_multiplier = (in_scale * w_scale) / out_scale
                output_multiplier, output_shift = quantize_multiplier(real_multiplier)
    
    # Pack into bytes (6 * 4 = 24 bytes)
    return struct.pack('iiiiii', 
                       int(input_offset), 
                       int(output_offset), 
                       int(output_multiplier), 
                       int(output_shift), 
                       int(quantized_activation_min), 
                       int(quantized_activation_max))

def serialize_model(tflite_path, output_path):
    with open(tflite_path, 'rb') as f:
        buf = f.read()
    
    model = Model.GetRootAsModel(buf, 0)
    subgraph = model.Subgraphs(0)
    
    tensors = []
    nodes = []
    weights_blob = bytearray()
    
    # Process Tensors
    for i in range(subgraph.TensorsLength()):
        t = subgraph.Tensors(i)
        shape = t.ShapeAsNumpy()
        dims = [1, 1, 1, 1]
        if len(shape) > 0:
            for j in range(min(len(shape), 4)):
                dims[j] = int(shape[j])
        
        # Type
        etype = EIF_TENSOR_FLOAT32
        if t.Type() == tflite.TensorType.FLOAT32: etype = EIF_TENSOR_FLOAT32
        elif t.Type() == tflite.TensorType.INT8: etype = EIF_TENSOR_INT8
        elif t.Type() == tflite.TensorType.UINT8: etype = EIF_TENSOR_UINT8
        elif t.Type() == tflite.TensorType.INT32: etype = EIF_TENSOR_INT32
        
        # Data
        buffer_idx = t.Buffer()
        data_offset = 0xFFFFFFFF
        is_variable = 1
        
        if buffer_idx > 0:
            buff = model.Buffers(buffer_idx)
            data = buff.DataAsNumpy()
            if data is not None and len(data) > 0:
                is_variable = 0
                data_offset = len(weights_blob)
                weights_blob.extend(data.tobytes())
        
        size_bytes = 1
        for d in dims: size_bytes *= d
        if etype == EIF_TENSOR_FLOAT32: size_bytes *= 4
        elif etype == EIF_TENSOR_INT32: size_bytes *= 4
        
        tensors.append({
            'type': etype,
            'dims': dims,
            'size_bytes': size_bytes,
            'is_variable': is_variable,
            'data_offset': data_offset
        })
        
    # Process Nodes
    for i in range(subgraph.OperatorsLength()):
        op = subgraph.Operators(i)
        opcode_idx = op.OpcodeIndex()
        op_name = get_op_code_str(model, opcode_idx)
        
        eif_type = OP_MAPPING.get(op_name, EIF_LAYER_UNKNOWN)
        if eif_type == EIF_LAYER_UNKNOWN:
            print(f"Warning: Unsupported operator {op_name}")
            
        inputs = op.InputsAsNumpy()
        outputs = op.OutputsAsNumpy()
        
        # Extract Params (Simplified)
        params_bytes = bytearray()
        
        # Prepend Quantization Params (Always 24 bytes)
        quant_bytes = get_quantization_params(model, subgraph, op)
        params_bytes.extend(quant_bytes)
        
        if eif_type == EIF_LAYER_CONV2D:
            opts = op.BuiltinOptions()
            import tflite.Conv2DOptions as C2D
            c2d = C2D.Conv2DOptions()
            c2d.Init(opts.Bytes, opts.Pos)
            
            # Padding
            pad_h = 0
            pad_w = 0
            if c2d.Padding() == tflite.Padding.SAME:
                pad_h = 1 # Approximation
                pad_w = 1
            
            # Get kernel size from weights
            k_h = 1
            k_w = 1
            if len(inputs) > 1:
                w_tensor = subgraph.Tensors(inputs[1])
                w_shape = w_tensor.ShapeAsNumpy()
                # [out, h, w, in]
                k_h = w_shape[1]
                k_w = w_shape[2]
            
            filters = 0
            if len(outputs) > 0:
                out_tensor = subgraph.Tensors(outputs[0])
                out_shape = out_tensor.ShapeAsNumpy()
                filters = out_shape[3] # [N, H, W, C]
            
            # struct { uint16_t filters; uint8_t kernel_h, kernel_w; uint8_t stride_h, stride_w; uint8_t pad_h, pad_w; }
            params_bytes.extend(struct.pack('HBBBBBB', 
                                            filters, 
                                            k_h, k_w, 
                                            c2d.StrideH(), c2d.StrideW(), 
                                            pad_h, pad_w))
                                            
        elif eif_type == EIF_LAYER_DENSE:
            # struct { uint16_t units; }
            units = 0
            if len(outputs) > 0:
                out_tensor = subgraph.Tensors(outputs[0])
                out_shape = out_tensor.ShapeAsNumpy()
                units = out_shape[1]
            params_bytes.extend(struct.pack('H', units))
            
        elif eif_type == EIF_LAYER_QUANTIZE:
            # struct { float scale; int32_t zero_point, min_val, max_val; }
            scale = 1.0
            zp = 0
            if len(outputs) > 0:
                out_tensor = subgraph.Tensors(outputs[0])
                quant = out_tensor.Quantization()
                if quant and quant.ScaleLength() > 0:
                    scale = quant.Scale(0)
                    zp = quant.ZeroPoint(0)
            
            params_bytes.extend(struct.pack('fiii', scale, zp, -128, 127))
            
        elif eif_type == EIF_LAYER_DEQUANTIZE:
            # struct { float scale; int32_t zero_point; }
            scale = 1.0
            zp = 0
            if len(inputs) > 0:
                in_tensor = subgraph.Tensors(inputs[0])
                quant = in_tensor.Quantization()
                if quant and quant.ScaleLength() > 0:
                    scale = quant.Scale(0)
                    zp = quant.ZeroPoint(0)
            
            params_bytes.extend(struct.pack('fi', scale, zp))
            
        # ... Add other param extractions ...
        
        nodes.append({
            'type': eif_type,
            'inputs': inputs,
            'outputs': outputs,
            'params': params_bytes
        })
        
    # Graph Inputs/Outputs
    graph_inputs = subgraph.InputsAsNumpy()
    graph_outputs = subgraph.OutputsAsNumpy()
    
    # Write File
    with open(output_path, 'wb') as f:
        # Header
        f.write(struct.pack('I', EIF_MAGIC))
        f.write(struct.pack('I', EIF_VERSION))
        f.write(struct.pack('I', len(tensors)))
        f.write(struct.pack('I', len(nodes)))
        f.write(struct.pack('I', len(graph_inputs)))
        f.write(struct.pack('I', len(graph_outputs)))
        f.write(struct.pack('I', len(weights_blob)))
        
        # Tensors
        for t in tensors:
            f.write(struct.pack('I', t['type']))
            f.write(struct.pack('iiii', *t['dims']))
            f.write(struct.pack('I', t['size_bytes']))
            f.write(struct.pack('I', t['is_variable']))
            f.write(struct.pack('I', t['data_offset']))
            
        # Nodes
        for n in nodes:
            f.write(struct.pack('I', n['type']))
            f.write(struct.pack('I', len(n['inputs'])))
            f.write(struct.pack('I', len(n['outputs'])))
            f.write(struct.pack('I', len(n['params'])))
            
            for idx in n['inputs']: f.write(struct.pack('i', idx))
            for idx in n['outputs']: f.write(struct.pack('i', idx))
            if len(n['params']) > 0:
                f.write(n['params'])
                
        # Graph I/O
        for idx in graph_inputs: f.write(struct.pack('i', idx))
        for idx in graph_outputs: f.write(struct.pack('i', idx))
        
        # Weights
        f.write(weights_blob)
        
    print(f"Successfully converted {tflite_path} to {output_path}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert TFLite model to EIF binary format')
    parser.add_argument('input', help='Input .tflite file')
    parser.add_argument('output', help='Output .eif file')
    args = parser.parse_args()
    
    serialize_model(args.input, args.output)
