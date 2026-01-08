#include "../framework/eif_test_runner.h"
#include "eif_neural.h"
#include "eif_dl_internal.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static void setup_tensor(eif_tensor_t* t, eif_tensor_type_t type, int* dims, int num_dims, bool is_variable) {
    t->type = type;
    t->num_dims = num_dims;
    for(int i=0; i<num_dims; i++) t->dims[i] = dims[i];
    t->is_variable = is_variable;
    t->data = NULL;
    t->size_bytes = eif_tensor_type_size(type);
    for(int i=0; i<num_dims; i++) t->size_bytes *= dims[i];
    t->arena_offset = 0; 
}

// Helper to create a simple model context
static void setup_model(eif_neural_context_t* ctx, eif_model_t* model, eif_memory_pool_t* pool, 
                       eif_tensor_t* tensors, int num_tensors,
                       eif_layer_node_t* nodes, int num_nodes,
                       int* input_indices, int num_inputs,
                       int* output_indices, int num_outputs,
                       size_t persistent_size) {
    
    model->tensors = tensors;
    model->num_tensors = num_tensors;
    model->nodes = nodes;
    model->num_nodes = num_nodes;
    model->input_tensor_indices = input_indices;
    model->num_inputs = num_inputs;
    model->output_tensor_indices = output_indices;
    model->num_outputs = num_outputs;
    model->scratch_size = 1024;
    model->persistent_size = persistent_size;

    // Initialize pool
    uint8_t* pool_buffer = (uint8_t*)malloc(8192); // Increased for persistent memory
    eif_memory_pool_init(pool, pool_buffer, 8192);

    eif_neural_init(ctx, model, pool);
}

static void teardown_model(eif_neural_context_t* ctx, eif_memory_pool_t* pool) {
    free(pool->base_addr);
}

bool test_dispatcher_reshape_flatten() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;
    
    // Tensors:
    // 0: Input (1x2x2x1)
    // 1: Reshape Output (1x1x1x4)
    // 2: Flatten Output (4)
    eif_tensor_t tensors[3];
    
    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Reshape Output
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=4;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float32_t);
    tensors[1].is_variable = true;

    // Flatten Output
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=4; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 4 * sizeof(float32_t);
    tensors[2].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[2];
    
    // Node 0: Reshape 0 -> 1
    int n0_in[] = {0};
    int n0_out[] = {1};
    nodes[0].type = EIF_LAYER_RESHAPE;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 1;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    nodes[0].params = NULL;

    // Node 1: Flatten 1 -> 2
    int n1_in[] = {1};
    int n1_out[] = {2};
    nodes[1].type = EIF_LAYER_FLATTEN;
    nodes[1].input_indices = n1_in;
    nodes[1].num_inputs = 1;
    nodes[1].output_indices = n1_out;
    nodes[1].num_outputs = 1;
    nodes[1].params = NULL;

    int model_inputs[] = {0};
    int model_outputs[] = {2};

    setup_model(&ctx, &model, &pool, tensors, 3, nodes, 2, model_inputs, 1, model_outputs, 1, 0);

    // Set Input
    float32_t input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    float32_t output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);

    // Verify data is preserved
    for(int i=0; i<4; i++) {
        if (output_data[i] != input_data[i]) return false;
    }

    return true;
}

bool test_dispatcher_pooling() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input (1x4x4x1)
    // 1: AvgPool Out (1x2x2x1)
    // 2: GlobalAvgPool Out (1x1x1x1)
    eif_tensor_t tensors[4];

    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=4; tensors[0].dims[2]=4; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 16 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // AvgPool Out
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float32_t);
    tensors[1].is_variable = true;

    // GlobalAvgPool Out
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 1 * sizeof(float32_t);
    tensors[2].is_variable = true;

    // MaxPool Out
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=2; tensors[3].dims[2]=2; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 4 * sizeof(float32_t);
    tensors[3].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[3];

    // Node 0: AvgPool 2x2
    int n0_in[] = {0};
    int n0_out[] = {1};
    nodes[0].type = EIF_LAYER_AVGPOOL2D;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 1;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    
    // Params for AvgPool
    static uint8_t params_buffer[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_buffer;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_buffer, 0, sizeof(params_buffer));
    lp->avgpool2d.pool_h = 2;
    lp->avgpool2d.pool_w = 2;
    lp->avgpool2d.stride_h = 2;
    lp->avgpool2d.stride_w = 2;
    nodes[0].params = params_buffer;

    // Node 1: GlobalAvgPool
    int n1_in[] = {1};
    int n1_out[] = {2};
    nodes[1].type = EIF_LAYER_GLOBAL_AVGPOOL2D;
    nodes[1].input_indices = n1_in;
    nodes[1].num_inputs = 1;
    nodes[1].output_indices = n1_out;
    nodes[1].num_outputs = 1;
    nodes[1].params = NULL; // GAP usually has no params

    // Node 2: MaxPool 2x2
    int n2_in[] = {0};
    int n2_out[] = {3};
    nodes[2].type = EIF_LAYER_MAXPOOL2D;
    nodes[2].input_indices = n2_in;
    nodes[2].num_inputs = 1;
    nodes[2].output_indices = n2_out;
    nodes[2].num_outputs = 1;
    
    static uint8_t params_max[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp_max = (eif_quant_param_t*)params_max;
    eif_layer_param_t* lp_max = (eif_layer_param_t*)(qp_max + 1);
    memset(params_max, 0, sizeof(params_max));
    lp_max->maxpool2d.pool_h = 2;
    lp_max->maxpool2d.pool_w = 2;
    lp_max->maxpool2d.stride_h = 2;
    lp_max->maxpool2d.stride_w = 2;
    nodes[2].params = params_max;

    int model_inputs[] = {0};
    int model_outputs[] = {2, 3};

    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 3, model_inputs, 1, model_outputs, 2, 0);

    // Set Input: All 1.0s
    float32_t input_data[16];
    for(int i=0; i<16; i++) input_data[i] = 1.0f;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    float32_t output_data[1];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);

    // Avg of 1.0s is 1.0
    if (output_data[0] != 1.0f) return false;

    return true;
}

bool test_dispatcher_concat_axis0() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input A (1x1x1x1)
    // 1: Input B (1x1x1x1)
    // 2: Output (2x1x1x1)
    eif_tensor_t tensors[3];

    // Input A
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 1 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Input B
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 1 * sizeof(float32_t);
    tensors[1].is_variable = true;

    // Output
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=2; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 2 * sizeof(float32_t);
    tensors[2].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];

    // Node 0: Concat Axis 0
    int n0_in[] = {0, 1};
    int n0_out[] = {2};
    nodes[0].type = EIF_LAYER_CONCAT;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 2;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_buffer[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_buffer;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_buffer, 0, sizeof(params_buffer));
    lp->concat.axis = 0;
    nodes[0].params = params_buffer;

    int model_inputs[] = {0, 1};
    int model_outputs[] = {2};

    setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 2, model_outputs, 1, 0);

    // Set Inputs
    float32_t in_a = 10.0f;
    float32_t in_b = 20.0f;
    eif_neural_set_input(&ctx, 0, &in_a, sizeof(float32_t));
    eif_neural_set_input(&ctx, 1, &in_b, sizeof(float32_t));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    float32_t output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);

    if (output_data[0] != 10.0f) return false;
    if (output_data[1] != 20.0f) return false;

    return true;
}

bool test_dispatcher_transpose_conv() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input (1x2x2x1)
    // 1: Weights (1x2x2x1) - Constant
    // 2: Output (1x4x4x1)
    eif_tensor_t tensors[3];

    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Weights (Constant)
    static float32_t w_data[] = {1.0f, 1.0f, 1.0f, 1.0f};
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float32_t);
    tensors[1].is_variable = false;
    tensors[1].data = w_data;

    // Output
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=4; tensors[2].dims[2]=4; tensors[2].dims[3]=1;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 16 * sizeof(float32_t);
    tensors[2].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];

    // Node 0: Transpose Conv
    int n0_in[] = {0, 1}; // Input, Weights
    int n0_out[] = {2};
    nodes[0].type = EIF_LAYER_TRANSPOSE_CONV2D;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 2;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_buffer[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_buffer;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_buffer, 0, sizeof(params_buffer));
    lp->transpose_conv2d.filters = 1;
    lp->transpose_conv2d.kernel_h = 2;
    lp->transpose_conv2d.kernel_w = 2;
    lp->transpose_conv2d.stride_h = 2;
    lp->transpose_conv2d.stride_w = 2;
    nodes[0].params = params_buffer;

    int model_inputs[] = {0};
    int model_outputs[] = {2};

    setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

    // Set Input
    float32_t input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    float32_t output_data[16];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);

    // Check corner (1.0 * 1.0 = 1.0)
    if (output_data[0] != 1.0f) return false;

    return true;
}

