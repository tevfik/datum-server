# Sensor Fusion Fundamentals

A conceptual guide to combining sensor data for accurate state estimation.

> **For embedded developers**: You have IMU data (accelerometers, gyroscopes, magnetometers).
> This guide explains how to combine them for reliable orientation and position.

---

## Table of Contents

1. [Why Sensor Fusion?](#why-sensor-fusion)
2. [The Fundamental Problem](#the-fundamental-problem)
3. [Complementary Filter](#complementary-filter)
4. [Kalman Filter Explained](#kalman-filter-explained)
5. [Extended Kalman Filter (EKF)](#extended-kalman-filter-ekf)
6. [Sensor Fusion Architectures](#sensor-fusion-architectures)
7. [Quaternions for 3D Orientation](#quaternions-for-3d-orientation)
8. [Common Mistakes](#common-mistakes)

---

## Why Sensor Fusion?

Every sensor has limitations:

| Sensor | Good At | Bad At |
|--------|---------|--------|
| **Accelerometer** | Long-term orientation | Vibration, linear acceleration |
| **Gyroscope** | Fast rotation tracking | Drift over time |
| **Magnetometer** | Absolute heading | Metal interference |
| **GPS** | Absolute position | Slow update, no indoor |
| **Barometer** | Altitude changes | Slow, weather affected |

**Sensor fusion** combines strengths, cancels weaknesses.

```
Gyro (fast, drifts) + Accelerometer (slow, noisy) = Stable orientation
```

---

## The Fundamental Problem

### Gyroscope Integration Drift

Integrating angular rate gives angle, but errors accumulate:

```
                           Actual angle
    Angle  │                    ─────────
           │              ♦ ♦ ♦
           │         ♦ ♦ ♦      Gyro integration
           │    ♦ ♦ ♦            (drifts away)
           │♦ ♦ ♦
           └──────────────────► Time
```

After 1 minute, you might be 10° off!

### Accelerometer Noise

Accelerometer gives gravity direction (long-term correct) but noisy:

```
    Angle  │    ∿∿∿∿∿∿∿∿∿∿∿∿∿ Accelerometer (noisy but correct average)
           │  ────────────────  True angle
           │
           └──────────────────► Time
```

### The Solution: Combine Them!

```
High-pass(gyro) + Low-pass(accel) = Best of both
   Fast response    No drift
```

---

## Complementary Filter

The simplest sensor fusion. Perfect for embedded.

### Concept

```
angle = α × (angle + gyro × dt) + (1-α) × accel_angle
        \_________________/      \__________________/
         High-frequency          Low-frequency
         (from gyro)             (from accel)
```

### Implementation

```c
// Complementary filter for pitch
float pitch = 0;
const float alpha = 0.98;  // Trust gyro 98%

void update(float accel_pitch, float gyro_rate, float dt) {
    float gyro_pitch = pitch + gyro_rate * dt;  // Integrate
    float accel_angle = atan2(ax, az);          // From gravity
    
    pitch = alpha * gyro_pitch + (1 - alpha) * accel_angle;
}
```

### Choosing Alpha

| Alpha | Behavior |
|-------|----------|
| 0.98 | Very smooth, slow correction (typical IMU) |
| 0.90 | Faster correction, more noise |
| 0.50 | Equal weight (not recommended) |

**Rule of thumb**: `alpha = tau / (tau + dt)` where tau is filter time constant.

### In EIF

```c
#include "eif_ml.h"  // or eif_sensor_fusion.h

eif_complementary_t filter;
eif_complementary_init(&filter, 0.98f, 0.02f);  // alpha, dt

float angle = eif_complementary_update(&filter, gyro_rate, accel_angle);
```

### Pros and Cons

**Pros**:
- Simple (5 lines of code)
- Fast (just multiplication/addition)
- No tuning required (alpha ~ 0.98 works)

**Cons**:
- No confidence/uncertainty estimate
- Can't incorporate multiple sensors easily
- Not optimal (Kalman is better)

---

## Kalman Filter Explained

The Kalman filter is an **optimal estimator** - it gives the best possible estimate given noisy measurements.

### Core Idea

1. **Predict** what the next state will be (using physics/model)
2. **Update** with measurement (weighted by confidence)

```
    ┌─────────┐         ┌─────────┐
    │ Predict │───────► │ Update  │───────► Best Estimate
    └────┬────┘         └────┬────┘
         │                   │
    Uses model          Uses measurement
    (physics)           (sensor)
```

### The Magic: Kalman Gain

How much to trust measurement vs prediction:

```
Kalman Gain K = Prediction_Uncertainty / Total_Uncertainty

If prediction is uncertain: K → 1 (trust measurement more)
If measurement is uncertain: K → 0 (trust prediction more)
```

### 1D Example: Altitude Estimation

```c
// State: altitude
// Measurement: barometer

float altitude = 0;          // State estimate
float uncertainty = 1.0;     // State uncertainty

void kalman_update(float baro_reading, float baro_noise) {
    // Kalman gain
    float K = uncertainty / (uncertainty + baro_noise);
    
    // Update estimate
    altitude = altitude + K * (baro_reading - altitude);
    
    // Update uncertainty
    uncertainty = (1 - K) * uncertainty;
}
```

### In EIF

```c
#include "eif_ml.h"

eif_kalman_1d_t kf;
eif_kalman_1d_init(&kf, 0.0f, 1.0f, 0.01f, 0.5f);  // x0, P0, Q, R
//                       ↑     ↑     ↑     ↑
//                    Initial  Initial Process Measurement
//                    state    uncert  noise   noise

float estimate = eif_kalman_1d_update(&kf, measurement);
```

### Tuning Q and R

| Parameter | Meaning | Effect |
|-----------|---------|--------|
| **Q** (Process noise) | How much model drifts | Larger = trust measurement more |
| **R** (Measurement noise) | How noisy sensor is | Larger = trust prediction more |

**Rule of thumb**: Start with R = sensor noise variance, Q = 0.01×R.

---

## Extended Kalman Filter (EKF)

For **nonlinear** systems (most real robots/drones).

### Why "Extended"?

Linear Kalman works for: `x_new = A * x + B * u`

But orientation is nonlinear:
- Quaternion multiplication is nonlinear
- Sensor models are nonlinear (e.g., GPS lat/lon to position)

**EKF linearizes** around current estimate using Jacobians.

### State Vector Example (IMU Fusion)

```
State = [roll, pitch, yaw, gyro_bias_x, gyro_bias_y, gyro_bias_z]

- Orientation (roll, pitch, yaw)
- Gyro biases (calibrated online)
```

### Architecture

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  Gyro Input  │────►│              │     │              │
└──────────────┘     │    EKF       │────►│  Attitude    │
                     │   Fusion     │     │  Estimate    │
┌──────────────┐     │              │     │              │
│ Accel Input  │────►│              │     │              │
└──────────────┘     └──────────────┘     └──────────────┘
```

### In EIF

```c
#include "eif_imu.h"

eif_imu_t imu;
eif_imu_config_t config;
eif_imu_default_config(&config);
eif_imu_init(&imu, &config, &pool);

// In loop
eif_imu_update_sensors(&imu, accel, gyro, dt);

eif_pose_t pose;
eif_imu_get_pose(&imu, &pose);
// pose.quaternion, pose.euler, pose.position
```

---

## Sensor Fusion Architectures

### Loosely Coupled

Each sensor processed separately, then combined:

```
GPS ──► GPS Kalman ──┐
                     ├──► Final Fusion ──► Position
IMU ──► IMU Kalman ──┘
```

**Pros**: Simple, sensors independent
**Cons**: Suboptimal, can't share information

### Tightly Coupled

All sensors in one Kalman filter:

```
GPS raw ──────┐
              ├──► Single EKF ──► Position + Velocity + Attitude
IMU raw ──────┘
```

**Pros**: Optimal, GPS helps IMU calibration
**Cons**: Complex, all sensors must be synchronized

### Recommendation

| Application | Architecture |
|-------------|--------------|
| Simple IMU orientation | Complementary filter |
| Drone attitude | EKF (tightly coupled IMU) |
| GPS/IMU navigation | Loosely coupled (simpler) or tightly (better) |
| SLAM | Tightly coupled EKF or particle filter |

---

## Quaternions for 3D Orientation

### Why Not Euler Angles?

Euler angles (roll, pitch, yaw) have **gimbal lock** at ±90° pitch.

Quaternions:
- No gimbal lock
- Smooth interpolation
- Efficient rotation composition

### Quaternion Basics

A quaternion has 4 components: `q = [w, x, y, z]`

```
w = cos(θ/2)
x = sin(θ/2) × axis_x
y = sin(θ/2) × axis_y
z = sin(θ/2) × axis_z
```

Where θ is rotation angle around axis (x, y, z).

### Operations

**Rotation composition**:
```c
q_total = q2 * q1  (apply q1 first, then q2)
```

**Rotate vector**:
```c
v' = q * v * q^(-1)
```

### Converting Quaternion to Euler

```c
// Assuming q = [w, x, y, z]
roll  = atan2(2*(w*x + y*z), 1 - 2*(x*x + y*y));
pitch = asin(2*(w*y - z*x));
yaw   = atan2(2*(w*z + x*y), 1 - 2*(y*y + z*z));
```

### In EIF

```c
#include "eif_imu.h"

eif_pose_t pose;
eif_imu_get_pose(&imu, &pose);

// Access quaternion
float w = pose.quaternion[0];
float x = pose.quaternion[1];
float y = pose.quaternion[2];
float z = pose.quaternion[3];

// Or Euler angles (computed from quaternion)
float roll = pose.euler[0];
float pitch = pose.euler[1];
float yaw = pose.euler[2];
```

---

## Common Mistakes

### Mistake 1: Wrong Coordinate Frame

```c
// WRONG: Mixing sensor frames
angle = atan2(ay, az);  // Assumes specific IMU orientation!

// RIGHT: Check your sensor datasheet
// Different IMUs have different axis conventions (NED, ENU, etc.)
```

### Mistake 2: Forgetting Gyro Bias

```c
// WRONG: Using raw gyro
angle += gyro_rate * dt;

// RIGHT: Remove bias
angle += (gyro_rate - gyro_bias) * dt;
// Or let Kalman filter estimate bias online
```

### Mistake 3: Not Normalizing Quaternions

```c
// WRONG: Quaternion drifts from unit length
q = q + delta_q;

// RIGHT: Normalize after integration
float norm = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
q.w /= norm;
q.x /= norm;
q.y /= norm;
q.z /= norm;
```

### Mistake 4: Magnetic Disturbance

```c
// WRONG: Trust magnetometer everywhere
yaw = atan2(my, mx);  // Metal nearby = wrong!

// RIGHT: Detect and reject disturbance
if (mag_magnitude > 1.2 * expected || mag_magnitude < 0.8 * expected) {
    // Magnetic disturbance - don't update yaw
}
```

### Mistake 5: Accelerometer During Motion

```c
// WRONG: Use accel for tilt during acceleration
tilt = atan2(ax, az);  // Wrong if device is accelerating!

// RIGHT: Only when near 1g total
float mag = sqrt(ax*ax + ay*ay + az*az);
if (fabs(mag - 9.81) < 0.5) {
    // Safe to use for tilt
}
```

---

## EIF Sensor Fusion Cheat Sheet

### Quick Reference

```c
// Simple complementary filter
eif_complementary_t cf;
eif_complementary_init(&cf, 0.98f, dt);
float angle = eif_complementary_update(&cf, gyro, accel_angle);

// 1D Kalman
eif_kalman_1d_t kf;
eif_kalman_1d_init(&kf, 0, 1, 0.01, 0.5);
float estimate = eif_kalman_1d_update(&kf, measurement);

// Full IMU fusion
eif_imu_t imu;
eif_imu_default_config(&config);
eif_imu_init(&imu, &config, &pool);
eif_imu_update_sensors(&imu, accel, gyro, dt);
```

### Memory Usage

| Component | RAM |
|-----------|-----|
| Complementary (1D) | 12 B |
| Kalman 1D | 16 B |
| Full IMU EKF | 2-4 KB |

---

## Next Steps

1. **Try demos**: `./bin/imu_fusion_demo --batch`
2. **Start simple**: Use complementary filter first
3. **Read**: EIF `eif_imu.h` for full EKF implementation
4. **Experiment**: Compare complementary vs Kalman on real IMU data
