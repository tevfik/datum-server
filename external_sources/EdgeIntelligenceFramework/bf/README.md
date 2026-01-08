# Bayesian Filters Module (`bf/`)

State estimation and sensor fusion for navigation and robotics.

## Algorithms

| Filter | Use Case |
|--------|----------|
| **Kalman Filter** | Linear systems |
| **Extended KF (EKF)** | Nonlinear with Jacobians |
| **Unscented KF (UKF)** | Nonlinear without Jacobians |
| **Particle Filter** | Non-Gaussian distributions |
| **Complementary Filter** | IMU attitude |
| **EKF-SLAM** | Simultaneous localization & mapping |
| **UKF-SLAM** | More robust SLAM |
| **IMU Fusion** | 15-state navigation filter |

## IMU Fusion Features

- Position, velocity, attitude estimation
- Quaternion-based attitude
- Gyroscope bias estimation
- Accelerometer bias estimation
- GPS aiding (position + velocity)
- Barometer aiding (altitude)

## Usage

### Kalman Filter
```c
#include "eif_bayesian.h"

eif_kalman_t kf;
eif_kalman_init(&kf, state_dim, meas_dim);
eif_kalman_predict(&kf, dt);
eif_kalman_update(&kf, measurement);
```

### IMU Fusion
```c
#include "eif_imu.h"

eif_imu_t imu;
eif_imu_config_t config;
eif_imu_default_config(&config);
eif_imu_init(&imu, &config, &pool);

// Each loop iteration:
eif_imu_update_sensors(&imu, accel, gyro, dt);
eif_imu_update_gps(&imu, lat, lon, alt, vel);

eif_pose_t pose;
eif_imu_get_pose(&imu, &pose);
```

### EKF-SLAM
```c
#include "eif_slam.h"

eif_ekf_slam_t slam;
eif_ekf_slam_init(&slam, 100, &pool);
eif_ekf_slam_predict(&slam, control, dt);
eif_ekf_slam_update(&slam, observations);
```

## Files
- `eif_bayesian.h` - Kalman, EKF, UKF, Particle
- `eif_slam.h` - EKF-SLAM, UKF-SLAM
- `eif_imu.h` - 15-state IMU fusion