bool test_dispatcher_transpose_conv_bias() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input (1x2x2x1)
    // 1: Weights (1x2x2x1) - Constant
    // 2: Bias (1) - Constant
    // 3: Output (1x4x4x1)
    eif_tensor_t tensors[4];

    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=2; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Weights (Constant)
    static float32_t w_data[] = {1.0f, 1.0f, 1.0f, 1.0f};
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=2; tensors[1].dims[2]=2; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(float32_t);
    tensors[1].is_variable = false;
    tensors[1].data = w_data;

    // Bias (Constant)
    static float32_t b_data[] = {1.0f};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = sizeof(float32_t);
    tensors[2].is_variable = false;
    tensors[2].data = b_data;

    // Output
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=4; tensors[3].dims[2]=4; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 16 * sizeof(float32_t);
    tensors[3].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];

    // Node 0: Transpose Conv
    int n0_in[] = {0, 1, 2}; // Input, Weights, Bias
    int n0_out[] = {3};
    nodes[0].type = EIF_LAYER_TRANSPOSE_CONV2D;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 3;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_buffer[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_buffer;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_buffer, 0, sizeof(params_buffer));
    lp->transpose_conv2d.filters = 1;
    lp->transpose_conv2d.kernel_h = 2;
    lp->transpose_conv2d.kernel_w = 2;
    lp->transpose_conv2d.stride_h = 2;
    lp->transpose_conv2d.stride_w = 2;
    nodes[0].params = params_buffer;

    int model_inputs[] = {0};
    int model_outputs[] = {3};

    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

    // Set Input
    float32_t input_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    float32_t output_data[16];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);

    // Check corner (1.0 * 1.0 + 1.0 = 2.0)
    if (output_data[0] != 2.0f) return false;

    return true;
}

bool test_dispatcher_depthwise_conv() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input (1x3x3x2)
    // 1: Weights (1x3x3x2)
    // 2: Bias (2)
    // 3: Output (1x1x1x2)
    eif_tensor_t tensors[4];

    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=3; tensors[0].dims[2]=3; tensors[0].dims[3]=2;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 18 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Weights
    // 2 channels, 3x3 kernel. All 1.0s.
    static float32_t w_data[18];
    for(int i=0; i<18; i++) w_data[i] = 1.0f;
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=3; tensors[1].dims[2]=3; tensors[1].dims[3]=2;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 18 * sizeof(float32_t);
    tensors[1].is_variable = false;
    tensors[1].data = w_data;

    // Bias
    static float32_t b_data[2] = {1.0f, 1.0f};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=2; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 2 * sizeof(float32_t);
    tensors[2].is_variable = false;
    tensors[2].data = b_data;

    // Output
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=1; tensors[3].dims[3]=2;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 2 * sizeof(float32_t);
    tensors[3].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];

    // Node 0: Depthwise Conv
    int n0_in[] = {0, 1, 2};
    int n0_out[] = {3};
    nodes[0].type = EIF_LAYER_DEPTHWISE_CONV2D;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 3;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_buffer[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_buffer;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_buffer, 0, sizeof(params_buffer));
    lp->depthwise_conv2d.kernel_h = 3;
    lp->depthwise_conv2d.kernel_w = 3;
    lp->depthwise_conv2d.stride_h = 1;
    lp->depthwise_conv2d.stride_w = 1;
    lp->depthwise_conv2d.depth_multiplier = 1;
    nodes[0].params = params_buffer;

    int model_inputs[] = {0};
    int model_outputs[] = {3};

    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

    // Set Input: All 1.0s
    float32_t input_data[18];
    for(int i=0; i<18; i++) input_data[i] = 1.0f;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    float32_t output_data[2];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);

    // 3x3 kernel of 1s convolved with 1s = 9.0
    if (output_data[0] != 10.0f) return false;
    if (output_data[1] != 10.0f) return false;

    return true;
}

bool test_dispatcher_quant_dequant() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input (Float)
    // 1: Quantized (Int8)
    // 2: Output (Float)
    eif_tensor_t tensors[3];

    // Input
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=4;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Quantized
    tensors[1].type = EIF_TENSOR_INT8;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=4;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 4 * sizeof(int8_t);
    tensors[1].is_variable = true;

    // Output
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=4;
    tensors[2].num_dims = 4;
    tensors[2].size_bytes = 4 * sizeof(float32_t);
    tensors[2].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[2];

    // Node 0: Quantize
    int n0_in[] = {0};
    int n0_out[] = {1};
    nodes[0].type = EIF_LAYER_QUANTIZE;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 1;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params for Quantize
    static uint8_t params_q[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp_q = (eif_quant_param_t*)params_q;
    eif_layer_param_t* lp_q = (eif_layer_param_t*)(qp_q + 1);
    memset(params_q, 0, sizeof(params_q));
    // Scale = 0.5, ZeroPoint = 0
    // 1.0 -> 2
    // -1.0 -> -2
    lp_q->quantize.scale = 0.5f;
    lp_q->quantize.zero_point = 0;
    lp_q->quantize.min_val = -128;
    lp_q->quantize.max_val = 127;
    nodes[0].params = params_q;

    // Node 1: Dequantize
    int n1_in[] = {1};
    int n1_out[] = {2};
    nodes[1].type = EIF_LAYER_DEQUANTIZE;
    nodes[1].input_indices = n1_in;
    nodes[1].num_inputs = 1;
    nodes[1].output_indices = n1_out;
    nodes[1].num_outputs = 1;

    // Params for Dequantize
    static uint8_t params_dq[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp_dq = (eif_quant_param_t*)params_dq;
    eif_layer_param_t* lp_dq = (eif_layer_param_t*)(qp_dq + 1);
    memset(params_dq, 0, sizeof(params_dq));
    lp_dq->dequantize.scale = 0.5f;
    lp_dq->dequantize.zero_point = 0;
    nodes[1].params = params_dq;

    int model_inputs[] = {0};
    int model_outputs[] = {2};

    setup_model(&ctx, &model, &pool, tensors, 3, nodes, 2, model_inputs, 1, model_outputs, 1, 0);

    // Set Input
    float32_t input_data[] = {1.0f, -1.0f, 0.0f, 2.0f};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    float32_t output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);

    // 1.0 -> 2 -> 1.0
    if (fabsf(output_data[0] - 1.0f) > 0.01f) return false;
    // -1.0 -> -2 -> -1.0
    if (fabsf(output_data[1] - (-1.0f)) > 0.01f) return false;

    return true;
}

