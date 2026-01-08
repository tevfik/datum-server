/**
 * @file main.c
 * @brief Fitness Tracker Project - IMU-Based Activity Recognition
 * 
 * Complete wearable fitness tracker combining:
 * - Activity recognition (walking, running, cycling, idle)
 * - Step counting
 * - Calorie estimation
 * - Exercise session tracking
 * 
 * Target: ESP32 with MPU6050/BMI160 IMU
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "eif_memory.h"

// ============================================================================
// Configuration
// ============================================================================

#define SAMPLE_RATE 50      // Hz
#define WINDOW_SIZE 100     // 2 seconds
#define NUM_FEATURES 12

// ============================================================================
// Activity Types
// ============================================================================

typedef enum {
    ACTIVITY_IDLE = 0,
    ACTIVITY_WALKING,
    ACTIVITY_RUNNING,
    ACTIVITY_CYCLING,
    NUM_ACTIVITIES
} activity_t;

static const char* activity_names[] = {
    "IDLE", "WALKING", "RUNNING", "CYCLING"
};

static const float calories_per_minute[] = {
    1.0f,   // Idle
    4.0f,   // Walking
    10.0f,  // Running
    6.0f    // Cycling
};

// ============================================================================
// Fitness State
// ============================================================================

typedef struct {
    // Current activity
    activity_t current_activity;
    int activity_confidence;
    
    // Step tracking
    int total_steps;
    float last_accel_mag;
    int step_state;  // 0=low, 1=high
    
    // Session stats
    float session_minutes;
    float calories_burned;
    int activity_minutes[NUM_ACTIVITIES];
    
    // Sensor window
    float accel_window[WINDOW_SIZE * 3];
    float gyro_window[WINDOW_SIZE * 3];
    int window_idx;
    int window_full;
} fitness_tracker_t;

static uint8_t pool_buffer[32 * 1024];
static eif_memory_pool_t pool;

// ============================================================================
// IMU Simulation
// ============================================================================

static void generate_imu_data(float* accel, float* gyro, activity_t activity, int time_step) {
    float t = time_step * 0.02f;  // 50Hz
    
    switch (activity) {
        case ACTIVITY_IDLE:
            accel[0] = 0.1f * ((float)rand()/RAND_MAX - 0.5f);
            accel[1] = 0.1f * ((float)rand()/RAND_MAX - 0.5f);
            accel[2] = 9.8f + 0.1f * ((float)rand()/RAND_MAX - 0.5f);
            gyro[0] = 0.02f * ((float)rand()/RAND_MAX - 0.5f);
            gyro[1] = 0.02f * ((float)rand()/RAND_MAX - 0.5f);
            gyro[2] = 0.02f * ((float)rand()/RAND_MAX - 0.5f);
            break;
            
        case ACTIVITY_WALKING:
            accel[0] = 1.5f * sinf(2 * M_PI * 1.8f * t) + 0.3f * ((float)rand()/RAND_MAX - 0.5f);
            accel[1] = 0.5f * sinf(2 * M_PI * 3.6f * t) + 0.2f * ((float)rand()/RAND_MAX - 0.5f);
            accel[2] = 9.8f + 3.0f * fabsf(sinf(2 * M_PI * 1.8f * t));
            gyro[0] = 0.3f * sinf(2 * M_PI * 1.8f * t);
            gyro[1] = 0.1f * sinf(2 * M_PI * 1.8f * t);
            gyro[2] = 0.2f * sinf(2 * M_PI * 0.9f * t);
            break;
            
        case ACTIVITY_RUNNING:
            accel[0] = 3.0f * sinf(2 * M_PI * 2.5f * t) + 0.5f * ((float)rand()/RAND_MAX - 0.5f);
            accel[1] = 1.5f * sinf(2 * M_PI * 5.0f * t) + 0.3f * ((float)rand()/RAND_MAX - 0.5f);
            accel[2] = 9.8f + 6.0f * fabsf(sinf(2 * M_PI * 2.5f * t));
            gyro[0] = 0.8f * sinf(2 * M_PI * 2.5f * t);
            gyro[1] = 0.3f * sinf(2 * M_PI * 2.5f * t);
            gyro[2] = 0.5f * sinf(2 * M_PI * 1.25f * t);
            break;
            
        case ACTIVITY_CYCLING:
            accel[0] = 0.5f * sinf(2 * M_PI * 1.2f * t) + 0.2f * ((float)rand()/RAND_MAX - 0.5f);
            accel[1] = 0.3f * sinf(2 * M_PI * 2.4f * t) + 0.1f * ((float)rand()/RAND_MAX - 0.5f);
            accel[2] = 9.8f + 0.8f * sinf(2 * M_PI * 1.2f * t);
            gyro[0] = 1.5f * sinf(2 * M_PI * 1.2f * t);  // Leg rotation
            gyro[1] = 0.1f * sinf(2 * M_PI * 0.6f * t);
            gyro[2] = 0.2f * sinf(2 * M_PI * 0.6f * t);
            break;
            
        default:
            memset(accel, 0, 3 * sizeof(float));
            memset(gyro, 0, 3 * sizeof(float));
    }
}

// ============================================================================
// Feature Extraction
// ============================================================================

static void extract_features(const float* accel, const float* gyro, int n, float* features) {
    // Compute statistics
    float accel_mean[3] = {0}, accel_std[3] = {0};
    float gyro_mean[3] = {0}, gyro_std[3] = {0};
    float accel_mag_mean = 0, accel_mag_max = 0;
    
    // Means
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 3; j++) {
            accel_mean[j] += accel[i*3 + j];
            gyro_mean[j] += gyro[i*3 + j];
        }
        float mag = sqrtf(accel[i*3]*accel[i*3] + accel[i*3+1]*accel[i*3+1] + accel[i*3+2]*accel[i*3+2]);
        accel_mag_mean += mag;
        if (mag > accel_mag_max) accel_mag_max = mag;
    }
    for (int j = 0; j < 3; j++) {
        accel_mean[j] /= n;
        gyro_mean[j] /= n;
    }
    accel_mag_mean /= n;
    
    // Std devs
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 3; j++) {
            float da = accel[i*3 + j] - accel_mean[j];
            float dg = gyro[i*3 + j] - gyro_mean[j];
            accel_std[j] += da * da;
            gyro_std[j] += dg * dg;
        }
    }
    for (int j = 0; j < 3; j++) {
        accel_std[j] = sqrtf(accel_std[j] / n);
        gyro_std[j] = sqrtf(gyro_std[j] / n);
    }
    
    // Feature vector
    features[0] = accel_mean[0];
    features[1] = accel_mean[1];
    features[2] = accel_mean[2];
    features[3] = accel_std[0];
    features[4] = accel_std[1];
    features[5] = accel_std[2];
    features[6] = gyro_std[0];
    features[7] = gyro_std[1];
    features[8] = gyro_std[2];
    features[9] = accel_mag_mean;
    features[10] = accel_mag_max;
    features[11] = accel_std[0] + accel_std[1] + accel_std[2];  // Total activity
}

// ============================================================================
// Activity Classification
// ============================================================================

static activity_t classify_activity(const float* features, int* confidence) {
    float total_motion = features[11];
    float z_accel_std = features[5];
    float gyro_x_std = features[6];
    
    activity_t act;
    int conf;
    
    if (total_motion < 0.5f) {
        act = ACTIVITY_IDLE;
        conf = 95;
    } else if (gyro_x_std > 1.0f && z_accel_std < 2.0f) {
        act = ACTIVITY_CYCLING;
        conf = 80;
    } else if (z_accel_std > 4.0f) {
        act = ACTIVITY_RUNNING;
        conf = 85;
    } else {
        act = ACTIVITY_WALKING;
        conf = 90;
    }
    
    *confidence = conf;
    return act;
}

// ============================================================================
// Step Detection
// ============================================================================

static int detect_step(fitness_tracker_t* ft, const float* accel) {
    float mag = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
    int step = 0;
    
    // Simple threshold-based step detection
    if (ft->step_state == 0 && mag > 12.0f) {
        ft->step_state = 1;
    } else if (ft->step_state == 1 && mag < 9.0f) {
        ft->step_state = 0;
        step = 1;
    }
    
    ft->last_accel_mag = mag;
    return step;
}

// ============================================================================
// Tracker Functions
// ============================================================================

static void tracker_init(fitness_tracker_t* ft) {
    memset(ft, 0, sizeof(fitness_tracker_t));
    ft->current_activity = ACTIVITY_IDLE;
}

static void tracker_update(fitness_tracker_t* ft, const float* accel, const float* gyro) {
    // Add to window
    memcpy(&ft->accel_window[ft->window_idx * 3], accel, 3 * sizeof(float));
    memcpy(&ft->gyro_window[ft->window_idx * 3], gyro, 3 * sizeof(float));
    ft->window_idx = (ft->window_idx + 1) % WINDOW_SIZE;
    if (ft->window_idx == 0) ft->window_full = 1;
    
    // Step detection
    ft->total_steps += detect_step(ft, accel);
    
    // Classify activity when window is full
    if (ft->window_full && ft->window_idx == 0) {
        float features[NUM_FEATURES];
        extract_features(ft->accel_window, ft->gyro_window, WINDOW_SIZE, features);
        ft->current_activity = classify_activity(features, &ft->activity_confidence);
    }
    
    // Update stats
    float dt = 1.0f / SAMPLE_RATE / 60.0f;  // Minutes per sample
    ft->session_minutes += dt;
    ft->calories_burned += calories_per_minute[ft->current_activity] * dt;
    ft->activity_minutes[ft->current_activity] += 1;
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    srand(time(NULL));
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║       Fitness Tracker Project - Activity Recognition           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    fitness_tracker_t tracker;
    tracker_init(&tracker);
    
    printf("Simulating 5-minute workout session...\n\n");
    
    // Workout schedule (seconds)
    // 0-30: idle, 30-90: walking, 90-150: running, 150-210: cycling, 210-300: walking
    
    int total_samples = 5 * 60 * SAMPLE_RATE;  // 5 minutes
    
    printf("┌─────────┬────────────┬──────────┬────────┬──────────┐\n");
    printf("│  Time   │  Activity  │  Steps   │  Cal   │   Conf   │\n");
    printf("├─────────┼────────────┼──────────┼────────┼──────────┤\n");
    
    int last_print = -10;
    
    for (int s = 0; s < total_samples; s++) {
        float t_sec = (float)s / SAMPLE_RATE;
        
        // Determine simulated activity
        activity_t sim_activity;
        if (t_sec < 30) sim_activity = ACTIVITY_IDLE;
        else if (t_sec < 90) sim_activity = ACTIVITY_WALKING;
        else if (t_sec < 150) sim_activity = ACTIVITY_RUNNING;
        else if (t_sec < 210) sim_activity = ACTIVITY_CYCLING;
        else sim_activity = ACTIVITY_WALKING;
        
        float accel[3], gyro[3];
        generate_imu_data(accel, gyro, sim_activity, s);
        
        tracker_update(&tracker, accel, gyro);
        
        // Print every 30 seconds
        int t_int = (int)t_sec;
        if (t_int % 30 == 0 && t_int != last_print) {
            printf("│  %3d s  │  %-8s  │  %5d   │ %5.1f  │   %3d%%   │\n",
                   t_int, activity_names[tracker.current_activity],
                   tracker.total_steps, tracker.calories_burned,
                   tracker.activity_confidence);
            last_print = t_int;
        }
    }
    
    printf("└─────────┴────────────┴──────────┴────────┴──────────┘\n\n");
    
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│  WORKOUT SUMMARY                                                │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│  Duration:        %.1f minutes                                  │\n", tracker.session_minutes);
    printf("│  Total Steps:     %d                                           │\n", tracker.total_steps);
    printf("│  Calories Burned: %.1f kcal                                    │\n", tracker.calories_burned);
    printf("│                                                                 │\n");
    printf("│  Activity Breakdown:                                            │\n");
    printf("│    Idle:     %3d seconds                                       │\n", tracker.activity_minutes[0] / 50);
    printf("│    Walking:  %3d seconds                                       │\n", tracker.activity_minutes[1] / 50);
    printf("│    Running:  %3d seconds                                       │\n", tracker.activity_minutes[2] / 50);
    printf("│    Cycling:  %3d seconds                                       │\n", tracker.activity_minutes[3] / 50);
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    return 0;
}
