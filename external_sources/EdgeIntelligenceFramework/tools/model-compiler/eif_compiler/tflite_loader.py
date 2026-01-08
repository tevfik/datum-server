import numpy as np
try:
    import tflite
except ImportError:
    print("Error: tflite package not found. Install with `pip install tflite`")
    tflite = None

class EIFLayer:
    def __init__(self, type_name, params=None, weights=None, biases=None, input_shape=None, output_shape=None):
        self.type_name = type_name
        self.params = params or {}
        self.weights = weights
        self.biases = biases
        self.input_shape = input_shape or []
        self.output_shape = output_shape or []

def load_tflite_model(model_path):
    if tflite is None:
        raise ImportError("tflite package is missing")

    with open(model_path, 'rb') as f:
        buf = f.read()
        
    model = tflite.Model.GetRootAsModel(buf, 0)
    subgraph = model.Subgraphs(0)
    
    layers = []
    
    for i in range(subgraph.OperatorsLength()):
        op = subgraph.Operators(i)
        opcode_index = op.OpcodeIndex()
        opcode = model.OperatorCodes(opcode_index).BuiltinCode()
        
        # Import BuiltinOptions dynamically to avoid top-level import errors if package is missing
        from tflite.BuiltinOperator import BuiltinOperator
        from tflite.Conv2DOptions import Conv2DOptions
        from tflite.DepthwiseConv2DOptions import DepthwiseConv2DOptions
        from tflite.Pool2DOptions import Pool2DOptions
        from tflite.FullyConnectedOptions import FullyConnectedOptions
        from tflite.AddOptions import AddOptions
        from tflite.ConcatenationOptions import ConcatenationOptions
        
        from tflite.MulOptions import MulOptions
        from tflite.ReshapeOptions import ReshapeOptions
        from tflite.LeakyReluOptions import LeakyReluOptions
        # Clip is usually MINIMUM/MAXIMUM or RELU6, but TFLite has no direct CLIP op in standard schema sometimes?
        # Actually it does, but let's check if we need to import it.
        # Wait, BuiltinOperator.MEAN is for GlobalAvgPool usually.
        from tflite.ReducerOptions import ReducerOptions
        
        if opcode == BuiltinOperator.CONV_2D:
            # ... (existing Conv2D logic) ...
            opts = op.BuiltinOptions()
            conv_opts = Conv2DOptions()
            conv_opts.Init(opts.Bytes, opts.Pos)
            
            stride_h = conv_opts.StrideH()
            stride_w = conv_opts.StrideW()
            
            weight_tensor_index = op.Inputs(1)
            bias_tensor_index = op.Inputs(2)
            
            weight_tensor = subgraph.Tensors(weight_tensor_index)
            bias_tensor = subgraph.Tensors(bias_tensor_index)
            
            weight_buffer = model.Buffers(weight_tensor.Buffer())
            bias_buffer = model.Buffers(bias_tensor.Buffer())
            
            weights = np.frombuffer(weight_buffer.DataAsNumpy(), dtype=np.float32)
            biases = np.frombuffer(bias_buffer.DataAsNumpy(), dtype=np.float32)
            
            shape = [weight_tensor.Shape(j) for j in range(weight_tensor.ShapeLength())]
            weights = weights.reshape(shape)
            
            layers.append(EIFLayer(
                "CONV2D",
                params={
                    "filters": shape[0],
                    "kernel_h": shape[1],
                    "kernel_w": shape[2],
                    "stride_h": stride_h,
                    "stride_w": stride_w,
                    "pad_h": 0,
                    "pad_w": 0
                },
                weights=weights,
                biases=biases
            ))

        elif opcode == BuiltinOperator.DEPTHWISE_CONV_2D:
            # ... (existing Depthwise logic) ...
            opts = op.BuiltinOptions()
            dw_opts = DepthwiseConv2DOptions()
            dw_opts.Init(opts.Bytes, opts.Pos)
            
            stride_h = dw_opts.StrideH()
            stride_w = dw_opts.StrideW()
            depth_multiplier = dw_opts.DepthMultiplier()
            
            weight_tensor_index = op.Inputs(1)
            bias_tensor_index = op.Inputs(2)
            
            weight_tensor = subgraph.Tensors(weight_tensor_index)
            bias_tensor = subgraph.Tensors(bias_tensor_index)
            
            weight_buffer = model.Buffers(weight_tensor.Buffer())
            bias_buffer = model.Buffers(bias_tensor.Buffer())
            
            weights = np.frombuffer(weight_buffer.DataAsNumpy(), dtype=np.float32)
            biases = np.frombuffer(bias_buffer.DataAsNumpy(), dtype=np.float32)
            
            shape = [weight_tensor.Shape(j) for j in range(weight_tensor.ShapeLength())]
            weights = weights.reshape(shape)
            
            layers.append(EIFLayer(
                "DEPTHWISE_CONV2D",
                params={
                    "kernel_h": shape[1],
                    "kernel_w": shape[2],
                    "stride_h": stride_h,
                    "stride_w": stride_w,
                    "pad_h": 0,
                    "pad_w": 0,
                    "depth_multiplier": depth_multiplier
                },
                weights=weights,
                biases=biases
            ))

        elif opcode == BuiltinOperator.MAX_POOL_2D:
            # ... (existing MaxPool logic) ...
            opts = op.BuiltinOptions()
            pool_opts = Pool2DOptions()
            pool_opts.Init(opts.Bytes, opts.Pos)
            
            layers.append(EIFLayer(
                "MAXPOOL2D",
                params={
                    "pool_h": pool_opts.FilterHeight(),
                    "pool_w": pool_opts.FilterWidth(),
                    "stride_h": pool_opts.StrideH(),
                    "stride_w": pool_opts.StrideW()
                }
            ))

        elif opcode == BuiltinOperator.AVERAGE_POOL_2D:
            # ... (existing AvgPool logic) ...
            opts = op.BuiltinOptions()
            pool_opts = Pool2DOptions()
            pool_opts.Init(opts.Bytes, opts.Pos)
            
            layers.append(EIFLayer(
                "AVGPOOL2D",
                params={
                    "pool_h": pool_opts.FilterHeight(),
                    "pool_w": pool_opts.FilterWidth(),
                    "stride_h": pool_opts.StrideH(),
                    "stride_w": pool_opts.StrideW()
                }
            ))

        elif opcode == BuiltinOperator.RESHAPE:
            # Enhanced Reshape logic
            opts = op.BuiltinOptions()
            reshape_opts = ReshapeOptions()
            reshape_opts.Init(opts.Bytes, opts.Pos)
            
            # Target shape is usually in the second input tensor OR in the options
            # TFLite prefers options for constant shape
            new_shape = reshape_opts.NewShapeAsNumpy()
            # If new_shape is empty, check input tensor 1
            if new_shape.size == 0 and op.InputsLength() > 1:
                 shape_tensor_index = op.Inputs(1)
                 shape_tensor = subgraph.Tensors(shape_tensor_index)
                 shape_buffer = model.Buffers(shape_tensor.Buffer())
                 new_shape = np.frombuffer(shape_buffer.DataAsNumpy(), dtype=np.int32)

            layers.append(EIFLayer("RESHAPE", params={"target_shape": new_shape.tolist()}))

        elif opcode == BuiltinOperator.FULLY_CONNECTED:
            # ... (existing Dense logic) ...
            weight_tensor_index = op.Inputs(1)
            bias_tensor_index = op.Inputs(2)
            
            weight_tensor = subgraph.Tensors(weight_tensor_index)
            bias_tensor = subgraph.Tensors(bias_tensor_index)
            
            weight_buffer = model.Buffers(weight_tensor.Buffer())
            bias_buffer = model.Buffers(bias_tensor.Buffer())
            
            weights = np.frombuffer(weight_buffer.DataAsNumpy(), dtype=np.float32)
            biases = np.frombuffer(bias_buffer.DataAsNumpy(), dtype=np.float32)
            
            shape = [weight_tensor.Shape(j) for j in range(weight_tensor.ShapeLength())]
            weights = weights.reshape(shape)
            
            layers.append(EIFLayer(
                "DENSE",
                params={"units": shape[0]},
                weights=weights,
                biases=biases
            ))

        elif opcode == BuiltinOperator.SOFTMAX:
            layers.append(EIFLayer("SOFTMAX"))

        elif opcode == BuiltinOperator.LOGISTIC:
            layers.append(EIFLayer("SIGMOID"))

        elif opcode == BuiltinOperator.TANH:
            layers.append(EIFLayer("TANH"))

        elif opcode == BuiltinOperator.ADD:
            layers.append(EIFLayer("ADD"))

        elif opcode == BuiltinOperator.CONCATENATION:
            opts = op.BuiltinOptions()
            concat_opts = ConcatenationOptions()
            concat_opts.Init(opts.Bytes, opts.Pos)
            layers.append(EIFLayer("CONCAT", params={"axis": concat_opts.Axis()}))

        elif opcode == BuiltinOperator.MUL:
            layers.append(EIFLayer("MULTIPLY"))

        elif opcode == BuiltinOperator.MEAN:
            # Usually GlobalAvgPool if reducing H and W
            opts = op.BuiltinOptions()
            reduce_opts = ReducerOptions()
            reduce_opts.Init(opts.Bytes, opts.Pos)
            keep_dims = reduce_opts.KeepDims()
            
            # Check axes
            axis_tensor_index = op.Inputs(1)
            axis_tensor = subgraph.Tensors(axis_tensor_index)
            axis_buffer = model.Buffers(axis_tensor.Buffer())
            axes = np.frombuffer(axis_buffer.DataAsNumpy(), dtype=np.int32)
            
            # Heuristic: If reducing axes 1 and 2 (H and W), it's GlobalAvgPool
            if 1 in axes and 2 in axes:
                layers.append(EIFLayer("GLOBAL_AVGPOOL2D"))
            else:
                print(f"Warning: MEAN op on axes {axes} not fully supported, mapping to GlobalAvgPool as fallback")
                layers.append(EIFLayer("GLOBAL_AVGPOOL2D"))

        elif opcode == BuiltinOperator.LEAKY_RELU:
            opts = op.BuiltinOptions()
            leaky_opts = LeakyReluOptions()
            leaky_opts.Init(opts.Bytes, opts.Pos)
            alpha = leaky_opts.Alpha()
            layers.append(EIFLayer("LEAKY_RELU", params={"alpha": alpha}))
            
        # Note: LayerNorm is often a composite. If we see it as a custom op or specific pattern, handle it.
        # For now, we skip explicit LayerNorm parsing unless it's a custom op, or we assume the user uses a specific TFLite converter that emits it?
        # Standard TFLite doesn't have a single LAYER_NORM op code in the main enum usually, it uses Mean/Sub/Pow/etc.
        # But if we are building a custom runtime, we might want to support it if it appears.
        # Let's assume for now we might encounter it or just leave it for future complex graph matching.

            
    return layers