bool test_dispatcher_rnn_simple() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input (1x1x2x4) [Batch, Time, Input] - EIF uses [N, H, W, C] usually, but for RNN:
    //    Dispatcher: int input_size = in_tensor->dims[2];
    //    So dims[2] is input_size. dims[1] is likely time steps?
    //    Let's assume [Batch, Time, Input, 1] or similar.
    //    Standard EIF RNN: input (Batch, Time, Input_Dim)
    //    Let's use [1, 2, 4, 1]
    // 1: Weights (Input)
    // 2: Weights (Recurrent) - EIF RNN usually packs them or separate?
    //    Looking at eif_dl_core.c: 
    //    layer_wrapper.weights = ctx->tensor_data[node->input_indices[1]];
    //    layer_wrapper.biases = ctx->tensor_data[node->input_indices[2]];
    //    It seems it expects a single weights tensor containing both input and recurrent weights?
    //    Or maybe just input weights?
    //    Let's check eif_layer_rnn implementation or assume standard:
    //    Usually [Units, Input] and [Units, Units].
    //    If it's a single tensor, it might be concatenated.
    //    Let's assume simple identity weights for now.
    // 3: Bias
    // 4: Output (1x1x2x4)

    eif_tensor_t tensors[5];

    // Input: 1 seq, 2 steps, 4 features
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=4; tensors[0].dims[3]=1;
    tensors[0].num_dims = 3; // Treat as 3D
    tensors[0].size_bytes = 8 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Weights: [Units, Input_Dim + Units] or similar?
    // Let's make a dummy weight tensor large enough.
    // Units=4. Input=4.
    // W_ih: 4x4, W_hh: 4x4. Total 32 floats.
    static float32_t w_data[32];
    for(int i=0; i<32; i++) w_data[i] = 0.1f; // Small weights
    
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=32;
    tensors[1].num_dims = 1;
    tensors[1].size_bytes = 32 * sizeof(float32_t);
    tensors[1].is_variable = false;
    tensors[1].data = w_data;

    // Bias: 4 units
    static float32_t b_data[4] = {0};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=4; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 4 * sizeof(float32_t);
    tensors[2].is_variable = false;
    tensors[2].data = b_data;

    // Output
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=2; tensors[3].dims[2]=4; tensors[3].dims[3]=1;
    tensors[3].num_dims = 3;
    tensors[3].size_bytes = 8 * sizeof(float32_t);
    tensors[3].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];
    int n0_in[] = {0, 1, 2};
    int n0_out[] = {3};
    nodes[0].type = EIF_LAYER_RNN;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 3;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_rnn[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_rnn;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_rnn, 0, sizeof(params_rnn));
    lp->rnn.units = 4;
    lp->rnn.return_sequences = 1;
    nodes[0].params = params_rnn;

    int model_inputs[] = {0};
    int model_outputs[] = {3};

    // Persistent size: Units * sizeof(float) = 4 * 4 = 16 bytes
    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 16);

    // Set Input
    float32_t input_data[8];
    for(int i=0; i<8; i++) input_data[i] = 1.0f;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output (Just check it runs and produces something)
    float32_t output_data[8];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);
    return true;
}

bool test_dispatcher_int8_dense() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input (Int8)
    // 1: Weights (Int8)
    // 2: Bias (Int32) - Usually bias is int32 for int8 inference
    // 3: Output (Int8)
    eif_tensor_t tensors[4];

    // Input: 1x4
    tensors[0].type = EIF_TENSOR_INT8;
    tensors[0].dims[0]=1; tensors[0].dims[1]=1; tensors[0].dims[2]=1; tensors[0].dims[3]=4;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 4;
    tensors[0].is_variable = true;

    // Weights: 4x4 (16 bytes)
    static int8_t w_data[16];
    for(int i=0; i<16; i++) w_data[i] = 1;
    tensors[1].type = EIF_TENSOR_INT8;
    tensors[1].dims[0]=4; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=4;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 16;
    tensors[1].is_variable = false;
    tensors[1].data = w_data;

    // Bias: 4 (16 bytes)
    static int32_t b_data[4] = {0};
    tensors[2].type = EIF_TENSOR_INT8; // Type field might be ignored for bias in dispatcher or cast
    // Dispatcher: layer_wrapper.biases = ctx->tensor_data[...]
    // eif_layer_dense_int8 expects int32_t* bias usually.
    tensors[2].dims[0]=4; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 16;
    tensors[2].is_variable = false;
    tensors[2].data = b_data;

    // Output: 1x4
    tensors[3].type = EIF_TENSOR_INT8;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=1; tensors[3].dims[3]=4;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 4;
    tensors[3].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];
    int n0_in[] = {0, 1, 2};
    int n0_out[] = {3};
    nodes[0].type = EIF_LAYER_DENSE;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 3;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_dense[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_dense;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_dense, 0, sizeof(params_dense));
    lp->dense.units = 4;
    
    // Quant Params
    qp->input_offset = 0;
    qp->output_offset = 0;
    qp->output_multiplier = 2147483647; // INT32_MAX (approx 1.0x)
    qp->output_shift = 0;
    qp->quantized_activation_min = -128;
    qp->quantized_activation_max = 127;
    
    nodes[0].params = params_dense;

    int model_inputs[] = {0};
    int model_outputs[] = {3};

    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

    // Set Input
    int8_t input_data[] = {1, 1, 1, 1};
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    int8_t output_data[4];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);
    
    // 1*1 + 1*1 + 1*1 + 1*1 = 4
    if (output_data[0] != 4) return false;

    return true;
}

bool test_dispatcher_int8_conv2d() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors:
    // 0: Input (Int8) 1x3x3x1
    // 1: Weights (Int8) 1x3x3x1
    // 2: Bias (Int32)
    // 3: Output (Int8) 1x1x1x1
    eif_tensor_t tensors[4];

    // Input
    tensors[0].type = EIF_TENSOR_INT8;
    tensors[0].dims[0]=1; tensors[0].dims[1]=3; tensors[0].dims[2]=3; tensors[0].dims[3]=1;
    tensors[0].num_dims = 4;
    tensors[0].size_bytes = 9;
    tensors[0].is_variable = true;

    // Weights: 1 filter, 3x3 kernel, 1 channel
    static int8_t w_data[9];
    for(int i=0; i<9; i++) w_data[i] = 1;
    tensors[1].type = EIF_TENSOR_INT8;
    tensors[1].dims[0]=1; tensors[1].dims[1]=3; tensors[1].dims[2]=3; tensors[1].dims[3]=1;
    tensors[1].num_dims = 4;
    tensors[1].size_bytes = 9;
    tensors[1].is_variable = false;
    tensors[1].data = w_data;

    // Bias
    static int32_t b_data[1] = {0};
    tensors[2].type = EIF_TENSOR_INT8;
    tensors[2].dims[0]=1; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 4;
    tensors[2].is_variable = false;
    tensors[2].data = b_data;

    // Output
    tensors[3].type = EIF_TENSOR_INT8;
    tensors[3].dims[0]=1; tensors[3].dims[1]=1; tensors[3].dims[2]=1; tensors[3].dims[3]=1;
    tensors[3].num_dims = 4;
    tensors[3].size_bytes = 1;
    tensors[3].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];
    int n0_in[] = {0, 1, 2};
    int n0_out[] = {3};
    nodes[0].type = EIF_LAYER_CONV2D;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 3;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_conv[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_conv;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_conv, 0, sizeof(params_conv));
    lp->conv2d.filters = 1;
    lp->conv2d.kernel_h = 3;
    lp->conv2d.kernel_w = 3;
    lp->conv2d.stride_h = 1;
    lp->conv2d.stride_w = 1;
    
    qp->input_offset = 0;
    qp->output_offset = 0;
    qp->output_multiplier = 2147483647;
    qp->output_shift = 0;
    qp->quantized_activation_min = -128;
    qp->quantized_activation_max = 127;
    
    nodes[0].params = params_conv;

    int model_inputs[] = {0};
    int model_outputs[] = {3};

    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

    // Set Input
    int8_t input_data[9];
    for(int i=0; i<9; i++) input_data[i] = 1;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    // Check Output
    int8_t output_data[1];
    eif_neural_get_output(&ctx, 0, output_data, sizeof(output_data));

    teardown_model(&ctx, &pool);

    // 9 * (1*1) = 9
    if (output_data[0] != 9) return false;

    return true;
}

