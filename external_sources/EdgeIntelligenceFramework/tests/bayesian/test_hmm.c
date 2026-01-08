#include "../framework/eif_test_runner.h"
#include "eif_bf_hmm.h"
#include <math.h>

// =============================================================================
// Tests
// =============================================================================

bool test_hmm_init() {
    eif_hmm_t hmm;
    uint8_t buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    eif_status_t status = eif_hmm_init(&hmm, 2, 3, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(2, hmm.N);
    TEST_ASSERT_EQUAL_INT(3, hmm.M);
    TEST_ASSERT_TRUE(hmm.A != NULL);
    TEST_ASSERT_TRUE(hmm.B != NULL);
    TEST_ASSERT_TRUE(hmm.pi != NULL);
    
    return true;
}

bool test_hmm_forward() {
    eif_hmm_t hmm;
    uint8_t buffer[2048];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    eif_hmm_init(&hmm, 2, 3, &pool);
    
    // Example: Wikipedia HMM example (Rainy/Sunny)
    // States: 0=Rainy, 1=Sunny
    // Obs: 0=Walk, 1=Shop, 2=Clean
    
    float32_t A[] = {0.7f, 0.3f, 0.4f, 0.6f}; // Rainy->Rainy 0.7, Rainy->Sunny 0.3...
    float32_t B[] = {0.1f, 0.4f, 0.5f, 0.6f, 0.3f, 0.1f}; // Rainy emits Walk 0.1...
    float32_t pi[] = {0.6f, 0.4f}; // Start Rainy 0.6
    
    eif_hmm_set_params(&hmm, A, B, pi);
    
    int obs[] = {0, 1, 2}; // Walk, Shop, Clean
    
    // Forward algorithm returns log probability
    float32_t log_prob = eif_hmm_forward(&hmm, obs, 3);
    
    // Manual calculation or reference check
    // Just check it's a reasonable negative number (log prob)
    TEST_ASSERT_TRUE(log_prob < 0.0f);
    TEST_ASSERT_TRUE(log_prob > -10.0f);
    
    return true;
}

bool test_hmm_viterbi() {
    eif_hmm_t hmm;
    uint8_t buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    eif_hmm_init(&hmm, 2, 3, &pool);
    
    // Same model
    float32_t A[] = {0.7f, 0.3f, 0.4f, 0.6f}; 
    float32_t B[] = {0.1f, 0.4f, 0.5f, 0.6f, 0.3f, 0.1f}; 
    float32_t pi[] = {0.6f, 0.4f}; 
    
    eif_hmm_set_params(&hmm, A, B, pi);
    
    int obs[] = {0, 1, 2}; // Walk, Shop, Clean
    int path[3];
    
    eif_status_t status = eif_hmm_viterbi(&hmm, obs, 3, path, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Check path validity
    TEST_ASSERT_TRUE(path[0] >= 0 && path[0] < 2);
    TEST_ASSERT_TRUE(path[1] >= 0 && path[1] < 2);
    TEST_ASSERT_TRUE(path[2] >= 0 && path[2] < 2);
    
    // For this specific example, let's see if we can predict the path.
    // Walk (0): P(Rainy)=0.6*0.1=0.06, P(Sunny)=0.4*0.6=0.24. Max=Sunny(1).
    // Shop (1): 
    // From Sunny(1): ->Rainy(0): 0.24*0.4*0.4 = 0.0384. ->Sunny(1): 0.24*0.6*0.3 = 0.0432. Max=Sunny(1).
    // Clean (2):
    // From Sunny(1): ->Rainy(0): 0.0432*0.4*0.5 = 0.00864. ->Sunny(1): 0.0432*0.6*0.1 = 0.00259. Max=Rainy(0).
    // Path: Sunny, Rainy, Rainy -> 1, 0, 0.
    
    TEST_ASSERT_EQUAL_INT(1, path[0]);
    TEST_ASSERT_EQUAL_INT(0, path[1]);
    TEST_ASSERT_EQUAL_INT(0, path[2]);
    
    return true;
}

BEGIN_TEST_SUITE(run_hmm_tests)
    RUN_TEST(test_hmm_init);
    RUN_TEST(test_hmm_forward);
    RUN_TEST(test_hmm_viterbi);
END_TEST_SUITE()
