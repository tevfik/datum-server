# Drone Attitude Tutorial: Madgwick Filter & Flight Control

## Learning Objectives

- Madgwick AHRS algorithm for attitude estimation
- Quaternion-based orientation
- PID stabilization loop
- Quadcopter motor mixing

**Level**: Advanced  
**Time**: 50 minutes

---

## 1. Drone Control Stack

```
┌─────────────────────────────────────────────────────────┐
│                    FLIGHT CONTROLLER                     │
├────────────────┬────────────────┬────────────────────────┤
│  Sensors       │   Estimation   │   Control              │
│  ────────      │   ──────────   │   ───────              │
│  IMU (400Hz)   │   Madgwick     │   Rate PID (inner)     │
│  Baro (25Hz)   │   Filter       │   Angle PID (outer)    │
│  GPS (5Hz)     │                │   Motor Mixing         │
└────────────────┴────────────────┴────────────────────────┘
                          ↓
              PWM to ESCs (4 motors)
```

---

## 2. Madgwick Filter

### Why Madgwick?

| Filter | Pros | Cons |
|--------|------|------|
| Complementary | Simple | Tuning dependent |
| Mahony | Fast | Less accurate |
| **Madgwick** | Accurate, efficient | One parameter (β) |
| EKF | Optimal | Complex, expensive |

### Single Parameter (β)

- Low β (0.01): Slow, smooth, less drift correction
- High β (0.1+): Fast, responsive, may oscillate
- Typical drone: β = 0.04-0.1

---

## 3. Motor Mixing

### Quadcopter X Configuration

```
      Front
   FL(CW)  FR(CCW)
      ╲    ╱
       ╲  ╱
    ────╳────
       ╱  ╲
      ╱    ╲
   RL(CCW) RR(CW)
```

### Mixing Table

| Motor | Throttle | Roll | Pitch | Yaw |
|-------|----------|------|-------|-----|
| FL | + | + | + | - |
| FR | + | - | + | + |
| RL | + | + | - | + |
| RR | + | - | - | - |

---

## 4. PID Tuning

### Cascade Control

```
Setpoint (angle) → [Angle PID] → Rate Setpoint → [Rate PID] → Motor Command
```

### Typical Values (small quad)

| Axis | Rate Kp | Rate Ki | Rate Kd | Angle Kp |
|------|---------|---------|---------|----------|
| Roll | 0.7 | 0.3 | 0.02 | 4.0 |
| Pitch | 0.7 | 0.3 | 0.02 | 4.0 |
| Yaw | 0.5 | 0.2 | 0.01 | 2.0 |

---

## 5. ESP32 Implementation

```c
void flight_loop(void* arg) {
    TickType_t last_wake = xTaskGetTickCount();
    
    while (armed) {
        // 400Hz loop
        float accel[3], gyro[3];
        mpu6050_read(&accel, &gyro);
        
        // AHRS update
        madgwick_update(&ahrs, gyro, accel, dt);
        quat_to_euler(&ahrs.q, &roll, &pitch, &yaw);
        
        // Rate PID
        float roll_rate_cmd = angle_pid_update(&pid_roll, rc_roll, roll);
        float pitch_rate_cmd = angle_pid_update(&pid_pitch, rc_pitch, pitch);
        
        // Motor output
        float motors[4];
        mix_motors(throttle, roll_cmd, pitch_cmd, yaw_cmd, motors);
        
        // PWM output
        for (int i = 0; i < 4; i++) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, i, motors[i]);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, i);
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(2));  // 2ms = 400Hz
    }
}
```

---

## Summary

### Key Components
- **Madgwick**: Gradient descent attitude estimation
- **PID**: Cascaded angle/rate control
- **Mixing**: Convert commands to motor outputs

### Performance
- Loop rate: 400-1000Hz
- Latency: <5ms
- Memory: ~2KB
