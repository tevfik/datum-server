/**
 * @file exp2_noise_analysis.c
 * @brief Experiment: IMU sensor noise characterization
 * 
 * Analyzes accelerometer and gyroscope noise statistics
 * to determine appropriate filter parameters.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define NUM_SAMPLES 1000

typedef struct {
    float mean;
    float variance;
    float std_dev;
    float min_val;
    float max_val;
} noise_stats_t;

void calculate_stats(const float* data, int n, noise_stats_t* stats) {
    stats->mean = 0;
    stats->min_val = data[0];
    stats->max_val = data[0];
    
    for (int i = 0; i < n; i++) {
        stats->mean += data[i];
        if (data[i] < stats->min_val) stats->min_val = data[i];
        if (data[i] > stats->max_val) stats->max_val = data[i];
    }
    stats->mean /= n;
    
    stats->variance = 0;
    for (int i = 0; i < n; i++) {
        float d = data[i] - stats->mean;
        stats->variance += d * d;
    }
    stats->variance /= n;
    stats->std_dev = sqrtf(stats->variance);
}

// Generate Gaussian noise
float gaussian_noise(float mean, float std) {
    float u1 = (float)rand() / RAND_MAX;
    float u2 = (float)rand() / RAND_MAX;
    float z = sqrtf(-2 * logf(u1)) * cosf(2 * 3.14159f * u2);
    return mean + std * z;
}

int main(void) {
    srand(time(NULL));
    
    printf("\n=== IMU Sensor Noise Analysis ===\n\n");
    
    float accel_data[NUM_SAMPLES];
    float gyro_data[NUM_SAMPLES];
    
    // Simulate stationary IMU readings
    // Typical noise values from MPU6050 datasheet
    for (int i = 0; i < NUM_SAMPLES; i++) {
        accel_data[i] = gaussian_noise(0, 0.05f);  // 50 mg noise
        gyro_data[i] = gaussian_noise(0, 0.01f);   // 0.01 rad/s noise
    }
    
    noise_stats_t accel_stats, gyro_stats;
    calculate_stats(accel_data, NUM_SAMPLES, &accel_stats);
    calculate_stats(gyro_data, NUM_SAMPLES, &gyro_stats);
    
    printf("Accelerometer Noise (stationary):\n");
    printf("  Mean:     %8.5f m/s²\n", accel_stats.mean);
    printf("  Std Dev:  %8.5f m/s²\n", accel_stats.std_dev);
    printf("  Range:    [%.5f, %.5f] m/s²\n", accel_stats.min_val, accel_stats.max_val);
    
    printf("\nGyroscope Noise (stationary):\n");
    printf("  Mean:     %8.5f rad/s\n", gyro_stats.mean);
    printf("  Std Dev:  %8.5f rad/s\n", gyro_stats.std_dev);
    printf("  Range:    [%.5f, %.5f] rad/s\n", gyro_stats.min_val, gyro_stats.max_val);
    
    printf("\nRecommended Filter Parameters:\n");
    printf("  Accel noise (R): %.6f\n", accel_stats.variance);
    printf("  Gyro noise (Q):  %.6f\n", gyro_stats.variance);
    printf("  Madgwick beta:   %.3f\n", 0.1f * accel_stats.std_dev / gyro_stats.std_dev);
    
    return 0;
}
