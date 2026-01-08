#include "../framework/eif_test_runner.h"
#include "eif_neural.h"
#include <string.h>

#define CUSTOM_OP_ID 0x1234

typedef struct {
    uint32_t op_id;
    float scalar;
} my_custom_params_t;

eif_status_t my_custom_op(eif_neural_context_t* ctx, const eif_layer_node_t* node, void** inputs, void** outputs) {
    float* in = (float*)inputs[0];
    float* out = (float*)outputs[0];
    my_custom_params_t* params = (my_custom_params_t*)node->params;
    
    out[0] = in[0] * params->scalar;
    
    return EIF_STATUS_OK;
}

bool test_custom_op_execution(void) {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;
    uint8_t pool_buffer[4096];
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Setup Tensors
    eif_tensor_t tensors[2];
    memset(tensors, 0, sizeof(tensors));
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].size_bytes = sizeof(float);
    tensors[0].is_variable = true;
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].size_bytes = sizeof(float);
    tensors[1].is_variable = true;
    
    int input_indices[] = {0};
    int output_indices[] = {1};
    
    model.tensors = tensors;
    model.num_tensors = 2;
    model.input_tensor_indices = input_indices;
    model.num_inputs = 1;
    model.output_tensor_indices = output_indices;
    model.num_outputs = 1;
    model.scratch_size = 1024;
    
    // Setup Node
    eif_layer_node_t node;
    memset(&node, 0, sizeof(node));
    node.type = EIF_LAYER_CUSTOM;
    node.input_indices = input_indices;
    node.num_inputs = 1;
    node.output_indices = output_indices;
    node.num_outputs = 1;
    
    my_custom_params_t params;
    params.op_id = CUSTOM_OP_ID;
    params.scalar = 2.0f;
    node.params = &params;
    
    model.nodes = &node;
    model.num_nodes = 1;
    
    // Init
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_init(&ctx, &model, &pool));
    
    // Register Custom Op
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_register_custom_op(&ctx, CUSTOM_OP_ID, my_custom_op));
    
    // Set Input
    float input_val = 10.0f;
    eif_neural_set_input(&ctx, 0, &input_val, sizeof(float));
    
    // Invoke
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_neural_invoke(&ctx));
    
    // Check Output
    float output_val = 0.0f;
    eif_neural_get_output(&ctx, 0, &output_val, sizeof(float));
    
    TEST_ASSERT_EQUAL_FLOAT(20.0f, output_val, 0.001f);
    
    return true;
}

int main(void) {
    if (test_custom_op_execution()) {
        printf("test_custom_op_execution PASSED\n");
        return 0;
    } else {
        printf("test_custom_op_execution FAILED\n");
        return 1;
    }
}
