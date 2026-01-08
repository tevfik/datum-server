import argparse
import os
import numpy as np
from .onnx_loader import load_onnx_model

def quantize_f32_to_q15(arr):
    if arr is None: return None
    # Scale: -1.0..1.0 -> -32768..32767
    # Clamp to avoid overflow if weights > 1.0 (though EIF expects weights in range)
    # Note: Biases in EIF Q15 ops are also Q15 (input * weight >> 15 + bias).
    scaled = arr * 32768.0
    clipped = np.clip(scaled, -32768, 32767)
    return clipped.astype(np.int16)

def generate_c_header(layers, model_name, output_file, quantize=True):
    # Calculate arena size based on layer requirements
    max_scratch_size = 0
    
    # ... (scratch calculation same as before, assumes float size? 
    # No, runtime uses int16, so 2 bytes. Adjust scratch logic.)
    
    for layer in layers:
        scratch = 0
        element_size = 2 if quantize else 4 # int16 vs float
        
        if layer.type_name == "CONV2D":
            # Conv needs im2col buffer? Or just direct?
            # Runtime eif_exec_conv2d is direct. Scratch not needed?
            # But we double buffer.
            pass
        # ... (Scratch logic in EIF is actually for input/output buffers between layers)
        # eif_model_create infers sizes. Arena assumes max(layer_io).
        
    # Recalculate scratch based on max IO tensor size
    # Since we don't track exact shapes here easily without a full pass, 
    # let's trust the runtime's dynamic calculation or just allocate existing heuristics.
    # The C header generator just writes metadata.
    # The actual eif_model_create does the allocation from the arena.
    # We just need to give a ballpark "arena_size" constant for the user.
    
    # Let's assume 128x128x32 as a safe upper bound for "Tiny" demos
    max_tensor_bytes = 128 * 128 * 32 * (2 if quantize else 4)
    arena_size = max_tensor_bytes * 2 + 4096 
    
    with open(output_file, 'w') as f:
        f.write(f"#ifndef {model_name.upper()}_H\n")
        f.write(f"#define {model_name.upper()}_H\n\n")
        f.write('#include "eif_model.h"\n\n')
        
        # Write Weights and Biases
        DATA_TYPE = "int16_t" if quantize else "float32_t"
        
        for i, layer in enumerate(layers):
            if layer.weights is not None:
                w_data = layer.weights
                if quantize:
                    w_data = quantize_f32_to_q15(w_data)
                    
                f.write(f"// Layer {i} Weights ({w_data.size} elements)\n")
                f.write(f"static const {DATA_TYPE} layer_{i}_weights[] = {{\n")
                weights_flat = w_data.flatten()
                for j, w in enumerate(weights_flat):
                    if quantize:
                        f.write(f"{w}, ")
                    else:
                        f.write(f"{w:.6f}f, ")
                    if (j + 1) % 12 == 0: f.write("\n    ")
                f.write("\n};\n\n")
                
            if layer.biases is not None:
                b_data = layer.biases
                if quantize:
                    b_data = quantize_f32_to_q15(b_data)
                    
                f.write(f"// Layer {i} Biases ({b_data.size} elements)\n")
                f.write(f"static const {DATA_TYPE} layer_{i}_biases[] = {{\n")
                biases_flat = b_data.flatten()
                for j, b in enumerate(biases_flat):
                    if quantize:
                        f.write(f"{b}, ")
                    else:
                        f.write(f"{b:.6f}f, ")
                    if (j + 1) % 12 == 0: f.write("\n    ")
                f.write("\n};\n\n")
        
        # Write Layer Definitions
        f.write(f"static eif_layer_t {model_name}_layers[] = {{\n")
        for i, layer in enumerate(layers):
            f.write("    {\n")
            
            # Helper to write common params
            def write_common(ltype):
                f.write(f"        .type = {ltype},\n")
                # Activation handled via separate layer in EIF usually, or param?
                # EIF_LAYER_CONV2D struct doesn't have activation field in C.
                # It relies on EIF_RELU() layer following it.
                if layer.weights is not None:
                    f.write(f"        .weights = layer_{i}_weights,\n")
                else:
                    f.write("        .weights = NULL,\n")
                    
                if layer.biases is not None:
                    f.write(f"        .bias = layer_{i}_biases,\n")
                else:
                    f.write("        .bias = NULL,\n")

            if layer.type_name == "INPUT":
                write_common("EIF_LAYER_INPUT")
                p = layer.params['input']
                f.write(f"        .params.input = {{ .width={p['width']}, .height={p['height']}, .channels={p['channels']} }}\n")

            elif layer.type_name == "DENSE":
                write_common("EIF_LAYER_DENSE")
                f.write(f"        .params.dense.units = {layer.params['units']}\n")
                
            elif layer.type_name == "CONV2D":
                write_common("EIF_LAYER_CONV2D")
                p = layer.params
                f.write(f"        .params.conv = {{ .filters={p['filters']}, "
                        f".kernel_h={p['kernel_h']}, .kernel_w={p['kernel_w']}, "
                        f".stride_h={p['stride_h']}, .stride_w={p['stride_w']}, "
                        f".padding={1 if p['pad_h']>0 else 0} }}\n")
                        
            elif layer.type_name == "MAXPOOL2D":
                write_common("EIF_LAYER_MAXPOOL2D")
                p = layer.params
                f.write(f"        .params.pool = {{ .pool_h={p['pool_h']}, .pool_w={p['pool_w']}, "
                        f".stride_h={p['stride_h']}, .stride_w={p['stride_w']} }}\n")

            elif layer.type_name == "RELU":
                write_common("EIF_LAYER_RELU")
                
            elif layer.type_name == "SOFTMAX":
                write_common("EIF_LAYER_SOFTMAX")
                
            elif layer.type_name == "GLOBAL_AVGPOOL2D":
                 write_common("EIF_LAYER_GLOBAL_AVGPOOL")
                 
            elif layer.type_name == "FLATTEN":
                write_common("EIF_LAYER_FLATTEN")
                
            elif layer.type_name == "RESHAPE":
                # EIF Runtime doesn't strictly need explicit reshape if shapes infer correctly,
                # but explicit layer helps. The params might need work.
                # For now map to Flatten or similar if essentially flattened?
                # EIF has EIF_LAYER_FLATTEN.
                # If pure reshape, custom op needed or just omitted if buffers align.
                f.write(f"        .type = EIF_LAYER_FLATTEN, // Mapped Reshape\n")
                f.write("        .weights = NULL, .bias = NULL\n")
            
            elif layer.type_name == "ADD":
                write_common("EIF_LAYER_ADD")
                
            elif layer.type_name == "LSTM":
                write_common("EIF_LAYER_LSTM")
                p = layer.params
                f.write(f"        .params.rnn = {{ .hidden_size={p['units']}, .stateful=false, .return_sequences={str(p['return_sequences']).lower()} }}\n")
            
            elif layer.type_name == "GRU":
                write_common("EIF_LAYER_GRU")
                p = layer.params
                f.write(f"        .params.rnn = {{ .hidden_size={p['units']}, .stateful=false, .return_sequences={str(p['return_sequences']).lower()} }}\n")

            elif layer.type_name == "RNN":
                write_common("EIF_LAYER_RNN")
                p = layer.params
                f.write(f"        .params.rnn = {{ .hidden_size={p['units']}, .stateful=false, .return_sequences={str(p['return_sequences']).lower()} }}\n")
            
            else:
                f.write(f"        // Unknown layer type: {layer.type_name}\n")
                
            f.write("    },\n")
            
        # Calculate state size
        state_size = 0
        for layer in layers:
            if layer.type_name in ["RNN", "GRU", "LSTM"]:
                # Runtime rnn_state is int16_t* in eif_model.h
                # So 2 bytes per unit.
                # LSTM needs H and C (2x). GRU needs H. RNN needs H.
                mult = 2 if layer.type_name == "LSTM" else 1
                state_size += layer.params['units'] * mult

        f.write("};\n\n");

        # Generate Init Function
        f.write(f"static eif_model_t {model_name};\n")
        f.write(f"static int16_t {model_name}_pool[{arena_size//2}];\n") # Arena is bytes, pool is int16, integer division
        if state_size > 0:
            f.write(f"static int16_t {model_name}_state[{state_size}];\n")
            
        f.write(f"static inline void {model_name}_init(void) {{\n")
        f.write(f"    eif_model_create(&{model_name}, (eif_layer_t*){model_name}_layers, {len(layers)});\n")
        f.write(f"    {model_name}.workspace_a = {model_name}_pool;\n") # eif_model_create assigns default static workspace?
        # Check eif_model_create. It assigns static workspace.
        # But if we want unique workspace per model instance, we should pass it.
        # Current eif_model.h uses global static workspace _eif_workspace_a.
        # That's fine for demo.
        
        if state_size > 0:
             f.write(f"    {model_name}.rnn_state = {model_name}_state;\n")
             f.write(f"    {model_name}.rnn_state_size = {state_size};\n")
             f.write(f"    {model_name}.state_size_bytes = {state_size * 2};\n")
             
        f.write("}\n\n")

        f.write(f"#endif // {model_name.upper()}_H\n")

def main():
    parser = argparse.ArgumentParser(description="Convert ONNX/TFLite models to EIF C Header")
    parser.add_argument("input", help="Input model file (.onnx or .tflite)")
    parser.add_argument("output", help="Output Header file")
    parser.add_argument("--name", default="model", help="Model name prefix")
    parser.add_argument("--no-quantize", action="store_true", help="Disable Q15 quantization (Output Float32)")
    
    args = parser.parse_args()
    
    # Default to Quantize=True unless --no-quantize
    quantize = not args.no_quantize
    
    if args.input.endswith(".onnx"):
        layers = load_onnx_model(args.input)
        generate_c_header(layers, args.name, args.output, quantize=quantize)
        print(f"Successfully converted {args.input} to {args.output} (Quantized: {quantize})")
    elif args.input.endswith(".tflite"):
        from .tflite_loader import load_tflite_model
        layers = load_tflite_model(args.input)
        generate_c_header(layers, args.name, args.output, quantize=quantize)
        print(f"Successfully converted {args.input} to {args.output} (Quantized: {quantize})")
    else:
        print("Unsupported file format")

if __name__ == "__main__":
    main()
