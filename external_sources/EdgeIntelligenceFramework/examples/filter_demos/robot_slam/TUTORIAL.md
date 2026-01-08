# Robot SLAM Tutorial: 2D Localization and Mapping

## Learning Objectives

By the end of this tutorial, you will understand:
- Simultaneous Localization and Mapping (SLAM) fundamentals
- EKF-SLAM algorithm and state vector structure
- Landmark observation and data association
- Building maps while navigating unknown environments

**Level**: Advanced  
**Prerequisites**: Understanding of Kalman filters, matrix operations  
**Time**: 60-90 minutes

---

## 1. Introduction to SLAM

### The SLAM Problem

A robot needs to:
1. **Localize**: Know where it is (x, y, θ)
2. **Map**: Know where obstacles/landmarks are

**Chicken-and-egg problem**: 
- Need map to localize
- Need location to build map
- Solution: Estimate both simultaneously!

### SLAM Variants

| Method | Pros | Cons | Use Case |
|--------|------|------|----------|
| **EKF-SLAM** | Optimal for Gaussian | O(n²) complexity | <100 landmarks |
| **FastSLAM** | Particle filter | Memory intensive | Medium maps |
| **Graph-SLAM** | Batch optimization | Not real-time | Large maps |
| **ORB-SLAM** | Visual features | CPU intensive | Camera-based |

This tutorial covers **EKF-SLAM** for simplicity.

---

## 2. State Vector

### Robot State

```
x_robot = [x, y, θ]

where:
  x, y = position in world frame
  θ = heading angle (radians)
```

### Landmark States

```
x_landmark_i = [lx_i, ly_i]

Each landmark has 2D position.
```

### Full State Vector

```
x = [x, y, θ, lx₁, ly₁, lx₂, ly₂, ..., lxₙ, lyₙ]
     └──────┘ └─────────────────────────────────┘
      Robot         Landmarks (2n states)
```

For 5 landmarks: 3 + 2×5 = 13 states

---

## 3. Motion Model

### Velocity Model

```c
// Control input: [v, ω] = [linear velocity, angular velocity]
float v = 1.0f;   // m/s
float omega = 0.1f; // rad/s

// State prediction
x_new = x + v * cos(θ) * dt;
y_new = y + v * sin(θ) * dt;
θ_new = θ + ω * dt;
```

### Motion Jacobian

```
        ∂f/∂x:
        
        ┌                    ┐
    F = │ 1  0  -v·sin(θ)·dt │
        │ 0  1   v·cos(θ)·dt │
        │ 0  0        1      │
        └                    ┘
```

---

## 4. Observation Model

### Range-Bearing Sensor

Robot observes landmarks as (range, bearing):

```
        Landmark
           *
          /│
     r   / │ Δy
        /  │
       /   │
      /θ_b │
     ──────┴────
     Robot   Δx

r = √(Δx² + Δy²)       // range
θ_b = atan2(Δy, Δx) - θ  // bearing relative to robot
```

### Observation Jacobian

```
        ∂h/∂x:
        
        ┌                              ┐
    H = │ -Δx/r  -Δy/r   0   Δx/r  Δy/r │  (range)
        │  Δy/r² -Δx/r² -1  -Δy/r² Δx/r²│  (bearing)
        └                              ┘
```

---

## 5. EKF-SLAM Algorithm

### Initialization

```c
eif_ekf_slam_t slam;
eif_ekf_slam_init(&slam, max_landmarks, &pool);

// Initial robot pose
slam.x[0] = 0.0f;  // x
slam.x[1] = 0.0f;  // y
slam.x[2] = 0.0f;  // theta

// Initial covariance (robot pose uncertain)
slam.P[0] = 0.01f;  // x variance
slam.P[4] = 0.01f;  // y variance
slam.P[8] = 0.01f;  // theta variance
```

### Prediction Step

```c
// Control input
float32_t u[2] = {velocity, angular_vel};

// Predict new pose
eif_ekf_slam_predict(&slam, u, dt);
```

**Inside predict**:
1. Apply motion model to robot state
2. Compute Jacobian F
3. Propagate covariance: P = F·P·Fᵀ + Q

