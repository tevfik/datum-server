# Robot SLAM Demo - 2D Localization and Mapping

## Overview
This tutorial demonstrates **SLAM** (Simultaneous Localization and Mapping) using Extended Kalman Filter for a mobile robot navigating an unknown environment.

## Scenario
A mobile robot explores a 2D world containing landmark features. Using range-bearing sensors, it must:
1. **Localize** - Estimate its own position (x, y, θ)
2. **Map** - Build a map of landmark positions

The chicken-and-egg problem: To localize, you need a map. To map, you need to know your location. SLAM solves both simultaneously.

## Algorithms Used

### 1. Extended Kalman Filter SLAM (EKF-SLAM)

**State Vector:**
```
x = [robot_x, robot_y, robot_θ, lm1_x, lm1_y, lm2_x, lm2_y, ...]
```

State grows as new landmarks are discovered.

**Motion Model (Predict):**
```
x' = x + v·cos(θ)·dt
y' = y + v·sin(θ)·dt
θ' = θ + ω·dt
```

**Observation Model (Update):**
```
range = √((lm_x - x)² + (lm_y - y)²)
bearing = atan2(lm_y - y, lm_x - x) - θ
```

### 2. Data Association
Matching observed landmarks to known landmarks in the map:
- **Known Landmark**: Update existing estimate
- **New Landmark**: Add to state vector with initial uncertainty

### 3. Covariance Management

| Matrix | Size | Purpose |
|--------|------|---------|
| **P** | (3+2N) × (3+2N) | Full state covariance |
| **Q** | 3 × 3 | Process noise (motion) |
| **R** | 2 × 2 | Measurement noise (sensor) |

**Key Insight:** Robot-landmark cross-correlations in P capture how observations of landmarks reduce uncertainty about robot pose.

## Demo Walkthrough

1. **Initialization** - Robot starts at known position
2. **Motion Prediction** - Robot moves in circular path
3. **Observation** - Range-bearing sensor detects nearby landmarks
4. **Data Association** - Match observations to known/new landmarks
5. **EKF Update** - Refine robot and landmark estimates
6. **ASCII Map** - Real-time visualization of robot path and landmarks

## ASCII Map Legend
```
@ = True Robot Position
r = Estimated Robot Position  
X = True Landmark
o = Estimated Landmark
* = Matched (estimate overlaps true)
- = Robot trajectory
```

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_ekf_slam_init()` | Initialize EKF-SLAM with dimensions |
| `eif_ekf_slam_predict()` | Motion model prediction |
| `eif_ekf_slam_update()` | Measurement update with Jacobians |
| `eif_matrix_cholesky()` | Efficient covariance factorization |
| `eif_matrix_mult()` | State and covariance propagation |

## SLAM Considerations

| Factor | Impact |
|--------|--------|
| **Motion Noise** | Higher → faster covariance growth |
| **Sensor Noise** | Higher → less correction per observation |
| **Landmark Density** | More landmarks → better localization |
| **Loop Closure** | Revisiting landmarks dramatically reduces uncertainty |

## Real-World Applications
- Autonomous vehicles
- Warehouse robots
- Vacuum cleaning robots
- Drone mapping
- Underground mining robots

## Run the Demo
```bash
cd build && ./bin/robot_slam_demo
```