bool test_dispatcher_lstm_simple() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors: Input, Weights, Bias, Output
    eif_tensor_t tensors[4];

    // Input: 1x1x2x4
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=4; tensors[0].dims[3]=1;
    tensors[0].num_dims = 3;
    tensors[0].size_bytes = 8 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Weights
    static float32_t w_data[128]; // Enough for LSTM weights
    for(int i=0; i<128; i++) w_data[i] = 0.01f;
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=128;
    tensors[1].num_dims = 1;
    tensors[1].size_bytes = 128 * sizeof(float32_t);
    tensors[1].is_variable = false;
    tensors[1].data = w_data;

    // Bias
    static float32_t b_data[16] = {0};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=16; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 16 * sizeof(float32_t);
    tensors[2].is_variable = false;
    tensors[2].data = b_data;

    // Output
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=2; tensors[3].dims[2]=4; tensors[3].dims[3]=1;
    tensors[3].num_dims = 3;
    tensors[3].size_bytes = 8 * sizeof(float32_t);
    tensors[3].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];
    int n0_in[] = {0, 1, 2};
    int n0_out[] = {3};
    nodes[0].type = EIF_LAYER_LSTM;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 3;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_lstm[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_lstm;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_lstm, 0, sizeof(params_lstm));
    lp->lstm.units = 4;
    lp->lstm.return_sequences = 1;
    nodes[0].params = params_lstm;

    int model_inputs[] = {0};
    int model_outputs[] = {3};

    // Persistent size: 2 * Units * sizeof(float) = 2 * 4 * 4 = 32 bytes
    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 32);

    // Set Input
    float32_t input_data[8];
    for(int i=0; i<8; i++) input_data[i] = 1.0f;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    teardown_model(&ctx, &pool);
    return true;
}

bool test_dispatcher_gru_simple() {
    eif_neural_context_t ctx;
    eif_model_t model = {0};
    eif_memory_pool_t pool;

    // Tensors: Input, Weights, Bias, Output
    eif_tensor_t tensors[4];

    // Input: 1x1x2x4
    tensors[0].type = EIF_TENSOR_FLOAT32;
    tensors[0].dims[0]=1; tensors[0].dims[1]=2; tensors[0].dims[2]=4; tensors[0].dims[3]=1;
    tensors[0].num_dims = 3;
    tensors[0].size_bytes = 8 * sizeof(float32_t);
    tensors[0].is_variable = true;

    // Weights
    static float32_t w_data[128];
    for(int i=0; i<128; i++) w_data[i] = 0.01f;
    tensors[1].type = EIF_TENSOR_FLOAT32;
    tensors[1].dims[0]=1; tensors[1].dims[1]=1; tensors[1].dims[2]=1; tensors[1].dims[3]=128;
    tensors[1].num_dims = 1;
    tensors[1].size_bytes = 128 * sizeof(float32_t);
    tensors[1].is_variable = false;
    tensors[1].data = w_data;

    // Bias
    static float32_t b_data[16] = {0};
    tensors[2].type = EIF_TENSOR_FLOAT32;
    tensors[2].dims[0]=16; tensors[2].dims[1]=1; tensors[2].dims[2]=1; tensors[2].dims[3]=1;
    tensors[2].num_dims = 1;
    tensors[2].size_bytes = 16 * sizeof(float32_t);
    tensors[2].is_variable = false;
    tensors[2].data = b_data;

    // Output
    tensors[3].type = EIF_TENSOR_FLOAT32;
    tensors[3].dims[0]=1; tensors[3].dims[1]=2; tensors[3].dims[2]=4; tensors[3].dims[3]=1;
    tensors[3].num_dims = 3;
    tensors[3].size_bytes = 8 * sizeof(float32_t);
    tensors[3].is_variable = true;

    // Nodes
    eif_layer_node_t nodes[1];
    int n0_in[] = {0, 1, 2};
    int n0_out[] = {3};
    nodes[0].type = EIF_LAYER_GRU;
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 3;
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    // Params
    static uint8_t params_gru[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params_gru;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params_gru, 0, sizeof(params_gru));
    lp->gru.units = 4;
    lp->gru.return_sequences = 1;
    nodes[0].params = params_gru;

    int model_inputs[] = {0};
    int model_outputs[] = {3};

    // Persistent size: Units * sizeof(float) = 4 * 4 = 16 bytes
    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 16);

    // Set Input
    float32_t input_data[8];
    for(int i=0; i<8; i++) input_data[i] = 1.0f;
    eif_neural_set_input(&ctx, 0, input_data, sizeof(input_data));

    // Invoke
    eif_status_t status = eif_neural_invoke(&ctx);
    if (status != EIF_STATUS_OK) return false;

    teardown_model(&ctx, &pool);
    return true;
}

bool test_dispatcher_elementwise(void) {
    eif_layer_type_t types[] = {
        EIF_LAYER_ADD, EIF_LAYER_MULTIPLY, EIF_LAYER_SUB, EIF_LAYER_DIV
    };
    int num_types = 4;

    for(int i=0; i<num_types; i++) {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[3];
        eif_layer_node_t nodes[1];

        // Tensors: Input1, Input2, Output
        int dims[] = {1, 1, 1, 4};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true); // Input 1
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 4, true); // Input 2
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, dims, 4, true); // Output

        nodes[0].type = types[i];
        int n0_in[] = {0, 1};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 2;
        int n0_out[] = {2};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        nodes[0].params = NULL;

        int model_inputs[] = {0, 1};
        int model_outputs[] = {2};

        setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 2, model_outputs, 1, 0);

        float32_t in1[] = {1, 2, 3, 4};
        float32_t in2[] = {10, 20, 30, 40};
        eif_neural_set_input(&ctx, 0, in1, sizeof(in1));
        eif_neural_set_input(&ctx, 1, in2, sizeof(in2));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;

        teardown_model(&ctx, &pool);
    }
    return true;
}

bool test_dispatcher_activations(void) {
    eif_layer_type_t types[] = {
        EIF_LAYER_SOFTMAX, EIF_LAYER_SIGMOID, EIF_LAYER_TANH, 
        EIF_LAYER_LEAKY_RELU, EIF_LAYER_GELU, EIF_LAYER_HARD_SWISH,
        EIF_LAYER_RELU, EIF_LAYER_RELU6
    };
    int num_types = 8;

    for(int i=0; i<num_types; i++) {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[2];
        eif_layer_node_t nodes[1];

        int dims[] = {1, 1, 1, 4};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 4, true);

        nodes[0].type = types[i];
        int n0_in[] = {0};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 1;
        int n0_out[] = {1};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        
        static uint8_t params[sizeof(eif_layer_param_t)];
        if (types[i] == EIF_LAYER_LEAKY_RELU) {
            eif_layer_param_t* lp = (eif_layer_param_t*)params;
            lp->leaky_relu.alpha = 0.1f;
            nodes[0].params = params;
        } else {
            nodes[0].params = NULL;
        }

        int model_inputs[] = {0};
        int model_outputs[] = {1};

        setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {0.0f, 1.0f, -1.0f, 2.0f};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;

        teardown_model(&ctx, &pool);
    }
    return true;
}

bool test_dispatcher_normalization(void) {
    // Batch Norm
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[6]; // In, Gamma, Beta, Mean, Var, Out
        eif_layer_node_t nodes[1];

        int dims[] = {1, 2, 2, 1};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true); // Input
        
        int param_dims[] = {1};
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, param_dims, 1, false); // Gamma
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, param_dims, 1, false); // Beta
        setup_tensor(&tensors[3], EIF_TENSOR_FLOAT32, param_dims, 1, false); // Mean
        setup_tensor(&tensors[4], EIF_TENSOR_FLOAT32, param_dims, 1, false); // Var
        
        setup_tensor(&tensors[5], EIF_TENSOR_FLOAT32, dims, 4, true); // Output

        // Set param data
        static float32_t gamma[] = {1.0f};
        static float32_t beta[] = {0.0f};
        static float32_t mean[] = {0.0f};
        static float32_t var[] = {1.0f};
        tensors[1].data = gamma;
        tensors[2].data = beta;
        tensors[3].data = mean;
        tensors[4].data = var;

        nodes[0].type = EIF_LAYER_BATCH_NORM;
        int n0_in[] = {0, 1, 2, 3, 4};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 5;
        int n0_out[] = {5};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        nodes[0].params = NULL;

        int model_inputs[] = {0};
        int model_outputs[] = {5};

        setup_model(&ctx, &model, &pool, tensors, 6, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {1.0f, 2.0f, 3.0f, 4.0f};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }

    // Layer Norm
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[4]; // In, Weights, Biases, Out
        eif_layer_node_t nodes[1];

        int dims[] = {1, 1, 1, 4};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true); // Input
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 4, false); // Weights
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, dims, 4, false); // Biases
        setup_tensor(&tensors[3], EIF_TENSOR_FLOAT32, dims, 4, true); // Output

        static float32_t w[] = {1, 1, 1, 1};
        static float32_t b[] = {0, 0, 0, 0};
        tensors[1].data = w;
        tensors[2].data = b;

        nodes[0].type = EIF_LAYER_LAYER_NORM;
        int n0_in[] = {0, 1, 2};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 3;
        int n0_out[] = {3};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        nodes[0].params = NULL;

        int model_inputs[] = {0};
        int model_outputs[] = {3};

        setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {1.0f, 2.0f, 3.0f, 4.0f};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }
    return true;
}

