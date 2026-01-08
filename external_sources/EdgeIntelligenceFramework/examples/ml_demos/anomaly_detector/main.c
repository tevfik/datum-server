/**
 * @file main.c
 * @brief Anomaly Detection Tutorial - Industrial Sensor Monitoring
 * 
 * This tutorial demonstrates how to detect anomalies in sensor data
 * using statistical methods and machine learning.
 * 
 * SCENARIO:
 * A factory monitors temperature and vibration sensors.
 * We detect anomalies that may indicate equipment failure.
 * 
 * FEATURES DEMONSTRATED:
 * - Online statistics (mean, variance)
 * - Z-score anomaly detection
 * - K-Means clustering for anomaly detection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_data_analysis.h"
#include "../common/ascii_plot.h"

// ============================================================================
// Configuration
// ============================================================================

#define NUM_SAMPLES     100
#define NUM_FEATURES    2       // Temperature, Vibration
#define ANOMALY_THRESHOLD 2.5f  // Z-score threshold

// ============================================================================
// Sensor Data Generator
// ============================================================================

typedef struct {
    float32_t temperature;
    float32_t vibration;
    int is_anomaly;  // Ground truth
} sensor_sample_t;

static void generate_sensor_data(sensor_sample_t* samples, int len) {
    // Normal operating ranges
    float base_temp = 60.0f;   // 60°C normal
    float base_vib = 0.5f;     // 0.5g normal
    
    for (int i = 0; i < len; i++) {
        samples[i].is_anomaly = 0;
        
        // Normal data with noise
        samples[i].temperature = base_temp + 5.0f * ((float)rand()/RAND_MAX - 0.5f);
        samples[i].vibration = base_vib + 0.2f * ((float)rand()/RAND_MAX - 0.5f);
        
        // Inject anomalies (10% of data)
        if (rand() % 10 == 0) {
            int anomaly_type = rand() % 3;
            samples[i].is_anomaly = 1;
            
            switch (anomaly_type) {
                case 0:  // Overheating
                    samples[i].temperature = base_temp + 20.0f + 5.0f * (float)rand()/RAND_MAX;
                    break;
                case 1:  // High vibration
                    samples[i].vibration = base_vib + 1.0f + 0.5f * (float)rand()/RAND_MAX;
                    break;
                case 2:  // Both
                    samples[i].temperature = base_temp + 15.0f;
                    samples[i].vibration = base_vib + 0.8f;
                    break;
            }
        }
    }
}

// ============================================================================
// Online Statistics for Anomaly Detection
// ============================================================================

typedef struct {
    float32_t mean;
    float32_t variance;
    float32_t M2;   // For Welford's algorithm
    int count;
} online_stats_t;

static void stats_init(online_stats_t* s) {
    s->mean = 0;
    s->variance = 0;
    s->M2 = 0;
    s->count = 0;
}

static void stats_update(online_stats_t* s, float32_t value) {
    s->count++;
    float32_t delta = value - s->mean;
    s->mean += delta / s->count;
    float32_t delta2 = value - s->mean;
    s->M2 += delta * delta2;
    s->variance = (s->count > 1) ? s->M2 / (s->count - 1) : 0;
}

static float32_t stats_zscore(const online_stats_t* s, float32_t value) {
    float32_t std = sqrtf(s->variance);
    return (std > 0) ? fabsf(value - s->mean) / std : 0;
}

// ============================================================================
// Visualization
// ============================================================================

static void display_scatter(const sensor_sample_t* samples, int len, const int* predictions) {
    char plot[20][50];
    memset(plot, ' ', sizeof(plot));
    
    // Scale factors
    float temp_min = 50, temp_max = 90;
    float vib_min = 0, vib_max = 2.0f;
    
    for (int i = 0; i < len; i++) {
        int x = (int)((samples[i].temperature - temp_min) / (temp_max - temp_min) * 48);
        int y = 19 - (int)((samples[i].vibration - vib_min) / (vib_max - vib_min) * 19);
        
        if (x >= 0 && x < 49 && y >= 0 && y < 20) {
            if (predictions && predictions[i]) {
                plot[y][x] = 'X';  // Detected anomaly
            } else if (samples[i].is_anomaly) {
                plot[y][x] = '?';  // Missed anomaly
            } else {
                plot[y][x] = '.';  // Normal
            }
        }
    }
    
    printf("\n  %sTemperature vs Vibration%s\n", ASCII_BOLD, ASCII_RESET);
    printf("  Vib\n");
    for (int y = 0; y < 20; y++) {
        printf("  │");
        for (int x = 0; x < 49; x++) {
            char c = plot[y][x];
            if (c == 'X') printf("%s%c%s", ASCII_RED, c, ASCII_RESET);
            else if (c == '?') printf("%s%c%s", ASCII_YELLOW, c, ASCII_RESET);
            else if (c == '.') printf("%s%c%s", ASCII_GREEN, c, ASCII_RESET);
            else printf("%c", c);
        }
        printf("│\n");
    }
    printf("  └");
    for (int x = 0; x < 49; x++) printf("─");
    printf("┘ Temp\n");
    
    printf("\n  %sLegend:%s  %s.%s = Normal  %sX%s = Detected Anomaly  %s?%s = Missed\n",
           ASCII_BOLD, ASCII_RESET, ASCII_GREEN, ASCII_RESET, 
           ASCII_RED, ASCII_RESET, ASCII_YELLOW, ASCII_RESET);
}

// ============================================================================
// Main Tutorial
// ============================================================================

int main(void) {
    srand(time(NULL));
    
    printf("\n");
    ascii_section("EIF Tutorial: Anomaly Detection for Industrial Monitoring");
    
    printf("  This tutorial demonstrates anomaly detection for sensor data.\n\n");
    printf("  %sScenario:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    A factory monitors temperature and vibration sensors.\n");
    printf("    Anomalies may indicate equipment failures!\n\n");
    printf("  %sMethods:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    1. Online Statistics (Z-Score)\n");
    printf("    2. K-Means Clustering\n");
    printf("\n  Press Enter to continue...");
    getchar();
    
    // Generate data
    static sensor_sample_t samples[NUM_SAMPLES];
    generate_sensor_data(samples, NUM_SAMPLES);
    
    // Count true anomalies
    int true_anomalies = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (samples[i].is_anomaly) true_anomalies++;
    }
    
    // ========================================================================
    // Method 1: Z-Score Anomaly Detection
    // ========================================================================
    ascii_section("Method 1: Z-Score Anomaly Detection");
    
    printf("  Using online statistics to track mean and variance.\n");
    printf("  Anomalies are points with Z-score > %.1f\n\n", ANOMALY_THRESHOLD);
    
    online_stats_t temp_stats, vib_stats;
    stats_init(&temp_stats);
    stats_init(&vib_stats);
    
    static int zscore_predictions[NUM_SAMPLES];
    int zscore_detected = 0, zscore_tp = 0, zscore_fp = 0;
    
    // Train on first half
    printf("  Training on first 50 samples...\n");
    for (int i = 0; i < NUM_SAMPLES / 2; i++) {
        if (!samples[i].is_anomaly) {  // Only train on normal data
            stats_update(&temp_stats, samples[i].temperature);
            stats_update(&vib_stats, samples[i].vibration);
        }
    }
    
    printf("    Temperature: mean=%.1f°C, std=%.2f\n", temp_stats.mean, sqrtf(temp_stats.variance));
    printf("    Vibration:   mean=%.2fg, std=%.3f\n\n", vib_stats.mean, sqrtf(vib_stats.variance));
    
    // Test on all data
    printf("  Testing on all %d samples...\n", NUM_SAMPLES);
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float32_t z_temp = stats_zscore(&temp_stats, samples[i].temperature);
        float32_t z_vib = stats_zscore(&vib_stats, samples[i].vibration);
        
        zscore_predictions[i] = (z_temp > ANOMALY_THRESHOLD || z_vib > ANOMALY_THRESHOLD);
        
        if (zscore_predictions[i]) {
            zscore_detected++;
            if (samples[i].is_anomaly) zscore_tp++;
            else zscore_fp++;
        }
    }
    
    display_scatter(samples, NUM_SAMPLES, zscore_predictions);
    
    printf("\n  %s┌─ Z-Score Results ────────────────────────────┐%s\n", ASCII_CYAN, ASCII_RESET);
    printf("  │  True Anomalies:     %3d                     │\n", true_anomalies);
    printf("  │  Detected Anomalies: %3d                     │\n", zscore_detected);
    printf("  │  True Positives:     %3d                     │\n", zscore_tp);
    printf("  │  False Positives:    %3d                     │\n", zscore_fp);
    printf("  │  Precision:          %5.1f%%                  │\n", 
           zscore_detected > 0 ? 100.0f * zscore_tp / zscore_detected : 0);
    printf("  │  Recall:             %5.1f%%                  │\n", 
           true_anomalies > 0 ? 100.0f * zscore_tp / true_anomalies : 0);
    printf("  %s└───────────────────────────────────────────────┘%s\n", ASCII_CYAN, ASCII_RESET);
    
    printf("\n  Press Enter to continue to K-Means...");
    getchar();
    
    // ========================================================================
    // Method 2: K-Means Clustering
    // ========================================================================
    ascii_section("Method 2: K-Means Clustering");
    
    printf("  Using K-Means to find clusters.\n");
    printf("  Points far from cluster centers are anomalies.\n\n");
    
    // Initialize memory pool
    static uint8_t pool_buffer[32 * 1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Prepare data matrix (N x 2)
    float32_t* data = eif_memory_alloc(&pool, NUM_SAMPLES * 2 * sizeof(float32_t), 4);
    for (int i = 0; i < NUM_SAMPLES; i++) {
        data[i * 2]     = samples[i].temperature / 100.0f;  // Normalize
        data[i * 2 + 1] = samples[i].vibration;
    }
    
    // Run K-Means with K=2 (normal cluster + anomaly cluster)
    float32_t* centroids = eif_memory_alloc(&pool, 2 * 2 * sizeof(float32_t), 4);
    int* labels = eif_memory_alloc(&pool, NUM_SAMPLES * sizeof(int), 4);
    
    // Initialize centroids with first two data points
    centroids[0] = data[0]; centroids[1] = data[1];  // First sample
    centroids[2] = data[2]; centroids[3] = data[3];  // Second sample
    
    // Configure and run K-Means
    eif_kmeans_config_t kmeans_cfg = {
        .k = 2,
        .max_iterations = 20,
        .epsilon = 0.001f
    };
    eif_kmeans_compute(&kmeans_cfg, data, NUM_SAMPLES, NUM_FEATURES, centroids, labels, &pool);
    
    printf("  Cluster Centers:\n");
    printf("    Cluster 0: (%.1f°C, %.2fg)\n", centroids[0] * 100, centroids[1]);
    printf("    Cluster 1: (%.1f°C, %.2fg)\n", centroids[2] * 100, centroids[3]);
    
    // Identify which cluster is the anomaly cluster (smaller one)
    int cluster_sizes[2] = {0, 0};
    for (int i = 0; i < NUM_SAMPLES; i++) cluster_sizes[labels[i]]++;
    
    int anomaly_cluster = (cluster_sizes[0] < cluster_sizes[1]) ? 0 : 1;
    printf("\n  Cluster %d identified as anomaly cluster (size=%d)\n", 
           anomaly_cluster, cluster_sizes[anomaly_cluster]);
    
    // Count detections
    static int kmeans_predictions[NUM_SAMPLES];
    int kmeans_detected = 0, kmeans_tp = 0, kmeans_fp = 0;
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        kmeans_predictions[i] = (labels[i] == anomaly_cluster);
        if (kmeans_predictions[i]) {
            kmeans_detected++;
            if (samples[i].is_anomaly) kmeans_tp++;
            else kmeans_fp++;
        }
    }
    
    display_scatter(samples, NUM_SAMPLES, kmeans_predictions);
    
    printf("\n  %s┌─ K-Means Results ────────────────────────────┐%s\n", ASCII_CYAN, ASCII_RESET);
    printf("  │  True Anomalies:     %3d                     │\n", true_anomalies);
    printf("  │  Detected Anomalies: %3d                     │\n", kmeans_detected);
    printf("  │  True Positives:     %3d                     │\n", kmeans_tp);
    printf("  │  False Positives:    %3d                     │\n", kmeans_fp);
    printf("  │  Precision:          %5.1f%%                  │\n", 
           kmeans_detected > 0 ? 100.0f * kmeans_tp / kmeans_detected : 0);
    printf("  │  Recall:             %5.1f%%                  │\n", 
           true_anomalies > 0 ? 100.0f * kmeans_tp / true_anomalies : 0);
    printf("  %s└───────────────────────────────────────────────┘%s\n", ASCII_CYAN, ASCII_RESET);
    
    // ========================================================================
    // Summary
    // ========================================================================
    printf("\n");
    ascii_section("Tutorial Summary");
    
    printf("  %sComparison:%s\n\n", ASCII_BOLD, ASCII_RESET);
    
    const char* methods[] = {"Z-Score", "K-Means"};
    float32_t precisions[] = {
        zscore_detected > 0 ? 100.0f * zscore_tp / zscore_detected : 0,
        kmeans_detected > 0 ? 100.0f * kmeans_tp / kmeans_detected : 0
    };
    ascii_bar_chart("Precision (%)", methods, precisions, 2, 30);
    
    printf("\n  %sKey Takeaways:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    • Z-Score is simple but requires normal distribution\n");
    printf("    • K-Means can find complex patterns but needs K selection\n");
    printf("    • Both methods can be computed online (streaming data)\n");
    
    printf("\n  %sEIF APIs Used:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    • eif_kmeans_compute() - K-Means clustering\n");
    printf("    • eif_memory_alloc()   - Memory pool allocation\n");
    
    printf("\n  %sTutorial Complete!%s\n\n", ASCII_GREEN ASCII_BOLD, ASCII_RESET);
    
    return 0;
}
