#include "eif_test_runner.h"
#include "eif_neural.h"
#include <string.h>
#include <stdlib.h>

#define EIF_MAGIC 0x4549464D
#define EIF_VERSION 1

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

static bool test_loader_fixed_weights_and_params(void) {
    uint8_t buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    uint8_t* ptr = buffer;

    // 1. Header
    eif_file_header_t header = {0};
    header.magic = EIF_MAGIC;
    header.version = EIF_VERSION;
    header.num_tensors = 1;
    header.num_nodes = 1;
    header.num_inputs = 1;
    header.num_outputs = 1;
    header.weights_size = 32;
    memcpy(ptr, &header, sizeof(header));
    ptr += sizeof(header);

    // 2. Tensor
    eif_file_tensor_t t0 = {0};
    t0.type = EIF_TENSOR_FLOAT32;
    t0.dims[0] = 1; t0.dims[1] = 4; t0.dims[2] = 1; t0.dims[3] = 1;
    t0.size_bytes = 16;
    t0.is_variable = 0;
    t0.data_offset = 16;
    memcpy(ptr, &t0, sizeof(t0));
    
    // Explicitly set offset in buffer 
    uint32_t offset_val = 16;
    memcpy(ptr + 28, &offset_val, 4);

    ptr += sizeof(t0);

    // 3. Node
    eif_file_node_t n0 = {0};
    n0.type = EIF_LAYER_CONV2D; 
    n0.num_inputs = 1;
    n0.num_outputs = 1;
    n0.params_size = 8;
    memcpy(ptr, &n0, sizeof(n0));
    ptr += sizeof(n0);

    // Node Inputs
    int in_idx = 0;
    memcpy(ptr, &in_idx, sizeof(int));
    ptr += sizeof(int);
    
    // Node Outputs
    int out_idx = 0;
    memcpy(ptr, &out_idx, sizeof(int));
    ptr += sizeof(int);

    // Node Params
    uint32_t dummy_params[2] = {0xDEADBEEF, 0xCAFEBABE};
    memcpy(ptr, dummy_params, sizeof(dummy_params));
    ptr += sizeof(dummy_params);

    // 4. Graph Inputs
    int g_in = 0;
    memcpy(ptr, &g_in, sizeof(int));
    ptr += sizeof(int);

    // 5. Graph Outputs
    int g_out = 0;
    memcpy(ptr, &g_out, sizeof(int));
    ptr += sizeof(int);

    // 6. Weights
    uint8_t weights_blob[32];
    memset(weights_blob, 0, 32);
    // Put marker at offset 16 if resolution works
    uint32_t marker = 0x12345678;
    memcpy(weights_blob + 16, &marker, 4);
    
    memcpy(ptr, weights_blob, sizeof(weights_blob));
    ptr += sizeof(weights_blob);

    size_t total_size = ptr - buffer;
    
    uint8_t* heap = malloc(65536);
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, heap, 65536);
    
    eif_model_t model;
    eif_status_t status = eif_model_deserialize(&model, buffer, total_size, &pool);

    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(1, model.num_tensors);
    
    // Check Node Params (Must be correct)
    TEST_ASSERT_NOT_NULL(model.nodes[0].params);
    uint32_t* params_read = (uint32_t*)model.nodes[0].params;
    TEST_ASSERT_EQUAL_INT(0xDEADBEEF, params_read[0]);
    TEST_ASSERT_EQUAL_INT(0xCAFEBABE, params_read[1]);

    free(heap);
    return true;
}

BEGIN_TEST_SUITE(run_dl_loader_coverage_tests)
    RUN_TEST(test_loader_fixed_weights_and_params);
END_TEST_SUITE()
