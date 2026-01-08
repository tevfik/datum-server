#include "eif_bayesian.h"
#include "eif_test_runner.h"
#include <math.h>

static uint8_t pool_buffer[1024 * 64]; // 64KB for UKF
static eif_memory_pool_t pool;

void setup_ukf() {
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
}

bool test_ukf_slam_init() {
    setup_ukf();
    eif_ukf_slam_t slam;
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, eif_ukf_slam_init(&slam, 5, &pool));
    TEST_ASSERT_EQUAL_INT(5, slam.num_landmarks);
    TEST_ASSERT_EQUAL_INT(3 + 2*5, slam.state_dim);
    TEST_ASSERT_NOT_NULL(slam.state);
    TEST_ASSERT_NOT_NULL(slam.P);
    TEST_ASSERT_NOT_NULL(slam.sigma_points);
    return true;
}

bool test_ukf_slam_predict() {
    setup_ukf();
    eif_ukf_slam_t slam;
    eif_ukf_slam_init(&slam, 1, &pool);
    
    // Move forward 1m
    // v=1, w=0, dt=1
    eif_ukf_slam_predict(&slam, 1.0f, 0.0f, 1.0f, 0.01f, 0.01f, &pool);
    
    // x should be approx 1.0
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, slam.state[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, slam.state[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, slam.state[2]);
    
    // Variance should increase
    TEST_ASSERT_TRUE(slam.P[0] > 0.001f);
    
    return true;
}

bool test_ukf_slam_update() {
    setup_ukf();
    eif_ukf_slam_t slam;
    eif_ukf_slam_init(&slam, 1, &pool);
    
    // Robot at 0,0,0
    // Landmark 0 at 2,0 (Range 2, Bearing 0)
    
    // Update
    eif_ukf_slam_update(&slam, 0, 2.0f, 0.0f, 0.1f, 0.1f, &pool);
    
    // Landmark state should be initialized to approx 2,0
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 2.0f, slam.state[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 0.0f, slam.state[4]);
    
    // Move robot to 1,0
    eif_ukf_slam_predict(&slam, 1.0f, 0.0f, 1.0f, 0.01f, 0.01f, &pool);
    // x ~ 1.0
    
    // Observe landmark again. Should be Range 1, Bearing 0 (since robot is at 1,0 and landmark at 2,0)
    eif_ukf_slam_update(&slam, 0, 1.0f, 0.0f, 0.1f, 0.1f, &pool);
    
    // Robot x should be reinforced around 1.0
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 1.0f, slam.state[0]);
    // Landmark x should be reinforced around 2.0
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 2.0f, slam.state[3]);
    
    return true;
}

BEGIN_TEST_SUITE(run_ukf_slam_tests)
    RUN_TEST(test_ukf_slam_init);
    RUN_TEST(test_ukf_slam_predict);
    RUN_TEST(test_ukf_slam_update);
END_TEST_SUITE()
