
import onnx
from onnx import helper, TensorProto
import numpy as np

def create_model_direct():
    print("Creating ONNX model directly...")

    # Input: [Batch=1, Channels=1, Height=28, Width=28]
    input_tensor = helper.make_tensor_value_info('input', TensorProto.FLOAT, [1, 1, 28, 28])
    output_tensor = helper.make_tensor_value_info('output', TensorProto.FLOAT, [1, 10])

    # Weights for Conv1: 4 filters, 1 input channel, 3x3 kernel
    # Shape: [4, 1, 3, 3]
    conv1_w = np.random.randn(4, 1, 3, 3).astype(np.float32)
    conv1_w_init = helper.make_tensor('conv1_w', TensorProto.FLOAT, [4, 1, 3, 3], conv1_w.flatten())
    conv1_b = np.zeros(4).astype(np.float32)
    conv1_b_init = helper.make_tensor('conv1_b', TensorProto.FLOAT, [4], conv1_b.flatten())

    # Conv1 Node
    # Output: [1, 4, 28, 28] (Padding=1, Stride=1)
    conv1 = helper.make_node(
        'Conv', ['input', 'conv1_w', 'conv1_b'], ['conv1_out'],
        kernel_shape=[3, 3], pads=[1, 1, 1, 1], strides=[1, 1]
    )

    # ReLU
    relu1 = helper.make_node('Relu', ['conv1_out'], ['relu1_out'])

    # MaxPool: 2x2
    # Output: [1, 4, 14, 14]
    maxpool1 = helper.make_node(
        'MaxPool', ['relu1_out'], ['pool1_out'],
        kernel_shape=[2, 2], strands=[2, 2] # Typo in 'strands'? It's strides.
    )
    # Fix strides arg
    maxpool1 = helper.make_node(
        'MaxPool', ['relu1_out'], ['pool1_out'],
        kernel_shape=[2, 2], strides=[2, 2]
    )

    # Reshape for LSTM
    # We want [Batch, Seq, InputSize]
    # Current: [1, 4, 14, 14] -> Permute to [1, 14, 4, 14]?
    # Let's Flatten spatial: [1, 4, 196]
    # Transpose to [1, 196, 4] (Seq=196, In=4)
    # Or as per original plan: [1, 14, 56] (Seq=14, In=56)
    
    # 1. Transpose to [1, 14, 4, 14]
    transpose1 = helper.make_node(
        'Transpose', ['pool1_out'], ['trans1_out'],
        perm=[0, 2, 1, 3] # [B, H, C, W]
    )
    
    # 2. Reshape to [1, 14, 56]
    shape_const = helper.make_tensor('shape_const', TensorProto.INT64, [3], [1, 14, 56])
    reshape1 = helper.make_node('Reshape', ['trans1_out', 'shape_const'], ['lstm_in'])

    # LSTM
    # Input: [1, 14, 56]
    # Hidden: 16
    # Weights: 
    # W [num_directions, 4*hidden, input] -> [1, 64, 56]
    # R [num_directions, 4*hidden, hidden] -> [1, 64, 16]
    # B [num_directions, 8*hidden] -> [1, 128]
    
    hidden_size = 16
    input_size = 56
    
    lstm_w = np.random.randn(1, 4*hidden_size, input_size).astype(np.float32)
    lstm_r = np.random.randn(1, 4*hidden_size, hidden_size).astype(np.float32)
    lstm_b = np.zeros((1, 8*hidden_size)).astype(np.float32)
    
    lstm_w_init = helper.make_tensor('lstm_w', TensorProto.FLOAT, [1, 4*hidden_size, input_size], lstm_w.flatten())
    lstm_r_init = helper.make_tensor('lstm_r', TensorProto.FLOAT, [1, 4*hidden_size, hidden_size], lstm_r.flatten())
    lstm_b_init = helper.make_tensor('lstm_b', TensorProto.FLOAT, [1, 8*hidden_size], lstm_b.flatten())
    
    # LSTM Node
    # Outputs: Y [1, 14, 1, 16], Y_h [1, 1, 16]
    lstm1 = helper.make_node(
        'LSTM', ['lstm_in', 'lstm_w', 'lstm_r', 'lstm_b'], ['lstm_out', 'lstm_h'],
        hidden_size=hidden_size, direction='forward'
    )
    
    # Squeeze Y_h to [1, 16] for Dense
    # Axes 1 (num_dir)
    # Actually Y_h is [num_dir, batch, hidden] = [1, 1, 16]
    # We want [1, 16]
    
    squeeze_axes = helper.make_tensor('squeeze_axes', TensorProto.INT64, [2], [0, 1]) # Squeeze 0 and 1? No, batch is 1 at index 1.
    # We want to keep batch. Y_h shape is [num_directions, batch_size, hidden_size]
    # [1, 1, 16] -> Squeeze dim 0 -> [1, 16].
    
    squeeze_axes_init = helper.make_tensor('squeeze_axes', TensorProto.INT64, [1], [0])
    squeeze1 = helper.make_node('Squeeze', ['lstm_h', 'squeeze_axes'], ['dense_in'])

    # Dense (Gemm)
    # Input: [1, 16]
    # Weight: [16, 10] (Wait, Gemm usually expects A*B. If A is [M, K], B is [K, N])
    # ONNX Gemm: Y = alpha*A*B + beta*C
    # Standard Dense in Torch: W is [Out, In]. so x*W_t
    # In ONNX Gemm, typically B is [In, Out] or we use transB=1.
    # Let's use standard Gemm A*B with B [16, 10].
    
    dense_w = np.random.randn(16, 10).astype(np.float32)
    dense_b = np.zeros(10).astype(np.float32)
    
    dense_w_init = helper.make_tensor('dense_w', TensorProto.FLOAT, [16, 10], dense_w.flatten())
    dense_b_init = helper.make_tensor('dense_b', TensorProto.FLOAT, [10], dense_b.flatten())
    
    gemm1 = helper.make_node(
        'Gemm', ['dense_in', 'dense_w', 'dense_b'], ['output'],
        alpha=1.0, beta=1.0
    )

    # Graph
    graph = helper.make_graph(
        [conv1, relu1, maxpool1, transpose1, reshape1, lstm1, squeeze1, gemm1],
        'hybrid_simple',
        [input_tensor],
        [output_tensor],
        [conv1_w_init, conv1_b_init, shape_const, lstm_w_init, lstm_r_init, lstm_b_init, squeeze_axes_init, dense_w_init, dense_b_init]
    )

    model = helper.make_model(graph, producer_name='onnx-direct')
    onnx.checker.check_model(model)
    onnx.save(model, "hybrid_model.onnx")
    
    print("Success: hybrid_model.onnx created directly.")
    
    # Create input data for C verification
    dummy_input = np.random.randn(1, 1, 28, 28).astype(np.float32)
    input_q15 = (dummy_input.flatten() * 1024).astype(np.int16)
    with open("input.bin", "wb") as f:
        f.write(input_q15.tobytes())
    np.save("input_data.npy", dummy_input.flatten())

if __name__ == "__main__":
    create_model_direct()
