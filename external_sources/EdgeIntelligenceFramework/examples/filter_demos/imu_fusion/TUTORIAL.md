# IMU Sensor Fusion Tutorial: Drone/Vehicle Navigation

## Learning Objectives

By the end of this tutorial, you will understand:
- How IMU sensors (accelerometer & gyroscope) work
- Quaternion representation for 3D orientation
- Extended Kalman Filter (EKF) for sensor fusion
- Integrating GPS and barometer data
- Building a complete navigation system

**Level**: Intermediate to Advanced  
**Prerequisites**: Basic linear algebra, understanding of coordinate systems  
**Time**: 45-60 minutes

---

## 1. Introduction to Inertial Navigation

### What is an IMU?

An Inertial Measurement Unit combines:
- **Accelerometer**: Measures acceleration (m/s²) in 3 axes
- **Gyroscope**: Measures angular velocity (rad/s) in 3 axes
- **Magnetometer** (optional): Measures magnetic field for heading

### Applications

| Application | Requirements |
|-------------|-------------|
| **Drones** | High rate (100-400Hz), accurate attitude |
| **Autonomous Cars** | GPS fusion, lane-level accuracy |
| **Robotics** | SLAM integration, odometry |
| **Wearables** | Low power, activity recognition |

---

## 2. Understanding IMU Data

### 2.1 Accelerometer

```
      +Z (Up)
       ↑
       │
       │    /+Y (Right)
       │   /
       │  /
       │ /
       ├────────→ +X (Forward)
```

**Key Point**: At rest, accelerometer reads [0, 0, +9.81] (gravity!)

```c
// Raw accelerometer data
float32_t accel[3] = {0.1f, -0.05f, 9.78f};  // m/s²
// Approximate: forward = 0, left = 0, up = gravity
```

### 2.2 Gyroscope

Measures rotation rate around each axis:
- **Roll (X)**: Tilt left/right
- **Pitch (Y)**: Tilt forward/backward  
- **Yaw (Z)**: Turn left/right

```c
// Raw gyroscope data
float32_t gyro[3] = {0.01f, -0.02f, 0.05f};  // rad/s
```

### 2.3 Sensor Noise & Bias

| Error Type | Description | Mitigation |
|------------|-------------|------------|
| **White Noise** | Random fluctuations | Low-pass filter |
| **Bias** | Constant offset | Calibration / estimation |
| **Drift** | Accumulating error | GPS/external aiding |

---

## 3. Coordinate Frames

### 3.1 Body Frame vs World Frame

```
BODY FRAME (Sensor)          WORLD FRAME (NED)
     X (Forward)                  N (North)
       ↑                            ↑
       │                            │
       │                            │
   ────┼────→ Y                 ────┼────→ E (East)
       │     (Right)                │
       ↓                            ↓
     Z (Down)                     D (Down)
```

### 3.2 NED Convention

| Axis | Direction | Used for |
|------|-----------|----------|
| **N** | North | Latitude |
| **E** | East | Longitude |
| **D** | Down | Altitude |

---

## 4. Quaternion Representation

### Why Quaternions?

| Representation | Pros | Cons |
|----------------|------|------|
| **Euler Angles** | Intuitive | Gimbal lock |
| **Rotation Matrix** | No singularities | 9 parameters |
| **Quaternion** | 4 params, no gimbal lock | Less intuitive |

### Quaternion Basics

```
q = [w, x, y, z]

where:
- w = cos(θ/2)
- [x, y, z] = sin(θ/2) × [ax, ay, az]  (rotation axis)
```

**Constraint**: ||q|| = 1 (unit quaternion)

### EIF API

```c
// Quaternion structure
typedef struct {
    float32_t w, x, y, z;
} eif_quat_t;

// Operations
eif_quat_normalize(&q);
eif_quat_to_euler(&q, &euler);
eif_euler_to_quat(&euler, &q);
eif_quat_rotate_vec(&q, &vec_body, &vec_world);
```

---

## 5. Extended Kalman Filter

### 5.1 State Vector

Our 15-state navigation filter:

```
State Vector x = [
    pos_n, pos_e, pos_d,         // Position (3)
    vel_n, vel_e, vel_d,         // Velocity (3)
    q_w, q_x, q_y, q_z,          // Quaternion (4)
    bias_gx, bias_gy, bias_gz,   // Gyro bias (3)
    bias_ax, bias_ay             // Accel bias (2)
]
```

### 5.2 Prediction Step (IMU)

```c
// Called at 100Hz
eif_status_t eif_imu_update_sensors(
    eif_imu_t* imu,
    const float32_t* accel,  // [ax, ay, az]
    const float32_t* gyro,   // [gx, gy, gz]
    float32_t dt             // Time step
);
```

Algorithm:
1. Remove gyro bias from measurements
2. Update quaternion using angular velocity
3. Rotate acceleration to world frame
4. Remove gravity
5. Integrate velocity and position
6. Propagate covariance matrix

### 5.3 Correction Steps (GPS/Baro)

```c
// Called at 5Hz (GPS)
eif_imu_update_gps(
    &imu, 
    latitude, longitude, altitude,
    velocity_ned  // Optional
);

// Called at 25Hz (Barometer)
eif_imu_update_baro(&imu, baro_altitude);
```

