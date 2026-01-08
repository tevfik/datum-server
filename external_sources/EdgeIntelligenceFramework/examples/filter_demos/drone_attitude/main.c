/**
 * @file main.c
 * @brief Drone Attitude Estimation Demo
 * 
 * Real-time drone attitude (roll, pitch, yaw) estimation using:
 * - Madgwick filter for sensor fusion
 * - PID control for stabilization
 * - Motor mixing for quadcopter
 * 
 * Supports:
 * - Mock HAL for PC testing
 * - Real HAL for ESP32 deployment
 * - JSON output for visualization
 * 
 * Usage:
 *   ./drone_attitude_demo                          # Interactive mode
 *   ./drone_attitude_demo --json                   # JSON for eif_plotter.py
 *   ./drone_attitude_demo --json | python3 tools/eif_plotter.py --stdin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#include "eif_memory.h"

// Use unified HAL when available
#ifdef EIF_USE_MOCK_HAL
#include "eif_hal.h"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Configuration
// ============================================================================

#define SAMPLE_RATE 400   // Hz
#define DEMO_DURATION 5   // seconds
#define RAD_TO_DEG 57.2957795f
#define DEG_TO_RAD 0.0174532925f

static bool json_mode = false;
static bool continuous_mode = false;
static int sample_count = 0;

// ============================================================================
// Quaternion Math
// ============================================================================

typedef struct {
    float w, x, y, z;
} quat_t;

static void quat_normalize(quat_t* q) {
    float norm = sqrtf(q->w*q->w + q->x*q->x + q->y*q->y + q->z*q->z);
    if (norm > 0.0001f) {
        q->w /= norm;
        q->x /= norm;
        q->y /= norm;
        q->z /= norm;
    }
}

static void quat_to_euler(const quat_t* q, float* roll, float* pitch, float* yaw) {
    *roll = atan2f(2*(q->w*q->x + q->y*q->z), 1 - 2*(q->x*q->x + q->y*q->y));
    *pitch = asinf(fmaxf(-1.0f, fminf(1.0f, 2*(q->w*q->y - q->z*q->x))));
    *yaw = atan2f(2*(q->w*q->z + q->x*q->y), 1 - 2*(q->y*q->y + q->z*q->z));
}

// ============================================================================
// Madgwick Filter
// ============================================================================

typedef struct {
    quat_t q;
    float beta;
} madgwick_t;

static void madgwick_init(madgwick_t* m, float beta) {
    m->q.w = 1.0f;
    m->q.x = m->q.y = m->q.z = 0.0f;
    m->beta = beta;
}

static void madgwick_update(madgwick_t* m, float gx, float gy, float gz,
                            float ax, float ay, float az, float dt) {
    quat_t* q = &m->q;
    
    // Normalize accelerometer
    float norm = sqrtf(ax*ax + ay*ay + az*az);
    if (norm < 0.001f) return;
    ax /= norm; ay /= norm; az /= norm;
    
    // Gradient descent step
    float f1 = 2*(q->x*q->z - q->w*q->y) - ax;
    float f2 = 2*(q->w*q->x + q->y*q->z) - ay;
    float f3 = 2*(0.5f - q->x*q->x - q->y*q->y) - az;
    
    float j11 = 2*q->y, j12 = 2*q->z, j13 = 2*q->w, j14 = 2*q->x;
    float j21 = 2*q->w, j22 = 2*q->x, j23 = 2*q->y, j24 = 2*q->z;
    float j32 = 4*q->x, j33 = 4*q->y;
    
    float step_w = j11*f1 + j21*f2;
    float step_x = j12*f1 + j22*f2 + j32*f3;
    float step_y = j13*f1 + j23*f2 + j33*f3;
    float step_z = j14*f1 + j24*f2;
    
    norm = sqrtf(step_w*step_w + step_x*step_x + step_y*step_y + step_z*step_z);
    if (norm > 0.001f) {
        step_w /= norm; step_x /= norm; step_y /= norm; step_z /= norm;
    }
    
    // Quaternion derivative from gyro
    float qDot_w = 0.5f * (-q->x*gx - q->y*gy - q->z*gz);
    float qDot_x = 0.5f * ( q->w*gx + q->y*gz - q->z*gy);
    float qDot_y = 0.5f * ( q->w*gy - q->x*gz + q->z*gx);
    float qDot_z = 0.5f * ( q->w*gz + q->x*gy - q->y*gx);
    
    // Apply feedback
    qDot_w -= m->beta * step_w;
    qDot_x -= m->beta * step_x;
    qDot_y -= m->beta * step_y;
    qDot_z -= m->beta * step_z;
    
    // Integrate
    q->w += qDot_w * dt;
    q->x += qDot_x * dt;
    q->y += qDot_y * dt;
    q->z += qDot_z * dt;
    
    quat_normalize(q);
}

// ============================================================================
// Motor Mixing
// ============================================================================

typedef struct {
    float throttle;
    float motor[4];  // FL, FR, RL, RR
} motor_output_t;

static void mix_motors(float throttle, float roll_cmd, float pitch_cmd, float yaw_cmd,
                       motor_output_t* out) {
    out->throttle = throttle;
    out->motor[0] = throttle + roll_cmd + pitch_cmd - yaw_cmd;  // FL
    out->motor[1] = throttle - roll_cmd + pitch_cmd + yaw_cmd;  // FR
    out->motor[2] = throttle + roll_cmd - pitch_cmd + yaw_cmd;  // RL
    out->motor[3] = throttle - roll_cmd - pitch_cmd - yaw_cmd;  // RR
    
    for (int i = 0; i < 4; i++) {
        if (out->motor[i] < 0) out->motor[i] = 0;
        if (out->motor[i] > 100) out->motor[i] = 100;
    }
}

// ============================================================================
// PID Controller
// ============================================================================

typedef struct {
    float kp, ki, kd;
    float integral;
    float prev_error;
} pid_ctrl_t;

static void pid_init(pid_ctrl_t* pid, float kp, float ki, float kd) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0;
    pid->prev_error = 0;
}

static float pid_update(pid_ctrl_t* pid, float setpoint, float measured, float dt) {
    float error = setpoint - measured;
    pid->integral += error * dt;
    
    if (pid->integral > 50) pid->integral = 50;
    if (pid->integral < -50) pid->integral = -50;
    
    float derivative = (error - pid->prev_error) / dt;
    pid->prev_error = error;
    
    return pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
}

// ============================================================================
// IMU Simulation (fallback when HAL not available)
// ============================================================================

static void simulate_imu(float* accel, float* gyro, float roll, float pitch, int step) {
    accel[0] = 9.81f * sinf(pitch) + 0.1f * ((float)rand()/RAND_MAX - 0.5f);
    accel[1] = -9.81f * sinf(roll) * cosf(pitch) + 0.1f * ((float)rand()/RAND_MAX - 0.5f);
    accel[2] = -9.81f * cosf(roll) * cosf(pitch) + 0.1f * ((float)rand()/RAND_MAX - 0.5f);
    
    gyro[0] = 0.1f * sinf(step * 0.01f) + 0.01f * ((float)rand()/RAND_MAX - 0.5f);
    gyro[1] = 0.15f * sinf(step * 0.02f) + 0.01f * ((float)rand()/RAND_MAX - 0.5f);
    gyro[2] = 0.05f * sinf(step * 0.005f) + 0.01f * ((float)rand()/RAND_MAX - 0.5f);
}

// ============================================================================
// JSON Output
// ============================================================================

static void output_json(float time_s, float roll, float pitch, float yaw,
                        float* accel, float* gyro, motor_output_t* motors) {
    printf("{\"timestamp\": %d, \"type\": \"drone\"", sample_count++);
    
    // Attitude
    printf(", \"signals\": {\"roll\": %.2f, \"pitch\": %.2f, \"yaw\": %.2f}", roll, pitch, yaw);
    
    // IMU data
    printf(", \"imu\": {\"ax\": %.3f, \"ay\": %.3f, \"az\": %.3f, \"gx\": %.3f, \"gy\": %.3f, \"gz\": %.3f}",
           accel[0], accel[1], accel[2], gyro[0], gyro[1], gyro[2]);
    
    // Motors
    printf(", \"motors\": [%.1f, %.1f, %.1f, %.1f]",
           motors->motor[0], motors->motor[1], motors->motor[2], motors->motor[3]);
    
    // Stability indicator
    float stability = 100.0f - (fabsf(roll) + fabsf(pitch)) * 2;
    if (stability < 0) stability = 0;
    printf(", \"state\": {\"stability\": %.1f}", stability);
    
    printf("}\n");
    fflush(stdout);
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON for real-time plotting\n");
    printf("  --continuous  Run without display tables\n");
    printf("  --duration N  Duration in seconds (default: 5)\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
    printf("  %s --continuous --duration 10\n", prog);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    int duration = DEMO_DURATION;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--continuous") == 0) {
            continuous_mode = true;
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    srand(time(NULL));
    
    if (!json_mode) {
        printf("\n");
        printf("+================================================================+\n");
        printf("|     Drone Attitude Estimation Demo - Madgwick + PID           |\n");
        printf("+================================================================+\n\n");
    }
    
    // Initialize HAL if available
#ifdef EIF_USE_MOCK_HAL
    eif_imu_config_t imu_cfg = {
        .sample_rate_hz = SAMPLE_RATE,
        .accel_range_g = 4.0f,
        .gyro_range_dps = 500.0f,
        .use_magnetometer = false
    };
    eif_hal_imu_init(&imu_cfg);
    if (!json_mode) {
        printf("Using: %s\n", eif_hal_get_platform_name());
    }
#endif
    
    madgwick_t ahrs;
    madgwick_init(&ahrs, 0.1f);
    
    pid_ctrl_t pid_roll, pid_pitch, pid_yaw;
    pid_init(&pid_roll, 5.0f, 0.5f, 0.1f);
    pid_init(&pid_pitch, 5.0f, 0.5f, 0.1f);
    pid_init(&pid_yaw, 3.0f, 0.2f, 0.05f);
    
    float dt = 1.0f / SAMPLE_RATE;
    int total_samples = duration * SAMPLE_RATE;
    
    if (!json_mode) {
        printf("Sample rate: %d Hz\n", SAMPLE_RATE);
        printf("Filter: Madgwick (beta=0.1)\n");
        printf("PID: Kp=5.0, Ki=0.5, Kd=0.1\n\n");
        
        printf("+---------+---------------------------+----------------------------+\n");
        printf("|  Time   |  Attitude (Roll/Pitch/Yaw)|  Motors (FL/FR/RL/RR)      |\n");
        printf("+---------+---------------------------+----------------------------+\n");
    }
    
    float true_roll = 0, true_pitch = 0;
    
    for (int s = 0; s < total_samples; s++) {
        true_roll += 0.001f * sinf(s * 0.01f);
        true_pitch += 0.001f * sinf(s * 0.02f);
        
        float accel[3], gyro[3];
        
#ifdef EIF_USE_MOCK_HAL
        eif_imu_data_t imu_data;
        eif_hal_imu_read(&imu_data);
        accel[0] = imu_data.ax * 9.81f;
        accel[1] = imu_data.ay * 9.81f;
        accel[2] = imu_data.az * 9.81f;
        gyro[0] = imu_data.gx;
        gyro[1] = imu_data.gy;
        gyro[2] = imu_data.gz;
#else
        simulate_imu(accel, gyro, true_roll, true_pitch, s);
#endif
        
        madgwick_update(&ahrs, gyro[0], gyro[1], gyro[2],
                        accel[0], accel[1], accel[2], dt);
        
        float roll, pitch, yaw;
        quat_to_euler(&ahrs.q, &roll, &pitch, &yaw);
        roll *= RAD_TO_DEG;
        pitch *= RAD_TO_DEG;
        yaw *= RAD_TO_DEG;
        
        float roll_cmd = pid_update(&pid_roll, 0, roll, dt);
        float pitch_cmd = pid_update(&pid_pitch, 0, pitch, dt);
        float yaw_cmd = pid_update(&pid_yaw, 0, yaw, dt);
        
        motor_output_t motors;
        mix_motors(50.0f, roll_cmd, pitch_cmd, yaw_cmd, &motors);
        
        // Output every 100ms
        if (s % (SAMPLE_RATE / 10) == 0) {
            if (json_mode) {
                output_json((float)s / SAMPLE_RATE, roll, pitch, yaw, accel, gyro, &motors);
            } else {
                printf("|  %5.2fs |  R:%+5.1f P:%+5.1f Y:%+5.1f |  %4.0f  %4.0f  %4.0f  %4.0f     |\n",
                       (float)s / SAMPLE_RATE,
                       roll, pitch, yaw,
                       motors.motor[0], motors.motor[1], motors.motor[2], motors.motor[3]);
            }
        }
    }
    
    if (!json_mode) {
        printf("+---------+---------------------------+----------------------------+\n\n");
        
        printf("+----------------------------------------------------------------+\n");
        printf("|  FOR DRONE DEPLOYMENT:                                         |\n");
        printf("|  1. MPU6050/BMI160 at 400-1000Hz                               |\n");
        printf("|  2. PWM output to ESCs (50-400Hz)                              |\n");
        printf("|  3. Add rate-mode inner loop                                   |\n");
        printf("|  4. Add RC receiver input                                      |\n");
        printf("|  5. Add failsafe and arming logic                              |\n");
        printf("+----------------------------------------------------------------+\n\n");
    } else {
        printf("{\"type\": \"summary\", \"duration\": %d, \"samples\": %d, \"sample_rate\": %d}\n",
               duration, total_samples, SAMPLE_RATE);
    }
    
    return 0;
}
