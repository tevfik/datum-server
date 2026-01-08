#include "../framework/eif_test_runner.h"
#include "eif_ml_pca.h"
#include "eif_utils.h"

#define TEST_ASSERT_EQUAL_PTR(expected, actual) TEST_ASSERT((expected) == (actual))
#define TEST_ASSERT_NOT_EQUAL(expected, actual) TEST_ASSERT((expected) != (actual))
#define TEST_ASSERT_NOT_NULL(ptr) TEST_ASSERT((ptr) != NULL)

static bool test_pca_constant_data(void) {
    eif_pca_t pca;
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Constant data: variance is 0. std should be 0, but code sets to 1.0.
    float32_t X[] = {1.0f, 1.0f, 1.0f, 1.0f}; // 2 samples, 2 features
    eif_pca_init(&pca, 2, 1, &pool);
    
    eif_status_t status = eif_pca_fit(&pca, X, 2, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Check std. Since struct is likely in header, we can access it.
    TEST_ASSERT_EQUAL_FLOAT(1.0f, pca.std[0], 0.0001f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, pca.std[1], 0.0001f);
    
    return true;
}

static bool test_pca_cleanup(void) {
    eif_pca_t pca;
    memset(&pca, 0, sizeof(eif_pca_t)); // Safe init
    
    eif_pca_cleanup(&pca); // Should be fine on empty
    eif_pca_cleanup(NULL); // Should return
    
    uint8_t pool_buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    eif_pca_init(&pca, 2, 1, &pool);
    
    // Check pointers are set
    TEST_ASSERT_NOT_NULL(pca.mean);
    
    // Cleanup
    eif_pca_cleanup(&pca);
    TEST_ASSERT_EQUAL_PTR(NULL, pca.mean);
    TEST_ASSERT_EQUAL_INT(0, pca.is_fitted);
    
    return true;
}

static bool test_pca_init_oom(void) {
    eif_pca_t pca;
    // Small pool: need enough for pool struct overhead but not enough for arrays
    // pca needs 4 allocations: mean(F), std(F), comp(C*F), var(C)
    // float32 size = 4 bytes.
    // If we give just enough for pool overhead?
    // eif_memory_pool_init uses the buffer.
    uint8_t pool_buffer[32]; 
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Request large features to ensure fail
    eif_status_t status = eif_pca_init(&pca, 100, 10, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OUT_OF_MEMORY, status);
    return true;
}

int run_pca_coverage_tests(void) {
    tests_failed = 0; // Reset for this suite
    RUN_TEST(test_pca_constant_data);
    RUN_TEST(test_pca_cleanup);
    RUN_TEST(test_pca_init_oom);
    return tests_failed;
}
