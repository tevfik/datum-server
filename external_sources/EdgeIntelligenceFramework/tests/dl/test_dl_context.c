/**
 * @file test_dl_context.c
 * @brief Unit tests for DL Context and Graph Execution
 * 
 * Tests:
 * - Context initialization
 * - Memory planning / liveness analysis
 * - Graph execution
 * - Thread safety
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "eif_nn_context.h"
#include "eif_nn_graph.h"
#include "eif_neural.h"
#include "eif_memory.h"

// ============================================================================
// Test Utilities
// ============================================================================

static uint8_t test_pool_buffer[256 * 1024];
static eif_memory_pool_t test_pool;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_NE(a, b, msg) TEST_ASSERT((a) != (b), msg)
#define TEST_ASSERT_OK(status) TEST_ASSERT((status) == EIF_STATUS_OK, "Expected EIF_STATUS_OK")

static void test_begin(const char* name) {
    printf("  Testing: %s... ", name);
    fflush(stdout);
}

static void test_pass(void) {
    printf("PASS\n");
    tests_passed++;
}

// ============================================================================
// Create Simple Test Model (Dense -> ReLU -> Dense)
// ============================================================================

static float test_weights_1[4 * 8];   // 4 inputs -> 8 hidden
static float test_biases_1[8];
static float test_weights_2[8 * 2];   // 8 hidden -> 2 outputs
static float test_biases_2[2];

static eif_tensor_t test_tensors[7];
static int test_input_indices[] = {0};
static int test_output_indices[] = {6};
static eif_layer_node_t test_nodes[3];
static eif_layer_param_t test_params[3];

static int node0_inputs[] = {0, 1, 2};
static int node0_outputs[] = {3};
static int node1_inputs[] = {3};
static int node1_outputs[] = {4};
static int node2_inputs[] = {4, 5, 2};
static int node2_outputs[] = {6};

static void init_test_model(eif_model_t* model) {
    // Initialize weights with simple values
    for (int i = 0; i < 4 * 8; i++) test_weights_1[i] = 0.1f;
    for (int i = 0; i < 8; i++) test_biases_1[i] = 0.01f;
    for (int i = 0; i < 8 * 2; i++) test_weights_2[i] = 0.1f;
    for (int i = 0; i < 2; i++) test_biases_2[i] = 0.01f;
    
    // Tensor 0: Input (1, 4)
    test_tensors[0] = (eif_tensor_t){
        .type = EIF_TENSOR_FLOAT32,
        .dims = {1, 4, 1, 1}, .num_dims = 2,
        .size_bytes = 4 * sizeof(float),
        .is_variable = true
    };
    
    // Tensor 1: Weights 1 (constant)
    test_tensors[1] = (eif_tensor_t){
        .type = EIF_TENSOR_FLOAT32,
        .dims = {4, 8, 1, 1}, .num_dims = 2,
        .data = test_weights_1,
        .size_bytes = 4 * 8 * sizeof(float),
        .is_variable = false
    };
    
    // Tensor 2: Biases 1 (constant)
    test_tensors[2] = (eif_tensor_t){
        .type = EIF_TENSOR_FLOAT32,
        .dims = {8, 1, 1, 1}, .num_dims = 1,
        .data = test_biases_1,
        .size_bytes = 8 * sizeof(float),
        .is_variable = false
    };
    
    // Tensor 3: Hidden (1, 8) - after dense1
    test_tensors[3] = (eif_tensor_t){
        .type = EIF_TENSOR_FLOAT32,
        .dims = {1, 8, 1, 1}, .num_dims = 2,
        .size_bytes = 8 * sizeof(float),
        .is_variable = true
    };
    
    // Tensor 4: After ReLU (1, 8)
    test_tensors[4] = (eif_tensor_t){
        .type = EIF_TENSOR_FLOAT32,
        .dims = {1, 8, 1, 1}, .num_dims = 2,
        .size_bytes = 8 * sizeof(float),
        .is_variable = true
    };
    
    // Tensor 5: Weights 2 (constant)
    test_tensors[5] = (eif_tensor_t){
        .type = EIF_TENSOR_FLOAT32,
        .dims = {8, 2, 1, 1}, .num_dims = 2,
        .data = test_weights_2,
        .size_bytes = 8 * 2 * sizeof(float),
        .is_variable = false
    };
    
    // Tensor 6: Output (1, 2)
    test_tensors[6] = (eif_tensor_t){
        .type = EIF_TENSOR_FLOAT32,
        .dims = {1, 2, 1, 1}, .num_dims = 2,
        .size_bytes = 2 * sizeof(float),
        .is_variable = true
    };
    
    // Node 0: Dense (inputs: 0,1,2 -> output: 3)
    test_params[0].dense.units = 8;
    
    test_nodes[0] = (eif_layer_node_t){
        .type = EIF_LAYER_DENSE,
        .input_indices = node0_inputs,
        .num_inputs = 3,
        .output_indices = node0_outputs,
        .num_outputs = 1,
        .params = &test_params[0]
    };
    
    // Node 1: ReLU (input: 3 -> output: 4)
    test_nodes[1] = (eif_layer_node_t){
        .type = EIF_LAYER_RELU,
        .input_indices = node1_inputs,
        .num_inputs = 1,
        .output_indices = node1_outputs,
        .num_outputs = 1,
        .params = NULL
    };
    
    // Node 2: Dense (inputs: 4,5,2 -> output: 6)
    test_params[2].dense.units = 2;
    
    test_nodes[2] = (eif_layer_node_t){
        .type = EIF_LAYER_DENSE,
        .input_indices = node2_inputs,  // Note: reusing biases tensor 2 for simplicity
        .num_inputs = 3,
        .output_indices = node2_outputs,
        .num_outputs = 1,
        .params = &test_params[2]
    };
    
    // Model
    model->nodes = test_nodes;
    model->num_nodes = 3;
    model->tensors = test_tensors;
    model->num_tensors = 7;
    model->input_tensor_indices = test_input_indices;
    model->num_inputs = 1;
    model->output_tensor_indices = test_output_indices;
    model->num_outputs = 1;
    model->activation_arena_size = 0;  // Will be computed
    model->scratch_size = 1024;
    model->persistent_size = 0;
}

// ============================================================================
// Test: Context Initialization
// ============================================================================

static void test_context_init(void) {
    test_begin("Context Initialization");
    
    eif_model_t model = {0};
    init_test_model(&model);
    
    eif_dl_context_t ctx;
    eif_status_t status = eif_dl_context_init(&ctx, &model, &test_pool, NULL);
    
    TEST_ASSERT_OK(status);
    TEST_ASSERT_NE(ctx.tensor_data, NULL, "tensor_data should be allocated");
    TEST_ASSERT_NE(ctx.arena.scratch_base, NULL, "scratch buffer should be allocated");
    TEST_ASSERT_EQ(ctx.model, &model, "model pointer should be set");
    
    test_pass();
}

// ============================================================================
// Test: Memory Requirements
// ============================================================================

static void test_memory_requirements(void) {
    test_begin("Memory Requirements");
    
    eif_model_t model = {0};
    init_test_model(&model);
    model.activation_arena_size = 1024;
    model.scratch_size = 512;
    model.persistent_size = 256;
    
    eif_dl_memory_req_t req;
    eif_status_t status = eif_dl_get_memory_requirements(&model, &req);
    
    TEST_ASSERT_OK(status);
    TEST_ASSERT_EQ(req.activation_bytes, 1024, "activation size mismatch");
    TEST_ASSERT_EQ(req.scratch_bytes, 512, "scratch size mismatch");
    TEST_ASSERT_EQ(req.persistent_bytes, 256, "persistent size mismatch");
    TEST_ASSERT(req.total_bytes > 0, "total should be > 0");
    
    test_pass();
}

// ============================================================================
// Test: Graph Execution (Forward Pass)
// ============================================================================

static void test_graph_execution(void) {
    test_begin("Graph Execution");
    
    eif_model_t model = {0};
    init_test_model(&model);
    
    eif_dl_context_t ctx;
    eif_status_t status = eif_dl_context_init(&ctx, &model, &test_pool, NULL);
    TEST_ASSERT_OK(status);
    
    // Set input
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    void* in_ptr = eif_dl_input_ptr(&ctx, 0);
    TEST_ASSERT_NE(in_ptr, NULL, "input ptr should not be NULL");
    memcpy(in_ptr, input, sizeof(input));
    
    // Execute
    status = eif_dl_invoke(&ctx);
    TEST_ASSERT_OK(status);
    
    // Check output
    const float* out_ptr = (const float*)eif_dl_output_ptr(&ctx, 0);
    TEST_ASSERT_NE(out_ptr, NULL, "output ptr should not be NULL");
    
    // Output should be non-zero after Dense->ReLU->Dense
    TEST_ASSERT(out_ptr[0] != 0.0f || out_ptr[1] != 0.0f, "output should be non-zero");
    
    test_pass();
}

// ============================================================================
// Test: Invoke with I/O Copy
// ============================================================================

static void test_invoke_io(void) {
    test_begin("Invoke with I/O Copy");
    
    eif_model_t model = {0};
    init_test_model(&model);
    
    eif_dl_context_t ctx;
    eif_status_t status = eif_dl_context_init(&ctx, &model, &test_pool, NULL);
    TEST_ASSERT_OK(status);
    
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[2] = {0};
    
    status = eif_dl_invoke_io(&ctx, input, sizeof(input), output, sizeof(output));
    TEST_ASSERT_OK(status);
    
    // Output should have values
    TEST_ASSERT(output[0] != 0.0f || output[1] != 0.0f, "output should be computed");
    
    test_pass();
}

// ============================================================================
// Test: Statistics Tracking
// ============================================================================

static void test_statistics(void) {
    test_begin("Statistics Tracking");
    
    eif_model_t model = {0};
    init_test_model(&model);
    
    eif_dl_context_t ctx;
    eif_status_t status = eif_dl_context_init(&ctx, &model, &test_pool, NULL);
    TEST_ASSERT_OK(status);
    
    eif_dl_reset_stats(&ctx);
    TEST_ASSERT_EQ(ctx.stats.invoke_count, 0, "invoke count should be 0");
    
    // Run inference 3 times
    for (int i = 0; i < 3; i++) {
        float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        void* in_ptr = eif_dl_input_ptr(&ctx, 0);
        memcpy(in_ptr, input, sizeof(input));
        eif_dl_invoke(&ctx);
    }
    
    TEST_ASSERT_EQ(ctx.stats.invoke_count, 3, "invoke count should be 3");
    
    test_pass();
}

// ============================================================================
// Test: State Reset
// ============================================================================

static void test_state_reset(void) {
    test_begin("State Reset");
    
    eif_model_t model = {0};
    init_test_model(&model);
    model.persistent_size = 64;  // Enable persistent buffer
    
    eif_dl_context_t ctx;
    eif_status_t status = eif_dl_context_init(&ctx, &model, &test_pool, NULL);
    TEST_ASSERT_OK(status);
    TEST_ASSERT_NE(ctx.arena.persistent_base, NULL, "persistent should be allocated");
    
    // Write something to persistent buffer
    memset(ctx.arena.persistent_base, 0xFF, 64);
    
    // Reset
    status = eif_dl_reset_state(&ctx);
    TEST_ASSERT_OK(status);
    
    // Verify zeroed
    uint8_t sum = 0;
    for (int i = 0; i < 64; i++) sum |= ctx.arena.persistent_base[i];
    TEST_ASSERT_EQ(sum, 0, "persistent buffer should be zeroed");
    
    test_pass();
}

// ============================================================================
// Test: Invalid Arguments
// ============================================================================

static void test_invalid_args(void) {
    test_begin("Invalid Arguments");
    
    eif_dl_context_t ctx;
    eif_model_t model = {0};
    
    // NULL context
    eif_status_t status = eif_dl_context_init(NULL, &model, &test_pool, NULL);
    TEST_ASSERT_NE(status, EIF_STATUS_OK, "should fail with NULL ctx");
    
    // NULL model
    status = eif_dl_context_init(&ctx, NULL, &test_pool, NULL);
    TEST_ASSERT_NE(status, EIF_STATUS_OK, "should fail with NULL model");
    
    // NULL pool
    status = eif_dl_context_init(&ctx, &model, NULL, NULL);
    TEST_ASSERT_NE(status, EIF_STATUS_OK, "should fail with NULL pool");
    
    // NULL invoke
    status = eif_dl_invoke(NULL);
    TEST_ASSERT_NE(status, EIF_STATUS_OK, "should fail with NULL invoke");
    
    test_pass();
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         DL Context & Graph Execution Tests                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    eif_memory_pool_init(&test_pool, test_pool_buffer, sizeof(test_pool_buffer));
    
    test_context_init();
    test_memory_requirements();
    test_graph_execution();
    test_invoke_io();
    test_statistics();
    test_state_reset();
    test_invalid_args();
    
    printf("\n");
    printf("────────────────────────────────────────────────────────────────\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("────────────────────────────────────────────────────────────────\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
