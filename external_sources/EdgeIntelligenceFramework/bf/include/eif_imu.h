/**
 * @file eif_imu.h
 * @brief Inertial Measurement Unit (IMU) Fusion using UKF
 * 
 * High-level API for fusing IMU (accelerometer + gyroscope), GPS, and
 * barometer data using an Unscented Kalman Filter (UKF) for robust
 * nonlinear state estimation.
 * 
 * Features:
 * - 15-state UKF (position, velocity, quaternion, gyro bias, accel bias)
 * - IMU dead reckoning with bias estimation
 * - GPS position/velocity correction
 * - Barometric altitude aiding
 * - Full pose output (position, velocity, attitude)
 */

#ifndef EIF_IMU_H
#define EIF_IMU_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constants
// =============================================================================

#define EIF_IMU_STATE_DIM  15  // State dimension

// =============================================================================
// Data Structures
// =============================================================================

/**
 * @brief 3D vector
 */
typedef struct {
    float32_t x;
    float32_t y;
    float32_t z;
} eif_vec3_t;

/**
 * @brief Quaternion (w, x, y, z)
 */
typedef struct {
    float32_t w;
    float32_t x;
    float32_t y;
    float32_t z;
} eif_quat_t;

/**
 * @brief Full pose state
 */
typedef struct {
    eif_vec3_t position;    // NED position (meters)
    eif_vec3_t velocity;    // NED velocity (m/s)
    eif_quat_t quaternion;  // Body-to-NED quaternion
    eif_vec3_t euler;       // Roll, pitch, yaw (radians)
} eif_pose_t;

/**
 * @brief IMU fusion configuration
 */
typedef struct {
    // Process noise (prediction uncertainty)
    float32_t position_noise;      // Position process noise
    float32_t velocity_noise;      // Velocity process noise
    float32_t attitude_noise;      // Attitude process noise
    float32_t gyro_bias_noise;     // Gyro bias random walk
    float32_t accel_bias_noise;    // Accel bias random walk
    
    // Measurement noise
    float32_t gps_position_noise;  // GPS position noise (m)
    float32_t gps_velocity_noise;  // GPS velocity noise (m/s)
    float32_t baro_altitude_noise; // Barometer noise (m)
    float32_t accel_noise;         // Accelerometer noise (m/s^2)
    float32_t gyro_noise;          // Gyroscope noise (rad/s)
    
    // Initial uncertainties
    float32_t initial_position_std;
    float32_t initial_velocity_std;
    float32_t initial_attitude_std;
    float32_t initial_bias_std;
    
    // Reference origin
    float64_t ref_latitude;   // Reference latitude (degrees)
    float64_t ref_longitude;  // Reference longitude (degrees)
    float32_t ref_altitude;   // Reference altitude (meters MSL)
} eif_imu_config_t;

/**
 * @brief IMU fusion filter state
 */
typedef struct {
    // UKF state vector [15]: pos(3), vel(3), quat(4), gyro_bias(3), accel_bias(2)
    float32_t x[EIF_IMU_STATE_DIM];
    
    // Covariance matrix [15x15]
    float32_t P[EIF_IMU_STATE_DIM * EIF_IMU_STATE_DIM];
    
    // Process noise diagonal
    float32_t Q[EIF_IMU_STATE_DIM];
    
    // Configuration
    eif_imu_config_t config;
    
    // Reference frame
    float64_t ref_lat_rad;
    float64_t ref_lon_rad;
    float32_t ref_alt;
    bool ref_set;
    
    // Sensor validity
    bool gps_valid;
    bool baro_valid;
    bool initialized;
    
    // Gravity magnitude
    float32_t gravity;
    
    // Memory
    eif_memory_pool_t* pool;
    void* scratch;
    size_t scratch_size;
} eif_imu_t;

// =============================================================================
// Initialization API
// =============================================================================

/**
 * @brief Get default IMU configuration
 * @param config Configuration structure to fill
 */
void eif_imu_default_config(eif_imu_config_t* config);

/**
 * @brief Initialize IMU fusion filter
 * @param imu Filter instance
 * @param config Configuration
 * @param pool Memory pool for allocation
 * @return Status code
 */
