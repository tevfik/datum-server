#include "eif_neural.h"
#include "eif_test_runner.h"
#include <string.h>
#include <stdlib.h>

static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;

void setup_serialization() {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

// Helper structs to match loader
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t num_tensors;
    uint32_t num_nodes;
    uint32_t num_inputs;
    uint32_t num_outputs;
    uint32_t weights_size;
} eif_file_header_t;

typedef struct {
    uint32_t type;
    int32_t dims[4];
    uint32_t size_bytes;
    uint32_t is_variable;
    uint32_t data_offset;
} eif_file_tensor_t;

typedef struct {
    uint32_t type;
    uint32_t num_inputs;
    uint32_t num_outputs;
    uint32_t params_size;
} eif_file_node_t;

bool test_model_deserialize() {
    setup_serialization();
    
    // Construct a binary model in memory
    uint8_t buffer[1024];
    uint8_t* ptr = buffer;
    
    // Header
    eif_file_header_t header = {
        .magic = 0x4549464D,
        .version = 1,
        .num_tensors = 2,
        .num_nodes = 1,
        .num_inputs = 1,
        .num_outputs = 1,
        .weights_size = 0
    };
    memcpy(ptr, &header, sizeof(header));
    ptr += sizeof(header);
    
    // Tensor 0 (Input)
    eif_file_tensor_t t0 = {
        .type = EIF_TENSOR_FLOAT32,
        .dims = {1, 10, 1, 1},
        .size_bytes = 10 * sizeof(float),
        .is_variable = 1,
        .data_offset = 0xFFFFFFFF
    };
    memcpy(ptr, &t0, sizeof(t0));
    ptr += sizeof(t0);
    
    // Tensor 1 (Output)
    eif_file_tensor_t t1 = {
        .type = EIF_TENSOR_FLOAT32,
        .dims = {1, 10, 1, 1},
        .size_bytes = 10 * sizeof(float),
        .is_variable = 1,
        .data_offset = 0xFFFFFFFF
    };
    memcpy(ptr, &t1, sizeof(t1));
    ptr += sizeof(t1);
    
    // Node 0 (Relu)
    eif_file_node_t n0 = {
        .type = EIF_LAYER_RELU,
        .num_inputs = 1,
        .num_outputs = 1,
        .params_size = 0
    };
    memcpy(ptr, &n0, sizeof(n0));
    ptr += sizeof(n0);
    
    // Node Inputs
    int in_idx = 0;
    memcpy(ptr, &in_idx, sizeof(int));
    ptr += sizeof(int);
    
    // Node Outputs
    int out_idx = 1;
    memcpy(ptr, &out_idx, sizeof(int));
    ptr += sizeof(int);
    
    // Graph Inputs
    int g_in = 0;
    memcpy(ptr, &g_in, sizeof(int));
    ptr += sizeof(int);
    
    // Graph Outputs
    int g_out = 1;
    memcpy(ptr, &g_out, sizeof(int));
    ptr += sizeof(int);
    
    // Weights (Empty)
    
    size_t size = ptr - buffer;
    
    eif_model_t model;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_model_deserialize(&model, buffer, size, &pool));
    
    TEST_ASSERT_EQUAL_INT(2, model.num_tensors);
    TEST_ASSERT_EQUAL_INT(1, model.num_nodes);
    TEST_ASSERT_EQUAL_INT(EIF_LAYER_RELU, model.nodes[0].type);
    TEST_ASSERT_EQUAL_INT(0, model.nodes[0].input_indices[0]);
    TEST_ASSERT_EQUAL_INT(1, model.nodes[0].output_indices[0]);
    
    return true;
}

BEGIN_TEST_SUITE(run_serialization_tests)
    RUN_TEST(test_model_deserialize);
END_TEST_SUITE()
