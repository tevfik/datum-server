#include "../framework/eif_test_runner.h"
#include "eif_memory.h"

// =============================================================================
// Tests
// =============================================================================

bool test_memory_invalid_args() {
    eif_memory_pool_t pool;
    uint8_t buffer[100];
    
    // Init invalid
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_memory_pool_init(NULL, buffer, 100));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_memory_pool_init(&pool, NULL, 100));
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, eif_memory_pool_init(&pool, buffer, 0));
    
    // Alloc invalid
    eif_memory_pool_init(&pool, buffer, 100);
    TEST_ASSERT_TRUE(eif_memory_alloc(NULL, 10, 4) == NULL);
    TEST_ASSERT_TRUE(eif_memory_alloc(&pool, 0, 4) == NULL);
    
    return true;
}

bool test_memory_available() {
    eif_memory_pool_t pool;
    uint8_t buffer[100];
    eif_memory_pool_init(&pool, buffer, 100);
    
    TEST_ASSERT_EQUAL_INT(100, eif_memory_available(&pool));
    TEST_ASSERT_EQUAL_INT(0, eif_memory_available(NULL));
    
    eif_memory_alloc(&pool, 10, 1);
    TEST_ASSERT_EQUAL_INT(90, eif_memory_available(&pool));
    
    return true;
}

bool test_memory_scratch() {
    eif_memory_pool_t pool;
    uint8_t buffer[100];
    eif_memory_pool_init(&pool, buffer, 100);
    
    void* p1 = eif_memory_alloc(&pool, 10, 1);
    TEST_ASSERT_TRUE(p1 != NULL);
    TEST_ASSERT_EQUAL_INT(10, pool.used);
    
    // Mark
    eif_memory_mark_t mark = eif_memory_mark(&pool);
    TEST_ASSERT_EQUAL_INT(10, mark.used_at_mark);
    
    // Alloc scratch
    void* p2 = eif_memory_alloc(&pool, 20, 1);
    TEST_ASSERT_TRUE(p2 != NULL);
    TEST_ASSERT_EQUAL_INT(30, pool.used);
    
    // Restore
    eif_memory_restore(&pool, mark);
    TEST_ASSERT_EQUAL_INT(10, pool.used);
    
    // Alloc again (should reuse space)
    void* p3 = eif_memory_alloc(&pool, 20, 1);
    TEST_ASSERT_TRUE(p3 == p2); // Should be same address
    
    // Invalid restore
    eif_memory_restore(NULL, mark); // Should not crash
    
    return true;
}

bool test_memory_stats() {
    eif_memory_pool_t pool;
    uint8_t buffer[100];
    eif_memory_pool_init(&pool, buffer, 100);
    
    eif_memory_alloc(&pool, 10, 1);
    
    eif_memory_stats_t stats;
    eif_memory_get_stats(&pool, &stats);
    
    TEST_ASSERT_EQUAL_INT(100, stats.total_size);
    TEST_ASSERT_EQUAL_INT(10, stats.used);
    TEST_ASSERT_EQUAL_INT(90, stats.available);
    TEST_ASSERT_EQUAL_INT(10, stats.peak);
    TEST_ASSERT_EQUAL_INT(1, stats.alloc_count);
    TEST_ASSERT_TRUE(stats.utilization > 0.0f);
    
    // Peak
    TEST_ASSERT_EQUAL_INT(10, eif_memory_peak(&pool));
    
    // Reset peak
    eif_memory_reset_peak(&pool);
    TEST_ASSERT_EQUAL_INT(10, eif_memory_peak(&pool)); // Peak resets to current used
    
    // Invalid args
    eif_memory_get_stats(NULL, &stats);
    eif_memory_get_stats(&pool, NULL);
    TEST_ASSERT_EQUAL_INT(0, eif_memory_peak(NULL));
    eif_memory_reset_peak(NULL);
    
    return true;
}

BEGIN_TEST_SUITE(run_memory_coverage_tests)
    RUN_TEST(test_memory_invalid_args);
    RUN_TEST(test_memory_available);
    RUN_TEST(test_memory_scratch);
    RUN_TEST(test_memory_stats);
END_TEST_SUITE()
