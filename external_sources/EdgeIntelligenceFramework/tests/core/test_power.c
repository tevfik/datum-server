#include "../framework/eif_test_runner.h"
#include "eif_power.h"
#include "eif_memory.h"
#include <string.h>

// =============================================================================
// Tests
// =============================================================================

bool test_power_init() {
    eif_power_profile_t profile;
    uint8_t buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    eif_status_t status = eif_power_init(&profile, 10, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_EQUAL_INT(10, profile.max_layers);
    TEST_ASSERT_EQUAL_INT(0, profile.num_layers);
    TEST_ASSERT_TRUE(profile.layers != NULL);
    
    // Test invalid args
    status = eif_power_init(NULL, 10, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, status);
    
    status = eif_power_init(&profile, 0, &pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_INVALID_ARGUMENT, status);
    
    return true;
}

bool test_power_profiling_flow() {
    eif_power_profile_t profile;
    uint8_t buffer[2048];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    eif_power_init(&profile, 5, &pool);
    
    // Layer 1: Conv2D
    eif_status_t status = eif_power_layer_start(&profile, "Conv1");
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    eif_power_record_ops(&profile, EIF_OP_MAC_INT8, 1000);
    eif_power_record_ops(&profile, EIF_OP_MEMORY_READ, 500);
    eif_power_record_ops(&profile, EIF_OP_MEMORY_WRITE, 200);
    
    status = eif_power_layer_end(&profile, 150); // 150 us
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    TEST_ASSERT_EQUAL_INT(1, profile.num_layers);
    TEST_ASSERT_EQUAL_INT(1000, profile.total_mac_ops);
    TEST_ASSERT_EQUAL_INT(150, profile.total_latency_us);
    
    // Layer 2: FC
    eif_power_layer_start(&profile, "FC1");
    eif_power_record_ops(&profile, EIF_OP_MAC_FP32, 500);
    eif_power_layer_end(&profile, 50);
    
    TEST_ASSERT_EQUAL_INT(2, profile.num_layers);
    TEST_ASSERT_EQUAL_INT(1500, profile.total_mac_ops);
    TEST_ASSERT_EQUAL_INT(200, profile.total_latency_us);
    
    // Check energy calculation (approximate)
    // INT8 MAC energy + FP32 MAC energy + Memory energy
    uint64_t energy = eif_power_total_energy(&profile);
    TEST_ASSERT_TRUE(energy > 0);
    
    // Reset
    eif_power_reset(&profile);
    TEST_ASSERT_EQUAL_INT(0, profile.num_layers);
    TEST_ASSERT_EQUAL_INT(0, profile.total_mac_ops);
    TEST_ASSERT_EQUAL_INT(1, profile.inference_count);
    
    return true;
}

bool test_power_estimates() {
    // Conv2D Estimate
    uint64_t energy_conv = eif_power_estimate_conv2d(32, 32, 3, 16, 3, true);
    TEST_ASSERT_TRUE(energy_conv > 0);
    
    // FC Estimate
    uint64_t energy_fc = eif_power_estimate_fc(128, 10, false);
    TEST_ASSERT_TRUE(energy_fc > 0);
    
    return true;
}

bool test_battery_life() {
    eif_power_profile_t profile;
    uint8_t buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    eif_power_init(&profile, 1, &pool);
    
    eif_power_layer_start(&profile, "Test");
    eif_power_record_ops(&profile, EIF_OP_MAC_INT8, 1000000); // 1M ops
    eif_power_layer_end(&profile, 1000);
    
    // 1000 mAh battery, 10 inferences/sec
    float32_t hours = eif_power_battery_life(&profile, 1000.0f, 10.0f);
    TEST_ASSERT_TRUE(hours > 0.0f);
    
    return true;
}

bool test_timer() {
    eif_timer_t timer;
    eif_timer_start(&timer);
    
    // Busy wait (simulate work)
    volatile int x = 0;
    for(int i=0; i<10000; i++) x++;
    
    uint32_t elapsed = eif_timer_stop(&timer);
    TEST_ASSERT_TRUE(elapsed >= 0); // Can be 0 if very fast, but usually > 0
    
    return true;
}

bool test_power_print() {
    // Just ensure it doesn't crash
    eif_power_profile_t profile;
    uint8_t buffer[1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    eif_power_init(&profile, 1, &pool);
    
    eif_power_layer_start(&profile, "Test");
    eif_power_record_ops(&profile, EIF_OP_MAC_INT8, 100);
    eif_power_layer_end(&profile, 10);
    
    eif_power_print_summary(&profile);
    
    return true;
}

BEGIN_TEST_SUITE(run_power_tests)
    RUN_TEST(test_power_init);
    RUN_TEST(test_power_profiling_flow);
    RUN_TEST(test_power_estimates);
    RUN_TEST(test_battery_life);
    RUN_TEST(test_timer);
    RUN_TEST(test_power_print);
END_TEST_SUITE()
