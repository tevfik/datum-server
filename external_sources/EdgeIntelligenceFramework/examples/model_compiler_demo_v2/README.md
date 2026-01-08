# Model Compiler & Runtime Demo

This example demonstrates the complete workflow of the Edge Intelligence Framework (EIF) for Deep Learning models.

## Structure

*   `train_and_export.py`: (Requires PyTorch/ONNX) A Python script that creates a hybrid CNN+RNN model, exports it to ONNX, and would normally run the `eif_compiler`.
*   `model_q15.h`: A pre-generated C header (simulated) representing the output of the compiler for the Q15 Fixed-Point Runtime.
*   `model_f32.h`: A pre-generated C header (simulated) representing the output of the compiler for Float32 mode.
*   `main.c`: The C application that loads the model using the EIF Runtime (`eif_model.h`) and runs inference.

## How to Run (C Example)

The C example is self-contained and does not require Python dependencies to run the verification:

```bash
mkdir build
cd build
cmake ..
make
./model_compiler_demo_v2
```

## How to Run (Full Pipeline)

If you have a Python environment with `toch`, `onnx`, and `numpy` installed, you can generate the artifacts yourself:

1.  Run the training script:
    ```bash
    python3 train_and_export.py
    ```
2.  Compile the model (from project root):
    ```bash
    # Q15 Version
    python3 ../../../tools/model-compiler/eif_compiler.py hybrid_model.onnx --output model_q15.h --name model_q15 --quantize

    # Float Version
    python3 ../../../tools/model-compiler/eif_compiler.py hybrid_model.onnx --output model_f32.h --name model_f32 --no-quantize
    ```
