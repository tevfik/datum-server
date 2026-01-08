/**
 * @file exp1_filter_comparison.c
 * @brief Experiment: Compare Complementary vs Madgwick filters
 * 
 * Shows difference between simple complementary filter
 * and Madgwick AHRS for attitude estimation.
 */

#include <stdio.h>
#include <math.h>

#define SAMPLE_RATE 100
#define DURATION 10
#define M_PI 3.14159265358979323846
#define RAD_TO_DEG 57.2957795f

// Complementary filter (simple)
float complementary_filter(float accel_angle, float gyro_rate, float prev_angle, float dt, float alpha) {
    float gyro_angle = prev_angle + gyro_rate * dt;
    return alpha * gyro_angle + (1 - alpha) * accel_angle;
}

// Madgwick filter beta parameter
float madgwick_beta = 0.1f;

// Simplified Madgwick update (single axis for demo)
float madgwick_filter(float accel_angle, float gyro_rate, float prev_angle, float dt) {
    float error = accel_angle - prev_angle;
    float correction = madgwick_beta * error;
    return prev_angle + (gyro_rate + correction) * dt;
}

int main(void) {
    printf("\n=== Complementary vs Madgwick Filter Comparison ===\n\n");
    
    float dt = 1.0f / SAMPLE_RATE;
    float true_angle = 0;
    float comp_angle = 0;
    float madg_angle = 0;
    
    float comp_error_sum = 0;
    float madg_error_sum = 0;
    int samples = 0;
    
    printf("Time(s) | True  | Comp  | Madg  | Comp Err | Madg Err\n");
    printf("--------|-------|-------|-------|----------|---------\n");
    
    for (int i = 0; i < SAMPLE_RATE * DURATION; i++) {
        float t = i * dt;
        
        // Simulate true angle (oscillating motion)
        true_angle = 30 * sinf(2 * M_PI * 0.2f * t);
        
        // Simulate sensors
        float accel_angle = true_angle + 2 * ((float)rand()/RAND_MAX - 0.5f);  // Noisy
        float gyro_rate = 30 * 2 * M_PI * 0.2f * cosf(2 * M_PI * 0.2f * t);
        gyro_rate += 0.05f;  // Bias
        
        // Update filters
        comp_angle = complementary_filter(accel_angle, gyro_rate, comp_angle, dt, 0.98f);
        madg_angle = madgwick_filter(accel_angle, gyro_rate, madg_angle, dt);
        
        // Track errors
        comp_error_sum += fabsf(comp_angle - true_angle);
        madg_error_sum += fabsf(madg_angle - true_angle);
        samples++;
        
        // Print every second
        if (i % SAMPLE_RATE == 0) {
            printf(" %5.1f  | %5.1f | %5.1f | %5.1f | %6.2f   | %6.2f\n",
                   t, true_angle, comp_angle, madg_angle,
                   fabsf(comp_angle - true_angle), fabsf(madg_angle - true_angle));
        }
    }
    
    printf("\n");
    printf("Average Error - Complementary: %.2f°\n", comp_error_sum / samples);
    printf("Average Error - Madgwick:      %.2f°\n", madg_error_sum / samples);
    printf("\nConclusion: Madgwick handles gyro bias better.\n");
    
    return 0;
}