---

## 6. Code Walkthrough

### 6.1 Initialization

```c
#include "eif_imu.h"

// Memory pool
static uint8_t pool_buf[16384];
eif_memory_pool_t pool;
eif_memory_pool_init(&pool, pool_buf, sizeof(pool_buf));

// IMU fusion
eif_imu_t imu;
eif_imu_config_t config;
eif_imu_default_config(&config);

// Tune noise parameters
config.gps_position_noise = 2.5f;   // meters
config.gps_velocity_noise = 0.1f;   // m/s
config.accel_noise = 0.5f;          // m/s²
config.gyro_noise = 0.01f;          // rad/s

eif_imu_init(&imu, &config, &pool);
```

### 6.2 Main Loop

```c
float dt = 0.01f;  // 100Hz IMU rate

while (1) {
    // Read sensors
    float32_t accel[3], gyro[3];
    read_imu(&accel, &gyro);
    
    // EKF prediction
    eif_imu_update_sensors(&imu, accel, gyro, dt);
    
    // GPS correction (5Hz)
    if (gps_ready) {
        eif_imu_update_gps(&imu, lat, lon, alt, vel);
    }
    
    // Baro correction (25Hz)
    if (baro_ready) {
        eif_imu_update_baro(&imu, baro_alt);
    }
    
    // Get output
    eif_pose_t pose;
    eif_imu_get_pose(&imu, &pose);
    
    // Use pose.position, pose.velocity, pose.quaternion
    send_telemetry(&pose);
}
```

### 6.3 Output Extraction

```c
// Full pose
eif_pose_t pose;
eif_imu_get_pose(&imu, &pose);

// Individual components
eif_vec3_t position, velocity;
eif_quat_t quaternion;
eif_vec3_t euler;

eif_imu_get_position(&imu, &position);
eif_imu_get_velocity(&imu, &velocity);
eif_imu_get_quaternion(&imu, &quaternion);
eif_imu_get_euler(&imu, &euler);  // roll, pitch, yaw

// Estimated biases
eif_vec3_t gyro_bias;
float accel_bias_z;
eif_imu_get_biases(&imu, &gyro_bias, &accel_bias_z);
```

---

## 7. GPS Integration

### GPS to NED Conversion

```c
// Set reference point (first fix)
// Automatic in eif_imu_update_gps()

// Manual conversion
eif_vec3_t ned;
eif_gps_to_ned(
    ref_lat, ref_lon, ref_alt,
    current_lat, current_lon, current_alt,
    &ned
);
```

### Formulas

```
Δlat = lat - ref_lat  (in radians)
Δlon = lon - ref_lon  (in radians)

N = Δlat × R_earth
E = Δlon × R_earth × cos(ref_lat)
D = -(alt - ref_alt)
```

---

## 8. Noise Tuning

### Process Noise (Q)

| State | Typical Value | Effect if too low | Effect if too high |
|-------|--------------|-------------------|-------------------|
| Position | 0.01 | Slow to correct | Jumpy position |
| Velocity | 0.1 | Sluggish response | Noisy velocity |
| Attitude | 0.001 | Gimbal lock issues | Attitude noise |
| Gyro bias | 0.0001 | Bias not estimated | Random walk |

### Measurement Noise (R)

| Sensor | Typical Value | Notes |
|--------|--------------|-------|
| GPS Position | 2.5 m | From GPS spec |
| GPS Velocity | 0.1 m/s | Usually accurate |
| Barometer | 0.5 m | Check datasheet |

---

## 9. Experiments

### Experiment 1: Compare with/without GPS
Disable GPS updates and observe drift.

### Experiment 2: Adjust Noise Parameters
Try extreme Q/R values and observe filter behavior.

### Experiment 3: Circular Motion
Verify centripetal acceleration handling.

---

## 10. Hardware Deployment

### Bill of Materials

| Component | Example | Interface |
|-----------|---------|-----------|
| MCU | STM32F4 | - |
| IMU | MPU6050, BMI160 | I2C/SPI |
| GPS | NEO-6M, NEO-M8N | UART |
| Barometer | BMP280, MS5611 | I2C |

### STM32F4 Example

```c
// In HAL IRQ handler (100Hz timer)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    float accel[3], gyro[3];
    mpu6050_read(&accel, &gyro);
    eif_imu_update_sensors(&imu, accel, gyro, 0.01f);
}

// In UART IRQ (GPS NMEA)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (parse_gps_nmea(&lat, &lon, &alt, &vel)) {
        eif_imu_update_gps(&imu, lat, lon, alt, vel);
    }
}
```

---

## 11. Summary

### Key Concepts
1. **IMU Sensors**: Accelerometer + Gyroscope
2. **Quaternions**: Singularity-free 3D rotation
3. **EKF**: Optimal sensor fusion
4. **GPS/Baro**: External aiding for drift correction

### EIF APIs
- `eif_imu_init()` - Initialize filter
- `eif_imu_update_sensors()` - IMU prediction
- `eif_imu_update_gps()` - GPS correction
- `eif_imu_update_baro()` - Barometer correction
- `eif_imu_get_pose()` - Get fused output

### Next Steps
- Try `robot_slam` for full SLAM
- Implement on drone with real sensors
- Add magnetometer for heading