bool test_dispatcher_math(void) {
    eif_layer_type_t types[] = {
        EIF_LAYER_EXP, EIF_LAYER_LOG, EIF_LAYER_SQRT
    };
    int num_types = 3;

    for(int i=0; i<num_types; i++) {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[2];
        eif_layer_node_t nodes[1];

        int dims[] = {1, 1, 1, 4};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 4, true);

        nodes[0].type = types[i];
        int n0_in[] = {0};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 1;
        int n0_out[] = {1};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        nodes[0].params = NULL;

        int model_inputs[] = {0};
        int model_outputs[] = {1};

        setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {1.0f, 2.0f, 3.0f, 4.0f};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }

    // Matmul
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[3];
        eif_layer_node_t nodes[1];

        // A: [2, 2], B: [2, 2], C: [2, 2]
        int dims[] = {2, 2};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 2, true);
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 2, true);
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, dims, 2, true);

        nodes[0].type = EIF_LAYER_MATMUL;
        int n0_in[] = {0, 1};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 2;
        int n0_out[] = {2};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        nodes[0].params = NULL;

        int model_inputs[] = {0, 1};
        int model_outputs[] = {2};

        setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 2, model_outputs, 1, 0);

        float32_t in[] = {1, 0, 0, 1};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));
        eif_neural_set_input(&ctx, 1, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }

    return true;
}

bool test_dispatcher_reduction(void) {
    eif_layer_type_t types[] = {
        EIF_LAYER_REDUCE_MEAN, EIF_LAYER_REDUCE_SUM
    };
    int num_types = 2;

    for(int i=0; i<num_types; i++) {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[2];
        eif_layer_node_t nodes[1];

        // Input: [1, 2, 2, 1]
        int dims[] = {1, 2, 2, 1};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
        
        // Output: [1, 1, 1, 1] (Reduce all)
        int out_dims[] = {1, 1, 1, 1};
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, out_dims, 4, true);

        nodes[0].type = types[i];
        int n0_in[] = {0};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 1;
        int n0_out[] = {1};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        
        static uint8_t params[sizeof(eif_layer_param_t)];
        eif_layer_param_t* lp = (eif_layer_param_t*)params;
        // Reduce along axis 1, 2 (H, W) - simplified test
        // Actually the param struct has 'axis' and 'keep_dims'
        lp->reduce.axis = 1; 
        lp->reduce.keep_dims = 1;
        nodes[0].params = params;

        int model_inputs[] = {0};
        int model_outputs[] = {1};

        setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {1, 2, 3, 4};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }
    return true;
}

bool test_dispatcher_manipulation(void) {
    // Split
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[3]; // In, Out1, Out2
        eif_layer_node_t nodes[1];

        // Input: [1, 2, 2, 2]
        int dims[] = {1, 2, 2, 2};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
        
        // Output: [1, 2, 2, 1]
        int out_dims[] = {1, 2, 2, 1};
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, out_dims, 4, true);
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, out_dims, 4, true);

        nodes[0].type = EIF_LAYER_SPLIT;
        int n0_in[] = {0};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 1;
        int n0_out[] = {1, 2};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 2;
        
        static uint8_t params[sizeof(eif_layer_param_t)];
        eif_layer_param_t* lp = (eif_layer_param_t*)params;
        lp->split.axis = 3; // Split channels
        lp->split.num_splits = 2;
        nodes[0].params = params;

        int model_inputs[] = {0};
        int model_outputs[] = {1, 2};

        setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 1, model_outputs, 2, 0);

        float32_t in[8];
        for(int i=0; i<8; i++) in[i] = (float)i;
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }

    // Pad
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[2];
        eif_layer_node_t nodes[1];

        int dims[] = {1, 2, 2, 1};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
        
        int out_dims[] = {1, 4, 4, 1}; // Pad 1 on each side
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, out_dims, 4, true);

        nodes[0].type = EIF_LAYER_PAD;
        int n0_in[] = {0};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 1;
        int n0_out[] = {1};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        
        static uint8_t params[sizeof(eif_layer_param_t)];
        eif_layer_param_t* lp = (eif_layer_param_t*)params;
        lp->pad.pads[0] = 1; lp->pad.pads[1] = 1; // Top, Bottom
        lp->pad.pads[2] = 1; lp->pad.pads[3] = 1; // Left, Right
        nodes[0].params = params;

        int model_inputs[] = {0};
        int model_outputs[] = {1};

        setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {1, 2, 3, 4};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }

    // Gather
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[3]; // In, Indices, Out
        eif_layer_node_t nodes[1];

        int dims[] = {4, 2}; // 4 rows, 2 cols
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 2, true);
        
        int idx_dims[] = {2}; // 2 indices
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, idx_dims, 1, true); // Indices are float in this framework?
        
        int out_dims[] = {2, 2};
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, out_dims, 2, true);

        nodes[0].type = EIF_LAYER_GATHER;
        int n0_in[] = {0, 1};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 2;
        int n0_out[] = {2};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        
        static uint8_t params[sizeof(eif_layer_param_t)];
        eif_layer_param_t* lp = (eif_layer_param_t*)params;
        lp->gather.axis = 0;
        nodes[0].params = params;

        int model_inputs[] = {0, 1};
        int model_outputs[] = {2};

        setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 2, model_outputs, 1, 0);

        float32_t in[] = {1, 2, 3, 4, 5, 6, 7, 8};
        float32_t idx[] = {0, 2};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));
        eif_neural_set_input(&ctx, 1, idx, sizeof(idx));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }

    // Resize
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[2];
        eif_layer_node_t nodes[1];

        int dims[] = {1, 2, 2, 1};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
        
        int out_dims[] = {1, 4, 4, 1};
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, out_dims, 4, true);

        nodes[0].type = EIF_LAYER_RESIZE;
        int n0_in[] = {0};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 1;
        int n0_out[] = {1};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        
        static uint8_t params[sizeof(eif_layer_param_t)];
        eif_layer_param_t* lp = (eif_layer_param_t*)params;
        lp->resize.scale_h = 2.0f;
        lp->resize.scale_w = 2.0f;
        nodes[0].params = params;

        int model_inputs[] = {0};
        int model_outputs[] = {1};

        setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {1, 2, 3, 4};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }

    // Clip
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[2];
        eif_layer_node_t nodes[1];

        int dims[] = {1, 1, 1, 4};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 4, true);

        nodes[0].type = EIF_LAYER_CLIP;
        int n0_in[] = {0};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 1;
        int n0_out[] = {1};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        
        static uint8_t params[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
        eif_quant_param_t* qp = (eif_quant_param_t*)params;
        eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
        lp->clip.min_val = 0.0f;
        lp->clip.max_val = 1.0f;
        nodes[0].params = params;

        int model_inputs[] = {0};
        int model_outputs[] = {1};

        setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {-1.0f, 0.5f, 1.5f, 2.0f};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
        teardown_model(&ctx, &pool);
    }

    return true;
}

