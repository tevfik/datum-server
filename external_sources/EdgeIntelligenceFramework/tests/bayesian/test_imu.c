#include "../framework/eif_test_runner.h"
#include "eif_imu.h"
#include "eif_generic.h"
#include <math.h>
#include <string.h>

// =============================================================================
// Helper Functions
// =============================================================================

static bool setup_imu(eif_imu_t* imu, eif_memory_pool_t* pool, uint8_t* buffer, size_t size) {
    eif_memory_pool_init(pool, buffer, size);
    
    eif_imu_config_t config;
    eif_imu_default_config(&config);
    
    // Set reference location (San Francisco)
    config.ref_latitude = 37.7749;
    config.ref_longitude = -122.4194;
    config.ref_altitude = 0.0f;
    
    eif_status_t status = eif_imu_init(imu, &config, pool);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // Manually set ref_set to true to use the configured reference
    // Otherwise the first GPS update will reset the reference to the current location
    imu->ref_set = true;
    return true;
}

// =============================================================================
// Tests
// =============================================================================

bool test_imu_init() {
    eif_imu_t imu;
    uint8_t buffer[8192]; // Need enough for 15x15 matrices
    eif_memory_pool_t pool;
    
    TEST_ASSERT_TRUE(setup_imu(&imu, &pool, buffer, sizeof(buffer)));
    
    TEST_ASSERT_TRUE(imu.initialized);
    TEST_ASSERT_TRUE(imu.ref_set);
    
    // Check initial state (quaternion should be [1, 0, 0, 0])
    TEST_ASSERT_EQUAL_FLOAT(1.0f, imu.x[6], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, imu.x[7], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, imu.x[8], 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, imu.x[9], 0.001f);
    
    return true;
}

bool test_imu_prediction_stationary() {
    eif_imu_t imu;
    uint8_t buffer[8192];
    eif_memory_pool_t pool;
    
    TEST_ASSERT_TRUE(setup_imu(&imu, &pool, buffer, sizeof(buffer)));
    
    // Stationary IMU reading (gravity on Z axis, no rotation)
    float32_t accel[3] = {0.0f, 0.0f, -9.81f}; // NED frame: Z is down, so gravity is -g? Or +g?
    // Usually accelerometer measures proper acceleration. Stationary on table: +1g upwards (reaction force).
    // If Z is down, reaction force is -1g (up). 
    // Let's assume standard NED: Z down. Gravity vector is [0, 0, g].
    // Accelerometer measures f = a - g. Stationary: a=0 -> f = -g = [0, 0, -9.81].
    
    float32_t gyro[3] = {0.0f, 0.0f, 0.0f};
    float32_t dt = 0.01f; // 100Hz
    
    // Run for 1 second
    for (int i = 0; i < 100; i++) {
        eif_status_t status = eif_imu_update_sensors(&imu, accel, gyro, dt);
        TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    }
    
    // Velocity should remain close to 0
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, imu.x[3]); // Vn
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, imu.x[4]); // Ve
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, imu.x[5]); // Vd
    
    return true;
}

bool test_imu_gps_correction() {
    eif_imu_t imu;
    uint8_t buffer[8192];
    eif_memory_pool_t pool;
    
    TEST_ASSERT_TRUE(setup_imu(&imu, &pool, buffer, sizeof(buffer)));
    
    // Simulate moving North at 10m/s
    // Initial position: 0,0,0
    
    // GPS update at t=1s indicating we moved 10m North
    // 1 degree lat ~ 111km. 10m is approx 9e-5 degrees.
    float64_t new_lat = 37.7749 + (10.0 / 111132.0);
    float64_t new_lon = -122.4194;
    float32_t new_alt = 0.0f;
    float32_t vel_ned[3] = {10.0f, 0.0f, 0.0f};
    
    eif_status_t status = eif_imu_update_gps(&imu, new_lat, new_lon, new_alt, vel_ned);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    // State should reflect position/velocity update
    // Note: UKF is complex, exact value depends on gains. Just check direction.
    
    // Position N should be positive
    TEST_ASSERT_TRUE(imu.x[0] > 0.1f);
    
    // Velocity N should be positive
    TEST_ASSERT_TRUE(imu.x[3] > 0.1f);
    
    return true;
}

bool test_imu_baro_update() {
    eif_imu_t imu;
    uint8_t buffer[8192];
    eif_memory_pool_t pool;
    
    TEST_ASSERT_TRUE(setup_imu(&imu, &pool, buffer, sizeof(buffer)));
    
    // Initial altitude is 0 (from setup_imu)
    // Barometer says 10m
    float32_t baro_alt = 10.0f;
    
    eif_status_t status = eif_imu_update_baro(&imu, baro_alt);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    TEST_ASSERT_TRUE(imu.baro_valid);
    
    // Altitude (NED z) should be negative (Up is -Z)
    // Measurement is 10m. NED Z should move towards -10.
    TEST_ASSERT_TRUE(imu.x[2] < 0.0f);
    
    return true;
}

bool test_imu_getters() {
    eif_imu_t imu;
    uint8_t buffer[8192];
    eif_memory_pool_t pool;
    
    TEST_ASSERT_TRUE(setup_imu(&imu, &pool, buffer, sizeof(buffer)));
    
    eif_pose_t pose;
    eif_status_t status = eif_imu_get_pose(&imu, &pose);
    TEST_ASSERT_EQUAL_INT(EIF_STATUS_OK, status);
    
    eif_vec3_t pos;
    eif_imu_get_position(&imu, &pos);
    TEST_ASSERT_EQUAL_FLOAT(pose.position.x, pos.x, 0.001f);
    
    eif_vec3_t vel;
    eif_imu_get_velocity(&imu, &vel);
    TEST_ASSERT_EQUAL_FLOAT(pose.velocity.x, vel.x, 0.001f);
    
    eif_quat_t q;
    eif_imu_get_quaternion(&imu, &q);
    TEST_ASSERT_EQUAL_FLOAT(pose.quaternion.w, q.w, 0.001f);
    
    eif_vec3_t euler;
    eif_imu_get_euler(&imu, &euler);
    TEST_ASSERT_EQUAL_FLOAT(pose.euler.x, euler.x, 0.001f);
    
    eif_vec3_t g_bias;
    float32_t a_bias;
    eif_imu_get_biases(&imu, &g_bias, &a_bias);
    
    return true;
}

bool test_quaternion_conversions() {
    eif_vec3_t euler = {0.1f, 0.2f, 0.3f}; // Roll, Pitch, Yaw
    eif_quat_t q;
    
    eif_euler_to_quat(&euler, &q);
    
    eif_vec3_t euler_out;
    eif_quat_to_euler(&q, &euler_out);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, euler.x, euler_out.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, euler.y, euler_out.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, euler.z, euler_out.z);
    
    return true;
}

BEGIN_TEST_SUITE(run_imu_tests)
    RUN_TEST(test_imu_init);
    RUN_TEST(test_imu_prediction_stationary);
    RUN_TEST(test_imu_gps_correction);
    RUN_TEST(test_imu_baro_update);
    RUN_TEST(test_imu_getters);
    RUN_TEST(test_quaternion_conversions);
END_TEST_SUITE()
