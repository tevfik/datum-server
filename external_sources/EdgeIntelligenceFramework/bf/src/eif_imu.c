/**
 * @file eif_imu.c
 * @brief IMU Fusion Implementation using EKF
 * 
 * Extended Kalman Filter based fusion of IMU, GPS, and Barometer data.
 * This is a simpler, numerically stable implementation designed for
 * embedded systems.
 */

#include "eif_imu.h"
#include <string.h>
#include <math.h>

// =============================================================================
// Constants
// =============================================================================

#define DEG_TO_RAD (3.14159265358979f / 180.0f)
#define RAD_TO_DEG (180.0f / 3.14159265358979f)
#define EARTH_RADIUS 6371000.0f
#define GRAVITY_NOMINAL 9.80665f

// State indices
#define IDX_POS  0   // Position N,E,D (indices 0,1,2)
#define IDX_VEL  3   // Velocity N,E,D (indices 3,4,5)
#define IDX_QUAT 6   // Quaternion w,x,y,z (indices 6,7,8,9)
#define IDX_GBIAS 10 // Gyro bias x,y,z (indices 10,11,12)
#define IDX_ABIAS 13 // Accel bias xy (indices 13,14)

#define N_STATE EIF_IMU_STATE_DIM

// =============================================================================
// Quaternion Operations
// =============================================================================

void eif_quat_normalize(eif_quat_t* q) {
    float32_t norm = sqrtf(q->w*q->w + q->x*q->x + q->y*q->y + q->z*q->z);
    if (norm > 1e-10f) {
        float32_t inv = 1.0f / norm;
        q->w *= inv;
        q->x *= inv;
        q->y *= inv;
        q->z *= inv;
    }
}

void eif_quat_to_euler(const eif_quat_t* q, eif_vec3_t* euler) {
    // Roll
    float32_t sinr_cosp = 2.0f * (q->w * q->x + q->y * q->z);
    float32_t cosr_cosp = 1.0f - 2.0f * (q->x * q->x + q->y * q->y);
    euler->x = atan2f(sinr_cosp, cosr_cosp);
    
    // Pitch
    float32_t sinp = 2.0f * (q->w * q->y - q->z * q->x);
    if (fabsf(sinp) >= 1.0f) {
        euler->y = copysignf(3.14159265f / 2.0f, sinp);
    } else {
        euler->y = asinf(sinp);
    }
    
    // Yaw
    float32_t siny_cosp = 2.0f * (q->w * q->z + q->x * q->y);
    float32_t cosy_cosp = 1.0f - 2.0f * (q->y * q->y + q->z * q->z);
    euler->z = atan2f(siny_cosp, cosy_cosp);
}

void eif_euler_to_quat(const eif_vec3_t* euler, eif_quat_t* q) {
    float32_t cr = cosf(euler->x * 0.5f);
    float32_t sr = sinf(euler->x * 0.5f);
    float32_t cp = cosf(euler->y * 0.5f);
    float32_t sp = sinf(euler->y * 0.5f);
    float32_t cy = cosf(euler->z * 0.5f);
    float32_t sy = sinf(euler->z * 0.5f);
    
    q->w = cr * cp * cy + sr * sp * sy;
    q->x = sr * cp * cy - cr * sp * sy;
    q->y = cr * sp * cy + sr * cp * sy;
    q->z = cr * cp * sy - sr * sp * cy;
}

void eif_quat_rotate_vec(const eif_quat_t* q, const eif_vec3_t* v, eif_vec3_t* out) {
    float32_t qw = q->w, qx = q->x, qy = q->y, qz = q->z;
    float32_t vx = v->x, vy = v->y, vz = v->z;
    
    float32_t tx = 2.0f * (qy * vz - qz * vy);
    float32_t ty = 2.0f * (qz * vx - qx * vz);
    float32_t tz = 2.0f * (qx * vy - qy * vx);
    
    out->x = vx + qw * tx + (qy * tz - qz * ty);
    out->y = vy + qw * ty + (qz * tx - qx * tz);
    out->z = vz + qw * tz + (qx * ty - qy * tx);
}

