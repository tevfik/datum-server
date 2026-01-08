#include "eif_bayesian.h"
#include "eif_test_runner.h"
#include <math.h>

bool test_ekf_slam() {
    // Test EKF-SLAM
    // 1 Landmark at (10, 0)
    // Robot starts at (0, 0, 0)
    // Move 1m forward (v=1, dt=1, w=0) -> Robot at (1, 0, 0)
    // Measure landmark: Range = 9, Bearing = 0
    
    eif_ekf_slam_t slam;
    uint8_t pool_buffer[4096];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_ekf_slam_init(&slam, 1, &pool);
    
    // 1. Predict
    eif_ekf_slam_predict(&slam, 1.0f, 0.0f, 1.0f, 0.1f, 0.01f);
    
    // Check state: (1, 0, 0)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, slam.state[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, slam.state[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, slam.state[2]);
    
    // 2. Update
    // Landmark is at (10, 0). Robot at (1, 0).
    // Expected Range = 9.0. Bearing = 0.
    // Let's give slightly noisy measurement: Range = 9.1, Bearing = 0.0
    
    eif_ekf_slam_update(&slam, 0, 9.1f, 0.0f, 0.1f, 0.01f, &pool);
    
    // Landmark state should be initialized near (1 + 9.1, 0) = (10.1, 0)
    // Robot state might be pulled slightly if correlation is strong, but initially landmark uncertainty is huge.
    // So landmark position will be updated to match measurement given robot pose.
    // L_x = R_x + range * cos(R_th + bearing) = 1 + 9.1 * 1 = 10.1
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.1f, slam.state[3]); // L_x
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, slam.state[4]);  // L_y
    
    // 3. Move again: 1m forward -> (2, 0, 0)
    eif_ekf_slam_predict(&slam, 1.0f, 0.0f, 1.0f, 0.1f, 0.01f);
    
    // 4. Update again
    // Robot at (2, 0). Landmark at (10, 0).
    // Expected Range = 8.0.
    // Measurement: Range = 7.9 (Correcting the previous over-shoot of 0.1)
    
    eif_ekf_slam_update(&slam, 0, 7.9f, 0.0f, 0.1f, 0.01f, &pool);
    
    // Now both robot and landmark should converge towards truth.
    // Truth: Robot (2,0), Landmark (10,0).
    // Previous estimate: Robot (2,0), Landmark (10.1, 0).
    // Measurement says dist is 7.9.
    // Current est dist is 10.1 - 2 = 8.1.
    // Innovation = 7.9 - 8.1 = -0.2.
    // This negative innovation should pull Landmark x down and Robot x up (slightly).
    
    TEST_ASSERT_TRUE(slam.state[3] < 10.1f); // Landmark x should decrease
    
    return true;
}

BEGIN_TEST_SUITE(run_slam_tests)
    RUN_TEST(test_ekf_slam);
END_TEST_SUITE()
