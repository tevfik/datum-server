# IMU Fusion Demo

Demonstrates the `eif_imu` module: a 15-state Extended Kalman Filter (EKF)
for fusing IMU, GPS, and barometer data.

## Features

- **15-State EKF**: Position, velocity, quaternion, gyro bias, accel bias
- **Simulated Flight**: Quadcopter takeoff → circular flight → landing
- **Realistic Noise**: GPS, IMU, and barometer measurement noise
- **Visualization**: Real-time trajectory and error metrics

## Why EKF?

The Extended Kalman Filter provides reliable state estimation for IMU fusion.
While UKF can theoretically handle nonlinearities better, EKF is more
attitude estimation where quaternion dynamics are highly nonlinear.

## Build & Run

```bash
cd build
cmake ..
make imu_fusion_demo
./examples/filter_demos/imu_fusion/imu_fusion_demo
```

## Expected Output

- RMSE Position: ~2-3 meters (with noisy sensors)
- RMSE Velocity: ~0.3-0.5 m/s
- Bias estimation converges over time