bool test_dispatcher_advanced(void) {
    // Attention
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[4]; // Q, K, V, Out
        eif_layer_node_t nodes[1];

        // Batch=1, Seq=2, Dim=2
        int dims[] = {1, 2, 2};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 3, true); // Q
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 3, true); // K
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, dims, 3, true); // V
        setup_tensor(&tensors[3], EIF_TENSOR_FLOAT32, dims, 3, true); // Out

        nodes[0].type = EIF_LAYER_ATTENTION;
        int n0_in[] = {0, 1, 2};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 3;
        int n0_out[] = {3};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        nodes[0].params = NULL;

        int model_inputs[] = {0, 1, 2};
        int model_outputs[] = {3};

        setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 3, model_outputs, 1, 0);

        float32_t in[] = {1, 0, 0, 1};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));
        eif_neural_set_input(&ctx, 1, in, sizeof(in));
        eif_neural_set_input(&ctx, 2, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) {
            return false;
        }
        teardown_model(&ctx, &pool);
    }

    // Embedding
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[3]; // Indices, Weights, Out
        eif_layer_node_t nodes[1];

        // Indices: [1, 2] (Batch=1, Seq=2)
        int idx_dims[] = {1, 2};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, idx_dims, 2, true);
        
        // Weights: [4, 2] (Vocab=4, Embed=2)
        int w_dims[] = {4, 2};
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, w_dims, 2, false);
        static float32_t w[] = {
            0.1, 0.1, 
            0.2, 0.2, 
            0.3, 0.3, 
            0.4, 0.4
        };
        tensors[1].data = w;

        // Out: [1, 2, 2]
        int out_dims[] = {1, 2, 2};
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, out_dims, 3, true);

        nodes[0].type = EIF_LAYER_EMBEDDING;
        int n0_in[] = {0, 1};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 2;
        int n0_out[] = {2};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 1;
        nodes[0].params = NULL;

        int model_inputs[] = {0};
        int model_outputs[] = {2};

        setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

        float32_t in[] = {1.0f, 3.0f}; // Indices 1 and 3
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) {
            return false;
        }
        teardown_model(&ctx, &pool);
    }

    // TopK
    {
        eif_neural_context_t ctx;
        eif_model_t model;
        eif_memory_pool_t pool;
        eif_tensor_t tensors[3]; // In, Values, Indices
        eif_layer_node_t nodes[1];

        // Input: [1, 4]
        int dims[] = {1, 4};
        setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 2, true);
        
        // Output: [1, 2] (K=2)
        int out_dims[] = {1, 2};
        setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, out_dims, 2, true); // Values
        setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, out_dims, 2, true); // Indices

        nodes[0].type = EIF_LAYER_TOPK;
        int n0_in[] = {0};
        nodes[0].input_indices = n0_in;
        nodes[0].num_inputs = 1;
        int n0_out[] = {1, 2};
        nodes[0].output_indices = n0_out;
        nodes[0].num_outputs = 2;
        
        static uint8_t params[sizeof(eif_layer_param_t)];
        eif_layer_param_t* lp = (eif_layer_param_t*)params;
        lp->topk.k = 2;
        nodes[0].params = params;

        int model_inputs[] = {0};
        int model_outputs[] = {1, 2};

        setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 2, model_outputs, 1, 0);

        float32_t in[] = {10, 30, 20, 40};
        eif_neural_set_input(&ctx, 0, in, sizeof(in));

        if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) {
            return false;
        }
        teardown_model(&ctx, &pool);
    }

    return true;
}

bool test_dispatcher_resize_bilinear(void) {
    eif_neural_context_t ctx;
    eif_model_t model;
    eif_memory_pool_t pool;
    eif_tensor_t tensors[2];
    eif_layer_node_t nodes[1];

    int dims[] = {1, 2, 2, 1};
    setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
    
    int out_dims[] = {1, 4, 4, 1};
    setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, out_dims, 4, true);

    nodes[0].type = EIF_LAYER_RESIZE;
    int n0_in[] = {0};
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 1;
    int n0_out[] = {1};
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    
    static uint8_t params[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params, 0, sizeof(params));
    lp->resize.scale_h = 2.0f;
    lp->resize.scale_w = 2.0f;
    lp->resize.method = 1; // Bilinear
    nodes[0].params = params;

    int model_inputs[] = {0};
    int model_outputs[] = {1};

    setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

    float32_t in[] = {1, 2, 3, 4};
    eif_neural_set_input(&ctx, 0, in, sizeof(in));

    if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
    teardown_model(&ctx, &pool);
    return true;
}

bool test_dispatcher_conv1d(void) {
    eif_neural_context_t ctx;
    eif_model_t model;
    eif_memory_pool_t pool;
    eif_tensor_t tensors[4]; // In, Weights, Biases, Out
    eif_layer_node_t nodes[1];

    // Input: [1, 1, 4, 1] (N, H, W, C) - Conv1D treats W as sequence length
    int dims[] = {1, 1, 4, 1};
    setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
    
    // Weights: [1, 2, 1] (Filters, Kernel, InC)
    int w_dims[] = {1, 2, 1};
    setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, w_dims, 3, false);
    static float32_t w[] = {1.0f, 1.0f};
    tensors[1].data = w;

    // Biases: [1]
    int b_dims[] = {1};
    setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, b_dims, 1, false);
    static float32_t b[] = {0.0f};
    tensors[2].data = b;

    // Output: [1, 1, 3, 1] (W_out = (4-2)/1 + 1 = 3)
    int out_dims[] = {1, 1, 3, 1};
    setup_tensor(&tensors[3], EIF_TENSOR_FLOAT32, out_dims, 4, true);

    nodes[0].type = EIF_LAYER_CONV1D;
    int n0_in[] = {0, 1, 2};
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 3;
    int n0_out[] = {3};
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    
    static uint8_t params[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params, 0, sizeof(params));
    lp->conv1d.filters = 1;
    lp->conv1d.kernel_size = 2;
    lp->conv1d.stride = 1;
    lp->conv1d.pad = 0;
    nodes[0].params = params;
    // nodes[0].activation = EIF_ACT_RELU; // Activation not directly accessible in node struct

    int model_inputs[] = {0};
    int model_outputs[] = {3};

    setup_model(&ctx, &model, &pool, tensors, 4, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

    float32_t in[] = {1, 2, 3, 4};
    eif_neural_set_input(&ctx, 0, in, sizeof(in));

    if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
    teardown_model(&ctx, &pool);
    return true;
}

bool test_dispatcher_clip_default(void) {
    eif_neural_context_t ctx;
    eif_model_t model;
    eif_memory_pool_t pool;
    eif_tensor_t tensors[2];
    eif_layer_node_t nodes[1];

    int dims[] = {1, 1, 1, 4};
    setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 4, true);
    setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 4, true);

    nodes[0].type = EIF_LAYER_CLIP;
    int n0_in[] = {0};
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 1;
    int n0_out[] = {1};
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    nodes[0].params = NULL; // Default clip (no params)

    int model_inputs[] = {0};
    int model_outputs[] = {1};

    setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 0);

    float32_t in[] = {-1.0f, 0.5f, 1.5f, 2.0f};
    eif_neural_set_input(&ctx, 0, in, sizeof(in));

    if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
    teardown_model(&ctx, &pool);
    return true;
}

bool test_dispatcher_concat_axis3(void) {
    eif_neural_context_t ctx;
    eif_model_t model;
    eif_memory_pool_t pool;
    eif_tensor_t tensors[3];
    eif_layer_node_t nodes[1];

    // Input A: 1x1x1x2
    int dims_a[] = {1, 1, 1, 2};
    setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims_a, 4, true);

    // Input B: 1x1x1x2
    int dims_b[] = {1, 1, 1, 2};
    setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims_b, 4, true);

    // Output: 1x1x1x4
    int dims_out[] = {1, 1, 1, 4};
    setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, dims_out, 4, true);

    nodes[0].type = EIF_LAYER_CONCAT;
    int n0_in[] = {0, 1};
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 2;
    int n0_out[] = {2};
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;

    static uint8_t params[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params, 0, sizeof(params));
    lp->concat.axis = 3;
    nodes[0].params = params;

    int model_inputs[] = {0, 1};
    int model_outputs[] = {2};

    setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 2, model_outputs, 1, 0);

    float32_t in_a[] = {1, 2};
    float32_t in_b[] = {3, 4};
    eif_neural_set_input(&ctx, 0, in_a, sizeof(in_a));
    eif_neural_set_input(&ctx, 1, in_b, sizeof(in_b));

    if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
    
    float32_t out[4];
    eif_neural_get_output(&ctx, 0, out, sizeof(out));
    // Expected: 1, 2, 3, 4
    if (out[0] != 1 || out[1] != 2 || out[2] != 3 || out[3] != 4) return false;

    teardown_model(&ctx, &pool);
    return true;
}