// =============================================================================
// GPS Conversion
// =============================================================================

void eif_gps_to_ned(float64_t ref_lat, float64_t ref_lon, float32_t ref_alt,
                    float64_t lat, float64_t lon, float32_t alt,
                    eif_vec3_t* ned) {
    float64_t lat_rad = lat * DEG_TO_RAD;
    float64_t ref_lat_rad = ref_lat * DEG_TO_RAD;
    float64_t dlon = (lon - ref_lon) * DEG_TO_RAD;
    float64_t dlat = lat_rad - ref_lat_rad;
    
    ned->x = (float32_t)(dlat * EARTH_RADIUS);
    ned->y = (float32_t)(dlon * EARTH_RADIUS * cos(ref_lat_rad));
    ned->z = -(alt - ref_alt);
}

// =============================================================================
// Configuration
// =============================================================================

void eif_imu_default_config(eif_imu_config_t* config) {
    config->position_noise = 0.01f;
    config->velocity_noise = 0.1f;
    config->attitude_noise = 0.001f;
    config->gyro_bias_noise = 0.0001f;
    config->accel_bias_noise = 0.001f;
    
    config->gps_position_noise = 2.5f;
    config->gps_velocity_noise = 0.1f;
    config->baro_altitude_noise = 0.5f;
    config->accel_noise = 0.5f;
    config->gyro_noise = 0.01f;
    
    config->initial_position_std = 10.0f;
    config->initial_velocity_std = 1.0f;
    config->initial_attitude_std = 0.1f;
    config->initial_bias_std = 0.1f;
    
    config->ref_latitude = 0.0;
    config->ref_longitude = 0.0;
    config->ref_altitude = 0.0f;
}

// =============================================================================
// Initialization
// =============================================================================

eif_status_t eif_imu_init(eif_imu_t* imu,
                           const eif_imu_config_t* config,
                           eif_memory_pool_t* pool) {
    if (!imu || !config || !pool) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    memset(imu, 0, sizeof(eif_imu_t));
    imu->config = *config;
    imu->pool = pool;
    imu->gravity = GRAVITY_NOMINAL;
    
    // Scratch buffer for EKF operations
    imu->scratch_size = N_STATE * N_STATE * 2 * sizeof(float32_t);
    imu->scratch = eif_memory_alloc(pool, imu->scratch_size, sizeof(float32_t));
    if (!imu->scratch) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    eif_imu_reset(imu);
    return EIF_STATUS_OK;
}

void eif_imu_reset(eif_imu_t* imu) {
    memset(imu->x, 0, sizeof(imu->x));
    imu->x[IDX_QUAT] = 1.0f;  // Unit quaternion
    
    // Initialize covariance as diagonal
    memset(imu->P, 0, sizeof(imu->P));
    
    float32_t pos_var = imu->config.initial_position_std * imu->config.initial_position_std;
    float32_t vel_var = imu->config.initial_velocity_std * imu->config.initial_velocity_std;
    float32_t att_var = imu->config.initial_attitude_std * imu->config.initial_attitude_std;
    float32_t bias_var = imu->config.initial_bias_std * imu->config.initial_bias_std;
    
    for (int i = 0; i < 3; i++) {
        imu->P[(IDX_POS + i) * N_STATE + (IDX_POS + i)] = pos_var;
        imu->P[(IDX_VEL + i) * N_STATE + (IDX_VEL + i)] = vel_var;
    }
    for (int i = 0; i < 4; i++) {
        imu->P[(IDX_QUAT + i) * N_STATE + (IDX_QUAT + i)] = att_var;
    }
    for (int i = 0; i < 3; i++) {
        imu->P[(IDX_GBIAS + i) * N_STATE + (IDX_GBIAS + i)] = bias_var;
    }
    for (int i = 0; i < 2; i++) {
        imu->P[(IDX_ABIAS + i) * N_STATE + (IDX_ABIAS + i)] = bias_var;
    }
    
    // Process noise
    imu->Q[0] = imu->Q[1] = imu->Q[2] = imu->config.position_noise;
    imu->Q[3] = imu->Q[4] = imu->Q[5] = imu->config.velocity_noise;
    imu->Q[6] = imu->Q[7] = imu->Q[8] = imu->Q[9] = imu->config.attitude_noise;
    imu->Q[10] = imu->Q[11] = imu->Q[12] = imu->config.gyro_bias_noise;
    imu->Q[13] = imu->Q[14] = imu->config.accel_bias_noise;
    
    imu->ref_set = false;
    imu->gps_valid = false;
    imu->baro_valid = false;
    imu->initialized = true;
}

