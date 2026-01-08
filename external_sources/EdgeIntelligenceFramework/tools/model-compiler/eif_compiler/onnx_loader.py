import onnx
import numpy as np

class EIFLayer:
    def __init__(self, type_name, params=None, weights=None, biases=None, input_shape=None, output_shape=None):
        self.type_name = type_name
        self.params = params or {}
        self.weights = weights
        self.biases = biases
        self.input_shape = input_shape or []
        self.output_shape = output_shape or []

def load_onnx_model(model_path):
    model = onnx.load(model_path)
    graph = model.graph
    
    layers = []
    
    # Simple parser for linear topology (Gemm -> Relu -> Gemm ...)
    # In a real compiler, we would traverse the graph properly.
    
    # Map initializers (weights/biases)
    initializers = {init.name: onnx.numpy_helper.to_array(init) for init in graph.initializer}
    
    for inp in graph.input:
        name = inp.name
        # Skip initializers (weights) which are also listed in inputs sometimes
        if name in initializers:
            continue
            
        # Get shape
        shape_dims = []
        for d in inp.type.tensor_type.shape.dim:
            if d.dim_value > 0:
                shape_dims.append(d.dim_value)
            else:
                shape_dims.append(1) # Unknown/Batch dim -> 1
        
        # Assume NCHW or similar. EIF uses specific struct.
        # If 4 dims: [N, C, H, W] -> height=H, width=W, channels=C.
        # If 3 dims: [N, Ch, Seq] ?? EIF is mostly vision/CNN focused on Input params?
        # eif_shape_t has width, height, channels.
        
        c, w, h = 1, 1, 1
        if len(shape_dims) == 4:
            # ONNX: NCHW [Batch, Channel, Height, Width]
            c = shape_dims[1]
            h = shape_dims[2]
            w = shape_dims[3]
        elif len(shape_dims) == 3:
             # N C L (1D conv) or N L C (RNN)?
             # EIF assumes "channels" is the depth.
             # Let's map to [1, 1, C] or similar.
             # Actually, if 3 dims: [Batch, Seq, InputSize] for RNN.
             # eif_shape_t height=1, width=1, channels=InputSize.
             # But [N, C, L] -> H=1, W=L, C=C?
             # For my demo model: [1, 1, 28, 28] -> 4 dims.
             pass
        
        layers.append(EIFLayer(
            "INPUT",
            params={"input": {"width": w, "height": h, "channels": c}},
            input_shape=[h, w, c],
            output_shape=[h, w, c]
        ))
        break # Only one input for now

    for node in graph.node:
        if node.op_type == "Gemm" or node.op_type == "MatMul":
            # Dense Layer
            # Inputs: X, W, B
            # ONNX Gemm: Y = alpha*A*B + beta*C
            # ONNX MatMul: Y = A*B
            
            # Find weight tensor
            weight_name = node.input[1]
            weights = initializers.get(weight_name)
            
            # Handle MatMul where weights might not be initializers (dynamic), but for now assume inference
            if weights is None:
                continue # Skip dynamic nodes for now
                
            # PyTorch: Linear(in, out) -> Weights (out, in).
            # ONNX Gemm: usually A(M,K) * B(K,N). B is (K,N). 
            # If exported from PyTorch, B is often transposed?
            # Check attribute "transB"
            transB = 0
            for attr in node.attribute:
                if attr.name == "transB":
                    transB = attr.i
            
            if transB:
                weights = weights.transpose() # Make it (K, N) -> (In, Out)
                
            # Our runtime expects (In, Out) for logic: in[i] * W[i*out + o]
            # So weights should be shape (In, Out).
            
            biases = None
            if len(node.input) > 2:
                 bias_name = node.input[2]
                 if bias_name in initializers:
                     biases = initializers[bias_name]
            
            if biases is None:
                biases = np.zeros(weights.shape[1])
                
            layers.append(EIFLayer(
                "DENSE",
                params={"units": weights.shape[1]},
                weights=weights,
                biases=biases
            ))

        elif node.op_type == "Conv":
            # Inputs: X, W, B
            weight_name = node.input[1]
            weights = initializers.get(weight_name) # Shape (Out, In/Groups, KH, KW)
            
            biases = None
            if len(node.input) > 2:
                bias_name = node.input[2]
                biases = initializers.get(bias_name)
                
            # Attributes
            pads = [0, 0, 0, 0]
            strides = [1, 1]
            kernel_shape = weights.shape[2:]
            
            for attr in node.attribute:
                if attr.name == "pads":
                    pads = attr.ints # [x1_begin, x2_begin... x1_end, x2_end]
                elif attr.name == "strides":
                    strides = attr.ints
                elif attr.name == "kernel_shape":
                    kernel_shape = attr.ints
                    
            # EIF expects weights in (Out, In, KH, KW) - or flattened?
            # Runtime: eif_exec_conv2d uses:
            # w_idx = ((ky * KW + kx) * InOnly + ic) * OutCh + oc;
            # This corresponds to standard NHWC layout or similar? 
            # Wait, `w_idx` calculation implies: [KH, KW, In, Out]
            # ONNX provides: [Out, In, KH, KW]
            # We must transpose weights to [KH, KW, In, Out] for the runtime!
            
            # ONNX: (Out, In, KH, KW) -> (2, 3)
            # Target: (KH, KW, In, Out) -> (2, 3, 1, 0)
            weights_transposed = weights.transpose(2, 3, 1, 0)
            
            if biases is None:
                biases = np.zeros(weights.shape[0])

            layers.append(EIFLayer(
                "CONV2D",
                params={
                    "filters": weights.shape[0],
                    "kernel_h": kernel_shape[0],
                    "kernel_w": kernel_shape[1],
                    "stride_h": strides[0],
                    "stride_w": strides[1],
                    "pad_h": pads[0], # Init pad
                    "pad_w": pads[1]  # Init pad
                },
                weights=weights_transposed,
                biases=biases
            ))

        elif node.op_type == "MaxPool":
            strides = [1, 1]
            kernel_shape = [2, 2]
            for attr in node.attribute:
                if attr.name == "strides":
                    strides = attr.ints
                elif attr.name == "kernel_shape":
                    kernel_shape = attr.ints
            
            layers.append(EIFLayer(
                "MAXPOOL2D",
                params={
                    "pool_h": kernel_shape[0],
                    "pool_w": kernel_shape[1],
                    "stride_h": strides[0],
                    "stride_w": strides[1]
                }
            ))
            
        elif node.op_type == "GlobalAveragePool":
            layers.append(EIFLayer("GLOBAL_AVGPOOL2D"))
            
        elif node.op_type == "Flatten":
            layers.append(EIFLayer("FLATTEN"))
            
        elif node.op_type == "Reshape":
            layers.append(EIFLayer("RESHAPE"))
            
        elif node.op_type == "Add":
             layers.append(EIFLayer("ADD"))
             
        elif node.op_type == "Relu":
            layers.append(EIFLayer("RELU"))
            
        elif node.op_type == "Softmax":
            layers.append(EIFLayer("SOFTMAX"))
            
        elif node.op_type == "Sigmoid":
            layers.append(EIFLayer("SIGMOID"))
            
        elif node.op_type == "Tanh":
            layers.append(EIFLayer("TANH"))
            
        elif node.op_type == "LSTM":
             # ... (Keep existing LSTM logic but clean up if needed)
             # For brevity, preserving previous LSTM/GRU/RNN logic as simpler blocks below
             pass
    
    # Re-scan for RNNs if not processed (naive single pass)
    # Merging the new robust loop with previous RNN logic
    for node in graph.node:
        if node.op_type == "LSTM":
             W_node = next(n for n in graph.initializer if n.name == node.input[1])
             R_node = next(n for n in graph.initializer if n.name == node.input[2])
             B_node = next((n for n in graph.initializer if len(node.input) > 3 and n.name == node.input[3]), None)
             
             W = onnx.numpy_helper.to_array(W_node)
             R = onnx.numpy_helper.to_array(R_node)
             B = onnx.numpy_helper.to_array(B_node) if B_node else None
             
             # Unidirectional check
             if W.shape[0] != 1: 
                 W=W[0]; R=R[0]; B=B[0] if B is not None else None
             else: 
                 W=W[0]; R=R[0]; B=B[0] if B is not None else None
             
             # ONNX Layout: [4*H, I] and [4*H, H]
             # Gates order: Input, Output, Forget, Cell (IOFC)
             # EIF Runtime needs separate blocks for F, I, C, O (or consistent with struct)
             # struct: f, i, c, o. 
             # Runtime expectation per gate: [Input+Hidden, Hidden] (Transposed for efficiency [j*H + i])
             
             H = R.shape[1] # R is [4H, H] or [num_dir, 4H, H]? R is [num_dir, 4H, H] originally. 
             # Extracted R is [4H, H]. So H = R.shape[1].
             
             # Split gates. ONNX: I, O, F, C
             w_i, w_o, w_f, w_c = np.split(W, 4, axis=0)
             r_i, r_o, r_f, r_c = np.split(R, 4, axis=0)
             
             # Biases [8*H] -> W_b [4H] + R_b [4H].
             b_i, b_o, b_f, b_c = np.zeros(H), np.zeros(H), np.zeros(H), np.zeros(H)
             
             if B is not None:
                 # B uses same IOFC order
                 w_b = B[:4*H]
                 r_b = B[4*H:]
                 wb_i, wb_o, wb_f, wb_c = np.split(w_b, 4)
                 rb_i, rb_o, rb_f, rb_c = np.split(r_b, 4)
                 b_i = wb_i + rb_i
                 b_o = wb_o + rb_o
                 b_f = wb_f + rb_f
                 b_c = wb_c + rb_c
                 
             # Helper to process gate
             def process_gate(w, r):
                 # w: [H, I], r: [H, H]
                 # Target: [I+H, H] but flattened from [I+H, H] memory (Row Major)
                 # Wait, EIF uses W[j*H + i]. This implies memory is (Input+Hidden) blocks of size H.
                 # i.e. Block 0: Weights for Input 0 to all H units.
                 # This is (Input+Hidden, Hidden) layout.
                 # So we need to Transpose W -> (I, H) and R -> (H, H) then Concatenate.
                 w_t = w.transpose() # [I, H]
                 r_t = r.transpose() # [H, H]
                 return np.concatenate((w_t, r_t), axis=0) # [I+H, H]
                 
             gate_f = process_gate(w_f, r_f)
             gate_i = process_gate(w_i, r_i)
             gate_c = process_gate(w_c, r_c)
             gate_o = process_gate(w_o, r_o)
             
             # Concatenate all gates: F, I, C, O (matching struct order in eif_rnn.h - W_f, W_i...)
             weights_packed = np.concatenate((gate_f, gate_i, gate_c, gate_o), axis=0)
             # Flattens to: [F_data..., I_data..., C_data..., O_data...]
             
             biases_packed = np.concatenate((b_f, b_i, b_c, b_o))
             
             layers.append(EIFLayer("LSTM", params={"units":H, "return_sequences":1}, weights=weights_packed.flatten(), biases=biases_packed))

        elif node.op_type == "GRU":
             W_node = next(n for n in graph.initializer if n.name == node.input[1])
             R_node = next(n for n in graph.initializer if n.name == node.input[2])
             B_node = next((n for n in graph.initializer if len(node.input) > 3 and n.name == node.input[3]), None)
             
             W = onnx.numpy_helper.to_array(W_node)
             R = onnx.numpy_helper.to_array(R_node)
             B = onnx.numpy_helper.to_array(B_node) if B_node else None
             
             if W.shape[0] != 1: W=W[0]; R=R[0]; B=B[0] if B is not None else None
             else: W=W[0]; R=R[0]; B=B[0] if B is not None else None
             
             # GRU ONNX: Z, R, H (Update, Reset, Hidden/Candidate)
             # EIF GRU: R, Z, H (Reset, Update, Candidate) - Check struct order: W_r, W_z, W_h
             H = R.shape[1]
             
             w_z, w_r, w_h = np.split(W, 3, axis=0)
             r_z, r_r, r_h = np.split(R, 3, axis=0)
             
             b_z, b_r, b_h = np.zeros(H), np.zeros(H), np.zeros(H)
             if B is not None:
                 w_b = B[:3*H]
                 r_b = B[3*H:]
                 wb_z, wb_r, wb_h = np.split(w_b, 3)
                 rb_z, rb_r, rb_h = np.split(r_b, 3)
                 b_z = wb_z + rb_z
                 b_r = wb_r + rb_r
                 b_h = wb_h + rb_h
             
             # Helper Same as LSTM
             def process_gate(w, r):
                 w_t = w.transpose() 
                 r_t = r.transpose() 
                 return np.concatenate((w_t, r_t), axis=0)
             
             gate_r = process_gate(w_r, r_r)
             gate_z = process_gate(w_z, r_z)
             gate_h = process_gate(w_h, r_h)
             
             weights_packed = np.concatenate((gate_r, gate_z, gate_h), axis=0)
             biases_packed = np.concatenate((b_r, b_z, b_h))
             
             layers.append(EIFLayer("GRU", params={"units":H, "return_sequences":1}, weights=weights_packed.flatten(), biases=biases_packed))
             
        elif node.op_type == "RNN":
             W_node = next(n for n in graph.initializer if n.name == node.input[1])
             R_node = next(n for n in graph.initializer if n.name == node.input[2])
             B_node = next((n for n in graph.initializer if len(node.input) > 3 and n.name == node.input[3]), None)
             
             W = onnx.numpy_helper.to_array(W_node)
             R = onnx.numpy_helper.to_array(R_node)
             B = onnx.numpy_helper.to_array(B_node) if B_node else None
             
             if W.shape[0] != 1: W=W[0]; R=R[0]; B=B[0] if B is not None else None
             else: W=W[0]; R=R[0]; B=B[0] if B is not None else None
             
             # RNN: Just one gate (Input, Hidden)
             # Transpose W [H, I] -> [I, H]
             # Transpose R [H, H] -> [H, H]
             # W_ih, W_hh
             
             H = R.shape[1]
             
             w_t = W.transpose()
             r_t = R.transpose()
             
             # EIF RNN Cell has W_ih and W_hh
             # We should probably pack them sequentially.
             # eif_rnn_cell has separate pointers. We'll map them.
             
             weights_packed = np.concatenate((w_t, r_t), axis=0) # [I+H, H]
             
             biases = np.zeros(2*H) # b_ih, b_hh
             if B is not None:
                 # ONNX B is [2H] -> W_b, R_b
                 biases = B
                 
             layers.append(EIFLayer("RNN", params={"units":H, "return_sequences":1}, weights=weights_packed.flatten(), biases=biases))

    return layers