// Forward declarations for internal functions to test coverage
void eif_layer_dense_q7(const eif_layer_t* layer, const int8_t* input, int8_t* output, int input_size);
void eif_layer_conv2d_q7(const eif_layer_t* layer, const int8_t* input, int8_t* output, 
                             int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);
eif_status_t eif_layer_resize(const float32_t* input, float32_t* output, const eif_tensor_shape_t* in_shape, const eif_tensor_shape_t* out_shape, const eif_layer_param_t* param);

bool test_layers_direct_coverage(void) {
    // Test eif_layer_dense_q7
    {
        eif_layer_t layer = {0};
        layer.params.dense.units = 1;
        int8_t weights[] = {1, 1};
        int32_t biases[] = {0};
        layer.weights = weights;
        layer.biases = biases;
        layer.quant_params.input_offset = 0;
        layer.quant_params.output_offset = 0;
        layer.quant_params.quantized_activation_min = -128;
        layer.quant_params.quantized_activation_max = 127;
        
        int8_t input[] = {10, 20};
        int8_t output[1];
        
        eif_layer_dense_q7(&layer, input, output, 2);
        
        if (output[0] != 30) return false;
    }

    // Test eif_layer_conv2d_q7
    {
        eif_layer_t layer = {0};
        layer.params.conv2d.filters = 1;
        layer.params.conv2d.kernel_h = 2;
        layer.params.conv2d.kernel_w = 2;
        layer.params.conv2d.stride_h = 1;
        layer.params.conv2d.stride_w = 1;
        
        int8_t weights[] = {1, 1, 1, 1}; // 2x2 kernel
        int32_t biases[] = {0};
        layer.weights = weights;
        layer.biases = biases;
        layer.quant_params.input_offset = 0;
        layer.quant_params.output_offset = 0;
        layer.quant_params.quantized_activation_min = -128;
        layer.quant_params.quantized_activation_max = 127;
        
        int8_t input[] = {1, 1, 1, 1, 1, 1, 1, 1, 1}; // 3x3 input
        int8_t output[4]; // 2x2 output
        
        int out_h, out_w, out_c;
        eif_layer_conv2d_q7(&layer, input, output, 3, 3, 1, &out_h, &out_w, &out_c);
        
        if (out_h != 2 || out_w != 2 || out_c != 1) return false;
        if (output[0] != 4) return false;
    }

    // Test eif_layer_resize (Bilinear) direct call
    {
        float32_t input[] = {1, 2, 3, 4}; // 2x2
        float32_t output[16]; // 4x4
        
        eif_tensor_shape_t in_shape = {{1, 2, 2, 1}};
        eif_tensor_shape_t out_shape = {{1, 4, 4, 1}};
        
        eif_layer_param_t param = {0};
        param.resize.method = 1; // Bilinear
        param.resize.scale_h = 2.0f;
        param.resize.scale_w = 2.0f;
        
        eif_layer_resize(input, output, &in_shape, &out_shape, &param);
        
        // Check corner
        if (output[0] != 1.0f) return false;
    }

    // Test eif_layer_conv1d with ReLU
    {
        eif_layer_t layer = {0};
        layer.type = EIF_LAYER_CONV1D;
        layer.activation = EIF_ACT_RELU;
        layer.params.conv1d.filters = 1;
        layer.params.conv1d.kernel_size = 2;
        layer.params.conv1d.stride = 1;
        layer.params.conv1d.pad = 0;
        
        float32_t weights[] = {1.0f, 1.0f};
        float32_t biases[] = {-10.0f}; // Large negative bias
        layer.weights = weights;
        layer.biases = biases;
        
        float32_t input[] = {1, 2, 3, 4}; // 1x4x1
        float32_t output[3]; // 1x3x1
        
        int out_w, out_c;
        eif_layer_conv1d(&layer, input, output, 4, 1, &out_w, &out_c);
        
        // 1+2 - 10 = -7 -> ReLU -> 0
        if (output[0] != 0.0f) return false;
    }

    // Test eif_layer_transpose_conv2d with ReLU6
    {
        eif_layer_t layer = {0};
        layer.type = EIF_LAYER_TRANSPOSE_CONV2D;
        layer.activation = EIF_ACT_RELU6;
        layer.params.transpose_conv2d.filters = 1;
        layer.params.transpose_conv2d.kernel_h = 2;
        layer.params.transpose_conv2d.kernel_w = 2;
        layer.params.transpose_conv2d.stride_h = 2;
        layer.params.transpose_conv2d.stride_w = 2;
        
        float32_t weights[] = {1.0f, 1.0f, 1.0f, 1.0f};
        float32_t biases[] = {10.0f}; // Large bias
        layer.weights = weights;
        layer.biases = biases;
        
        float32_t input[] = {1.0f, 2.0f, 3.0f, 4.0f}; // 2x2
        float32_t output[16]; // 4x4
        
        int out_h, out_w, out_c;
        eif_layer_transpose_conv2d(&layer, input, output, 2, 2, 1, &out_h, &out_w, &out_c);
        
        // 1.0 * 1.0 + 10.0 = 11.0 -> ReLU6 -> 6.0
        if (output[0] != 6.0f) return false;
    }

    // Test eif_layer_conv1d with ReLU6
    {
        eif_layer_t layer = {0};
        layer.type = EIF_LAYER_CONV1D;
        layer.activation = EIF_ACT_RELU6;
        layer.params.conv1d.filters = 1;
        layer.params.conv1d.kernel_size = 2;
        layer.params.conv1d.stride = 1;
        layer.params.conv1d.pad = 0;
        
        float32_t weights[] = {1.0f, 1.0f};
        float32_t biases[] = {10.0f}; // Large positive bias
        layer.weights = weights;
        layer.biases = biases;
        
        float32_t input[] = {1, 2, 3, 4}; // 1x4x1
        float32_t output[3]; // 1x3x1
        
        int out_w, out_c;
        eif_layer_conv1d(&layer, input, output, 4, 1, &out_w, &out_c);
        
        // 1+2 + 10 = 13 -> ReLU6 -> 6
        if (output[0] != 6.0f) return false;
    }

    // Test eif_layer_transpose_conv2d with ReLU
    {
        eif_layer_t layer = {0};
        layer.type = EIF_LAYER_TRANSPOSE_CONV2D;
        layer.activation = EIF_ACT_RELU;
        layer.params.transpose_conv2d.filters = 1;
        layer.params.transpose_conv2d.kernel_h = 2;
        layer.params.transpose_conv2d.kernel_w = 2;
        layer.params.transpose_conv2d.stride_h = 2;
        layer.params.transpose_conv2d.stride_w = 2;
        
        float32_t weights[] = {1.0f, 1.0f, 1.0f, 1.0f};
        float32_t biases[] = {-10.0f}; // Large negative bias
        layer.weights = weights;
        layer.biases = biases;
        
        float32_t input[] = {1.0f, 2.0f, 3.0f, 4.0f}; // 2x2
        float32_t output[16]; // 4x4
        
        int out_h, out_w, out_c;
        eif_layer_transpose_conv2d(&layer, input, output, 2, 2, 1, &out_h, &out_w, &out_c);
        
        // 1.0 * 1.0 - 10.0 = -9.0 -> ReLU -> 0.0
        if (output[0] != 0.0f) return false;
    }

    return true;
}

