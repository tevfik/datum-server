
#include "eif_model.h"
#include "model_q15.h"
#include <stdio.h>
// #include "model_f32.h" // Just to show existence

int main() {
  printf("EIF Model Compiler & Runtime Demo\n");
  printf("=================================\n");

  // 1. Q15 Model Execution
  printf("\n[Q15 Mode] Initializing Model...\n");
  model_q15_init();

  eif_model_summary(&model_q15);

  int16_t input[784] = {0}; // 28x28 Input
  // Fill dummy data
  for (int i = 0; i < 784; i++)
    input[i] = (i % 2 == 0) ? 1000 : -1000;

  int16_t output[10];

  printf("\n[Q15 Mode] Running Inference (Time Step 1)...\n");
  eif_status_t status = eif_model_infer(&model_q15, input, output);

  if (status == EIF_STATUS_OK) {
    printf("Success! Output: [%d, %d]\n", output[0], output[1]);
  } else {
    printf("Error: Inference failed with status %d\n", status);
    return 1;
  }

  printf("\n[Q15 Mode] Running Inference (Time Step 2 - Stateful)...\n");
  status = eif_model_infer(&model_q15, input, output);
  if (status == EIF_STATUS_OK) {
    printf("Success! Output: [%d, %d]\n", output[0], output[1]);
  }

  // 2. Float Mode Description
  printf("\n[Float Mode] Demonstration\n");
  printf("The Compiler also supports '--no-quantize' to output Float32 "
         "weights.\n");
  printf("Because the EIF Runtime (eif_model.h) is highly optimized for "
         "embedded Fixed-Point,\n");
  printf("the Float32 weights are typically used for:\n");
  printf("  1. Desktop verification/gold-standard comparison.\n");
  printf("  2. Devices with FPU where a float-specific runtime variant is "
         "used.\n");
  printf("  (See examples/model_compiler_demo_v2/model_f32.h for generated "
         "structure)\n");

  return 0;
}