eif_status_t eif_imu_init(eif_imu_t* imu,
                           const eif_imu_config_t* config,
                           eif_memory_pool_t* pool);

/**
 * @brief Reset filter to initial state
 * @param imu Filter instance
 */
void eif_imu_reset(eif_imu_t* imu);

// =============================================================================
// Sensor Update API
// =============================================================================

/**
 * @brief Process IMU measurement (UKF prediction step)
 * @param imu Filter instance
 * @param accel Accelerometer reading [x, y, z] in m/s^2
 * @param gyro Gyroscope reading [x, y, z] in rad/s
 * @param dt Time step in seconds
 * @return Status code
 */
eif_status_t eif_imu_update_sensors(eif_imu_t* imu,
                                     const float32_t* accel,
                                     const float32_t* gyro,
                                     float32_t dt);

/**
 * @brief Process GPS measurement (UKF correction step)
 * @param imu Filter instance
 * @param latitude GPS latitude in degrees
 * @param longitude GPS longitude in degrees
 * @param altitude GPS altitude in meters MSL
 * @param vel_ned Optional NED velocity [vn, ve, vd] in m/s (NULL if unavailable)
 * @return Status code
 */
eif_status_t eif_imu_update_gps(eif_imu_t* imu,
                                 float64_t latitude,
                                 float64_t longitude,
                                 float32_t altitude,
                                 const float32_t* vel_ned);

/**
 * @brief Process barometer measurement
 * @param imu Filter instance
 * @param altitude Barometric altitude in meters MSL
 * @return Status code
 */
eif_status_t eif_imu_update_baro(eif_imu_t* imu, float32_t altitude);

// =============================================================================
// Output API
// =============================================================================

/**
 * @brief Get full pose estimate
 * @param imu Filter instance
 * @param pose Output pose structure
 * @return Status code
 */
eif_status_t eif_imu_get_pose(const eif_imu_t* imu, eif_pose_t* pose);

/**
 * @brief Get position estimate
 * @param imu Filter instance
 * @param position Output NED position
 */
void eif_imu_get_position(const eif_imu_t* imu, eif_vec3_t* position);

/**
 * @brief Get velocity estimate
 * @param imu Filter instance
 * @param velocity Output NED velocity
 */
void eif_imu_get_velocity(const eif_imu_t* imu, eif_vec3_t* velocity);

/**
 * @brief Get quaternion estimate
 * @param imu Filter instance
 * @param quaternion Output quaternion
 */
void eif_imu_get_quaternion(const eif_imu_t* imu, eif_quat_t* quaternion);

/**
 * @brief Get Euler angles
 * @param imu Filter instance
 * @param euler Output roll, pitch, yaw
 */
void eif_imu_get_euler(const eif_imu_t* imu, eif_vec3_t* euler);

/**
 * @brief Get estimated sensor biases
 * @param imu Filter instance
 * @param gyro_bias Output gyroscope bias
 * @param accel_bias_z Output accelerometer Z bias
 */
void eif_imu_get_biases(const eif_imu_t* imu,
                         eif_vec3_t* gyro_bias,
                         float32_t* accel_bias_z);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Normalize quaternion
 */
void eif_quat_normalize(eif_quat_t* q);

/**
 * @brief Convert quaternion to Euler angles
 */
void eif_quat_to_euler(const eif_quat_t* q, eif_vec3_t* euler);

/**
 * @brief Convert Euler angles to quaternion
 */
void eif_euler_to_quat(const eif_vec3_t* euler, eif_quat_t* q);

/**
 * @brief Rotate vector by quaternion
 */
void eif_quat_rotate_vec(const eif_quat_t* q, const eif_vec3_t* v, eif_vec3_t* out);

/**
 * @brief Convert GPS to NED coordinates
 */
void eif_gps_to_ned(float64_t ref_lat, float64_t ref_lon, float32_t ref_alt,
                    float64_t lat, float64_t lon, float32_t alt,
                    eif_vec3_t* ned);

#ifdef __cplusplus
}
#endif

#endif // EIF_IMU_H