bool test_dispatcher_concat_axis1(void) {
    eif_neural_context_t ctx;
    eif_model_t model;
    eif_memory_pool_t pool;
    eif_tensor_t tensors[3];
    eif_layer_node_t nodes[1];

    // Input 1: [1, 2, 2, 1]
    int dims1[] = {1, 2, 2, 1};
    setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims1, 4, true);
    
    // Input 2: [1, 2, 2, 1]
    setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims1, 4, true);
    
    // Output: [1, 4, 2, 1] (Concat on axis 1)
    int out_dims[] = {1, 4, 2, 1};
    setup_tensor(&tensors[2], EIF_TENSOR_FLOAT32, out_dims, 4, true);

    nodes[0].type = EIF_LAYER_CONCAT;
    int n0_in[] = {0, 1};
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 2;
    int n0_out[] = {2};
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    
    static uint8_t params[sizeof(eif_quant_param_t) + sizeof(eif_layer_param_t)];
    eif_quant_param_t* qp = (eif_quant_param_t*)params;
    eif_layer_param_t* lp = (eif_layer_param_t*)(qp + 1);
    memset(params, 0, sizeof(params));
    lp->concat.axis = 1;
    nodes[0].params = params;

    int model_inputs[] = {0, 1};
    int model_outputs[] = {2};

    setup_model(&ctx, &model, &pool, tensors, 3, nodes, 1, model_inputs, 2, model_outputs, 1, 0);

    float32_t in1[] = {1, 2, 3, 4};
    float32_t in2[] = {5, 6, 7, 8};
    eif_neural_set_input(&ctx, 0, in1, sizeof(in1));
    eif_neural_set_input(&ctx, 1, in2, sizeof(in2));

    if (eif_neural_invoke(&ctx) != EIF_STATUS_OK) return false;
    
    float32_t out[8];
    eif_neural_get_output(&ctx, 0, out, sizeof(out));
    
    // Expected: 1,2,3,4 then 5,6,7,8
    if (out[0] != 1 || out[4] != 5) return false;

    teardown_model(&ctx, &pool);
    return true;
}

bool test_api_accessors(void) {
    eif_neural_context_t ctx;
    eif_model_t model;
    eif_memory_pool_t pool;
    eif_tensor_t tensors[2];
    eif_layer_node_t nodes[1];

    // Simple identity model
    int dims[] = {1, 1};
    setup_tensor(&tensors[0], EIF_TENSOR_FLOAT32, dims, 2, true);
    setup_tensor(&tensors[1], EIF_TENSOR_FLOAT32, dims, 2, true);

    nodes[0].type = EIF_LAYER_RESHAPE; // Just copy
    int n0_in[] = {0};
    nodes[0].input_indices = n0_in;
    nodes[0].num_inputs = 1;
    int n0_out[] = {1};
    nodes[0].output_indices = n0_out;
    nodes[0].num_outputs = 1;
    nodes[0].params = NULL;

    int model_inputs[] = {0};
    int model_outputs[] = {1};

    // Set persistent size to test reset_state
    setup_model(&ctx, &model, &pool, tensors, 2, nodes, 1, model_inputs, 1, model_outputs, 1, 100);

    // Test get_input_ptr
    void* in_ptr = eif_neural_get_input_ptr(&ctx, 0);
    if (in_ptr == NULL) return false;
    
    // Test get_output_ptr
    const void* out_ptr = eif_neural_get_output_ptr(&ctx, 0);
    if (out_ptr == NULL) return false;
    
    // Test reset_state
    if (eif_neural_reset_state(&ctx) != EIF_STATUS_OK) return false;
    
    teardown_model(&ctx, &pool);
    return true;
}

// Forward declaration
void eif_layer_conv2d_im2col(const eif_layer_t* layer, const float32_t* input, float32_t* output, 
                             int in_h, int in_w, int in_c, int* out_h, int* out_w, int* out_c);

bool test_im2col_direct(void) {
    eif_layer_t layer = {0};
    layer.params.conv2d.filters = 1;
    layer.params.conv2d.kernel_h = 2;
    layer.params.conv2d.kernel_w = 2;
    layer.params.conv2d.stride_h = 1;
    layer.params.conv2d.stride_w = 1;
    
    float32_t weights[] = {1, 1, 1, 1}; // 2x2 kernel
    float32_t biases[] = {0};
    layer.weights = weights;
    layer.biases = biases;
    
    float32_t input[] = {1, 1, 1, 1, 1, 1, 1, 1, 1}; // 3x3 input
    float32_t output[4]; // 2x2 output
    
    int out_h, out_w, out_c;
    eif_layer_conv2d_im2col(&layer, input, output, 3, 3, 1, &out_h, &out_w, &out_c);
    
    if (out_h != 2 || out_w != 2 || out_c != 1) return false;
    if (output[0] != 4.0f) return false;
    
    return true;
}

bool test_im2col_large(void) {
    // Test with enough filters to trigger AVX2 loop (N >= 8)
    // and enough height to trigger multiple tiles (o_h > 4)
    
    // Input: 8x8x1
    int in_h = 8, in_w = 8, in_c = 1;
    int k_h = 3, k_w = 3;
    int stride_h = 1, stride_w = 1;
    int filters = 8; // N=8 triggers AVX2 loop
    
    // Output dims: (8-3)/1 + 1 = 6
    int o_h = 6, o_w = 6;
    
    // Setup layer
    eif_layer_t layer = {0};
    layer.params.conv2d.filters = filters;
    layer.params.conv2d.kernel_h = k_h;
    layer.params.conv2d.kernel_w = k_w;
    layer.params.conv2d.stride_h = stride_h;
    layer.params.conv2d.stride_w = stride_w;
    
    // Weights: [filters, k_h, k_w, in_c] = [8, 3, 3, 1] = 72 elements
    // Set weights to 1.0 for simplicity
    int weight_count = filters * k_h * k_w * in_c;
    float* weights = (float*)malloc(weight_count * sizeof(float));
    for(int i=0; i<weight_count; i++) weights[i] = 1.0f;
    layer.weights = weights;
    
    // Biases: [filters] = 8 elements
    // Set biases to 0.0
    float* biases = (float*)calloc(filters, sizeof(float));
    layer.biases = biases;
    
    // Input: [8, 8, 1] = 64 elements
    // Set input to 1.0
    int input_count = in_h * in_w * in_c;
    float* input = (float*)malloc(input_count * sizeof(float));
    for(int i=0; i<input_count; i++) input[i] = 1.0f;
    
    // Output: [6, 6, 8]
    int out_count = o_h * o_w * filters;
    float* output = (float*)calloc(out_count, sizeof(float));
    
    int res_h, res_w, res_c;
    eif_layer_conv2d_im2col(&layer, input, output, in_h, in_w, in_c, &res_h, &res_w, &res_c);
    
    if (res_h != o_h || res_w != o_w || res_c != filters) return false;
    
    // Expected value:
    // Each output pixel is dot product of kernel (3x3=9 ones) and input window (9 ones).
    // Sum = 9.
    // Weights are all 1. Input is all 1.
    // So each channel should be 9.0f.
    
    for (int i = 0; i < out_count; i++) {
        if (fabs(output[i] - 9.0f) > 1e-5) return false;
    }
    
    free(weights);
    free(biases);
    free(input);
    free(output);
    return true;
}

int run_dispatcher_coverage_tests(void) {
    tests_run = 0; tests_passed = 0; tests_failed = 0;
    
    RUN_TEST(test_dispatcher_reshape_flatten);
    RUN_TEST(test_dispatcher_pooling);
    RUN_TEST(test_dispatcher_concat_axis0);
    RUN_TEST(test_dispatcher_concat_axis3);
    RUN_TEST(test_dispatcher_concat_axis1);
    RUN_TEST(test_dispatcher_transpose_conv);
    RUN_TEST(test_dispatcher_transpose_conv_bias);
    RUN_TEST(test_dispatcher_depthwise_conv);
    RUN_TEST(test_dispatcher_quant_dequant);
    RUN_TEST(test_dispatcher_rnn_simple);
    RUN_TEST(test_dispatcher_int8_dense);
    RUN_TEST(test_dispatcher_int8_conv2d);
    RUN_TEST(test_dispatcher_lstm_simple);
    RUN_TEST(test_dispatcher_gru_simple);
    RUN_TEST(test_dispatcher_elementwise);
    RUN_TEST(test_dispatcher_activations);
    RUN_TEST(test_dispatcher_normalization);
    RUN_TEST(test_dispatcher_math);
    RUN_TEST(test_dispatcher_reduction);
    RUN_TEST(test_dispatcher_manipulation);
    RUN_TEST(test_dispatcher_clip_default);
    RUN_TEST(test_dispatcher_advanced);
    RUN_TEST(test_dispatcher_resize_bilinear);
    RUN_TEST(test_dispatcher_conv1d);
    RUN_TEST(test_layers_direct_coverage);
    RUN_TEST(test_api_accessors);
    RUN_TEST(test_im2col_direct);
    RUN_TEST(test_im2col_large);
    
    printf("Results: %d Run, %d Passed, %d Failed\n", tests_run, tests_passed, tests_failed);
    return tests_failed;
}