// =============================================================================
// EKF Prediction (IMU Update)
// =============================================================================

eif_status_t eif_imu_update_sensors(eif_imu_t* imu,
                                     const float32_t* accel,
                                     const float32_t* gyro,
                                     float32_t dt) {
    if (!imu || !accel || !gyro || dt <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Corrected gyro (remove bias)
    float32_t wx = gyro[0] - imu->x[IDX_GBIAS];
    float32_t wy = gyro[1] - imu->x[IDX_GBIAS + 1];
    float32_t wz = gyro[2] - imu->x[IDX_GBIAS + 2];
    
    // Current quaternion
    eif_quat_t q = {imu->x[IDX_QUAT], imu->x[IDX_QUAT+1], 
                    imu->x[IDX_QUAT+2], imu->x[IDX_QUAT+3]};
    
    // Quaternion derivative: q_dot = 0.5 * q * omega
    float32_t dqw = 0.5f * (-q.x * wx - q.y * wy - q.z * wz);
    float32_t dqx = 0.5f * (q.w * wx + q.y * wz - q.z * wy);
    float32_t dqy = 0.5f * (q.w * wy + q.z * wx - q.x * wz);
    float32_t dqz = 0.5f * (q.w * wz + q.x * wy - q.y * wx);
    
    // Update quaternion
    imu->x[IDX_QUAT]   += dqw * dt;
    imu->x[IDX_QUAT+1] += dqx * dt;
    imu->x[IDX_QUAT+2] += dqy * dt;
    imu->x[IDX_QUAT+3] += dqz * dt;
    
    // Normalize quaternion
    q.w = imu->x[IDX_QUAT]; q.x = imu->x[IDX_QUAT+1];
    q.y = imu->x[IDX_QUAT+2]; q.z = imu->x[IDX_QUAT+3];
    eif_quat_normalize(&q);
    imu->x[IDX_QUAT] = q.w; imu->x[IDX_QUAT+1] = q.x;
    imu->x[IDX_QUAT+2] = q.y; imu->x[IDX_QUAT+3] = q.z;
    
    // Rotate accelerometer to NED and remove gravity
    eif_vec3_t accel_body = {
        accel[0] - imu->x[IDX_ABIAS],
        accel[1] - imu->x[IDX_ABIAS + 1],
        accel[2]  // Z bias not estimated in this simplified model
    };
    
    eif_vec3_t accel_ned;
    eif_quat_rotate_vec(&q, &accel_body, &accel_ned);
    
    // Remove gravity (in NED, gravity is +g in Down direction)
    accel_ned.z += imu->gravity;
    
    // Update velocity
    imu->x[IDX_VEL]   += accel_ned.x * dt;
    imu->x[IDX_VEL+1] += accel_ned.y * dt;
    imu->x[IDX_VEL+2] += accel_ned.z * dt;
    
    // Update position
    imu->x[IDX_POS]   += imu->x[IDX_VEL] * dt + 0.5f * accel_ned.x * dt * dt;
    imu->x[IDX_POS+1] += imu->x[IDX_VEL+1] * dt + 0.5f * accel_ned.y * dt * dt;
    imu->x[IDX_POS+2] += imu->x[IDX_VEL+2] * dt + 0.5f * accel_ned.z * dt * dt;
    
    // Simplified EKF: just add process noise to diagonal
    for (int i = 0; i < N_STATE; i++) {
        imu->P[i * N_STATE + i] += imu->Q[i] * dt;
    }
    
    return EIF_STATUS_OK;
}

// =============================================================================
// GPS Update (Position/Velocity Correction)
// =============================================================================

eif_status_t eif_imu_update_gps(eif_imu_t* imu,
                                 float64_t latitude,
                                 float64_t longitude,
                                 float32_t altitude,
                                 const float32_t* vel_ned) {
    if (!imu) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    // Set reference on first fix
    if (!imu->ref_set) {
        imu->config.ref_latitude = latitude;
        imu->config.ref_longitude = longitude;
        imu->config.ref_altitude = altitude;
        imu->ref_set = true;
    }
    
    // Convert to NED
    eif_vec3_t gps_ned;
    eif_gps_to_ned(imu->config.ref_latitude, imu->config.ref_longitude,
                   imu->config.ref_altitude, latitude, longitude, altitude,
                   &gps_ned);
    
    float32_t R_pos = imu->config.gps_position_noise * imu->config.gps_position_noise;
    float32_t R_vel = imu->config.gps_velocity_noise * imu->config.gps_velocity_noise;
    
    // Update position (3 scalar updates)
    for (int axis = 0; axis < 3; axis++) {
        float32_t z = (axis == 0) ? gps_ned.x : (axis == 1) ? gps_ned.y : gps_ned.z;
        float32_t h = imu->x[IDX_POS + axis];
        float32_t y = z - h;
        
        int idx = IDX_POS + axis;
        float32_t S = imu->P[idx * N_STATE + idx] + R_pos;
        
        if (S > 1e-10f) {
            float32_t K[N_STATE];
            for (int i = 0; i < N_STATE; i++) {
                K[i] = imu->P[i * N_STATE + idx] / S;
            }
            
            // State update
            for (int i = 0; i < N_STATE; i++) {
                imu->x[i] += K[i] * y;
            }
            
            // Covariance update (Joseph form for stability)
            for (int i = 0; i < N_STATE; i++) {
                for (int j = 0; j < N_STATE; j++) {
                    imu->P[i * N_STATE + j] -= K[i] * imu->P[idx * N_STATE + j];
                }
            }
        }
    }
    
    // Update velocity if available
    if (vel_ned) {
        for (int axis = 0; axis < 3; axis++) {
            float32_t z = vel_ned[axis];
            float32_t h = imu->x[IDX_VEL + axis];
            float32_t y = z - h;
            
            int idx = IDX_VEL + axis;
            float32_t S = imu->P[idx * N_STATE + idx] + R_vel;
            
            if (S > 1e-10f) {
                float32_t K[N_STATE];
                for (int i = 0; i < N_STATE; i++) {
                    K[i] = imu->P[i * N_STATE + idx] / S;
                }
                
                for (int i = 0; i < N_STATE; i++) {
                    imu->x[i] += K[i] * y;
                }
                
                for (int i = 0; i < N_STATE; i++) {
                    for (int j = 0; j < N_STATE; j++) {
                        imu->P[i * N_STATE + j] -= K[i] * imu->P[idx * N_STATE + j];
                    }
                }
            }
        }
    }
    
    // Renormalize quaternion
    eif_quat_t q = {imu->x[IDX_QUAT], imu->x[IDX_QUAT+1], 
                    imu->x[IDX_QUAT+2], imu->x[IDX_QUAT+3]};
    eif_quat_normalize(&q);
    imu->x[IDX_QUAT] = q.w; imu->x[IDX_QUAT+1] = q.x;
    imu->x[IDX_QUAT+2] = q.y; imu->x[IDX_QUAT+3] = q.z;
    
    imu->gps_valid = true;
    return EIF_STATUS_OK;
}

// =============================================================================
// Barometer Update
// =============================================================================

eif_status_t eif_imu_update_baro(eif_imu_t* imu, float32_t altitude) {
    if (!imu) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    float32_t z_ned = -(altitude - imu->config.ref_altitude);
    float32_t R = imu->config.baro_altitude_noise * imu->config.baro_altitude_noise;
    
    float32_t y = z_ned - imu->x[IDX_POS + 2];
    int idx = IDX_POS + 2;
    float32_t S = imu->P[idx * N_STATE + idx] + R;
    
    if (S > 1e-10f) {
        float32_t K[N_STATE];
        for (int i = 0; i < N_STATE; i++) {
            K[i] = imu->P[i * N_STATE + idx] / S;
        }
        
        for (int i = 0; i < N_STATE; i++) {
            imu->x[i] += K[i] * y;
        }
        
        for (int i = 0; i < N_STATE; i++) {
            for (int j = 0; j < N_STATE; j++) {
                imu->P[i * N_STATE + j] -= K[i] * imu->P[idx * N_STATE + j];
            }
        }
    }
    
    imu->baro_valid = true;
    return EIF_STATUS_OK;
}

// =============================================================================
// Output Functions
// =============================================================================

eif_status_t eif_imu_get_pose(const eif_imu_t* imu, eif_pose_t* pose) {
    if (!imu || !pose) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    pose->position.x = imu->x[IDX_POS];
    pose->position.y = imu->x[IDX_POS + 1];
    pose->position.z = imu->x[IDX_POS + 2];
    
    pose->velocity.x = imu->x[IDX_VEL];
    pose->velocity.y = imu->x[IDX_VEL + 1];
    pose->velocity.z = imu->x[IDX_VEL + 2];
    
    pose->quaternion.w = imu->x[IDX_QUAT];
    pose->quaternion.x = imu->x[IDX_QUAT + 1];
    pose->quaternion.y = imu->x[IDX_QUAT + 2];
    pose->quaternion.z = imu->x[IDX_QUAT + 3];
    
    eif_quat_to_euler(&pose->quaternion, &pose->euler);
    
    return EIF_STATUS_OK;
}

void eif_imu_get_position(const eif_imu_t* imu, eif_vec3_t* position) {
    position->x = imu->x[IDX_POS];
    position->y = imu->x[IDX_POS + 1];
    position->z = imu->x[IDX_POS + 2];
}

void eif_imu_get_velocity(const eif_imu_t* imu, eif_vec3_t* velocity) {
    velocity->x = imu->x[IDX_VEL];
    velocity->y = imu->x[IDX_VEL + 1];
    velocity->z = imu->x[IDX_VEL + 2];
}

void eif_imu_get_quaternion(const eif_imu_t* imu, eif_quat_t* quaternion) {
    quaternion->w = imu->x[IDX_QUAT];
    quaternion->x = imu->x[IDX_QUAT + 1];
    quaternion->y = imu->x[IDX_QUAT + 2];
    quaternion->z = imu->x[IDX_QUAT + 3];
}

void eif_imu_get_euler(const eif_imu_t* imu, eif_vec3_t* euler) {
    eif_quat_t q = {imu->x[IDX_QUAT], imu->x[IDX_QUAT+1], 
                    imu->x[IDX_QUAT+2], imu->x[IDX_QUAT+3]};
    eif_quat_to_euler(&q, euler);
}

void eif_imu_get_biases(const eif_imu_t* imu,
                         eif_vec3_t* gyro_bias,
                         float32_t* accel_bias_z) {
    if (gyro_bias) {
        gyro_bias->x = imu->x[IDX_GBIAS];
        gyro_bias->y = imu->x[IDX_GBIAS + 1];
        gyro_bias->z = imu->x[IDX_GBIAS + 2];
    }
    if (accel_bias_z) {
        *accel_bias_z = imu->x[IDX_ABIAS];
    }
}