### Observation Step

```c
// Observed landmarks (range, bearing, ID)
typedef struct {
    float32_t range;
    float32_t bearing;
    int id;  // -1 if new landmark
} observation_t;

observation_t obs[5] = {...};

// Update
eif_ekf_slam_update(&slam, obs, num_obs);
```

**Inside update**:
1. For each observation:
   - If new landmark: Initialize in state vector
   - Compute expected measurement
   - Compute innovation z - h(x)
   - Compute Jacobian H
   - Kalman gain: K = P·Hᵀ·(H·P·Hᵀ + R)⁻¹
   - Update: x = x + K·(z - h(x))
   - Update: P = (I - K·H)·P

---

## 6. Data Association

### The Problem

"Which observed landmark matches which known landmark?"

### Nearest Neighbor

```c
int associate_landmark(eif_ekf_slam_t* slam, float32_t r, float32_t b) {
    float min_dist = 999.0f;
    int best_id = -1;
    
    for (int i = 0; i < slam->num_landmarks; i++) {
        // Predicted observation for landmark i
        float pred_r, pred_b;
        predict_observation(slam, i, &pred_r, &pred_b);
        
        // Mahalanobis distance
        float dr = r - pred_r;
        float db = normalize_angle(b - pred_b);
        float dist = sqrtf(dr*dr + db*db);
        
        if (dist < min_dist && dist < THRESHOLD) {
            min_dist = dist;
            best_id = i;
        }
    }
    
    return best_id;  // -1 = new landmark
}
```

---

## 7. Code Walkthrough

### Main Loop

```c
int main(void) {
    // Initialize
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, buffer, sizeof(buffer));
    
    eif_ekf_slam_t slam;
    eif_ekf_slam_init(&slam, 20, &pool);
    
    // Simulation loop
    for (int t = 0; t < 100; t++) {
        // Generate control
        float32_t v = 0.5f;
        float32_t omega = 0.1f * sinf(t * 0.1f);
        
        // Predict
        eif_ekf_slam_predict(&slam, (float32_t[]){v, omega}, 0.1f);
        
        // Simulate observations
        observation_t obs[10];
        int n_obs = simulate_observations(&slam, true_landmarks, obs);
        
        // Update
        eif_ekf_slam_update(&slam, obs, n_obs);
        
        // Visualize
        display_map(&slam);
    }
    
    return 0;
}
```

---

## 8. Experiments

### Experiment 1: Landmark Density
Vary number of landmarks and observe map quality.

### Experiment 2: Sensor Noise
Increase range/bearing noise and see degradation.

### Experiment 3: Loop Closure
Have robot return to start - watch uncertainty reduce.

---

## 9. Hardware Deployment

### ESP32 with LIDAR

```c
// Using TFMini LIDAR on UART2
void lidar_task(void* arg) {
    while (1) {
        float range = tfmini_read();
        float bearing = servo_angle;  // Scanning servo
        
        if (range > 0) {
            observation_t obs = {range, bearing, -1};
            xQueueSend(obs_queue, &obs, 0);
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
```

### Resource Requirements

| Component | RAM | Flash |
|-----------|-----|-------|
| SLAM state (20 LM) | 8 KB | - |
| Covariance matrix | 7 KB | - |
| Temp matrices | 4 KB | - |
| Code | - | 15 KB |
| **Total** | **~20 KB** | **~15 KB** |

---

## 10. Summary

### Key Concepts
1. **SLAM**: Simultaneous localization and mapping
2. **State Vector**: Robot pose + landmark positions
3. **EKF**: Optimal fusion of motion and observations
4. **Data Association**: Matching observations to landmarks

### EIF APIs
- `eif_ekf_slam_init()` - Initialize SLAM
- `eif_ekf_slam_predict()` - Motion update
- `eif_ekf_slam_update()` - Observation update
- `eif_ekf_slam_get_map()` - Get landmark map

### Next Steps
- Try UKF-SLAM for better nonlinearity handling
- Implement visual landmarks for ESP32-CAM
- Add loop closure detection
