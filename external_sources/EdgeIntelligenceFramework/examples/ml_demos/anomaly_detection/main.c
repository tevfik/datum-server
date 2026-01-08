/**
 * @file main.c
 * @brief Anomaly Detection Demo - Predictive Maintenance
 * 
 * Simulates a predictive maintenance scenario with JSON output support.
 * 
 * Features:
 * - Real-time sensor monitoring with anomaly detection
 * - JSON output for eif_plotter.py integration
 * - Interactive mode with anomaly injection
 * - Multiple detection methods: Statistical, Isolation Forest, EWMA, Ensemble
 * 
 * Usage:
 *   ./anomaly_detection_demo                    # Interactive mode
 *   ./anomaly_detection_demo --json             # JSON output for plotter
 *   ./anomaly_detection_demo --json | python3 tools/eif_plotter.py --stdin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "eif_anomaly.h"
#include "eif_memory.h"

// Memory pool (512KB needed for Isolation Forest trees)
static uint8_t pool_buffer[512 * 1024];
static eif_memory_pool_t pool;

// Configuration
#define NUM_FEATURES 3
#define NORMAL_SAMPLES 100
#define TEST_SAMPLES 50

static bool json_mode = false;
static bool continuous_mode = false;
static int sample_count = 0;

static const char* feature_names[] = {"temperature", "vibration", "current"};
static const char* feature_units[] = {"C", "g", "A"};

// ============================================================================
// Data Generation
// ============================================================================

static void generate_normal_data(float32_t* data, int count) {
    for (int i = 0; i < count; i++) {
        data[i * NUM_FEATURES + 0] = 60.0f + ((float)rand() / RAND_MAX - 0.5f) * 10.0f;
        data[i * NUM_FEATURES + 1] = 0.2f + ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        data[i * NUM_FEATURES + 2] = 6.0f + ((float)rand() / RAND_MAX - 0.5f) * 1.0f;
    }
}

static void generate_test_data(float32_t* data, int count, int* anomaly_indices, int* num_anomalies) {
    *num_anomalies = 0;
    
    for (int i = 0; i < count; i++) {
        bool is_anomaly = (rand() % 10 == 0);
        
        if (is_anomaly) {
            anomaly_indices[(*num_anomalies)++] = i;
            int anomaly_type = rand() % 3;
            switch (anomaly_type) {
                case 0:  // High temperature
                    data[i * NUM_FEATURES + 0] = 85.0f + ((float)rand() / RAND_MAX) * 15.0f;
                    data[i * NUM_FEATURES + 1] = 0.2f + ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
                    data[i * NUM_FEATURES + 2] = 6.0f + ((float)rand() / RAND_MAX - 0.5f) * 1.0f;
                    break;
                case 1:  // High vibration
                    data[i * NUM_FEATURES + 0] = 60.0f + ((float)rand() / RAND_MAX - 0.5f) * 10.0f;
                    data[i * NUM_FEATURES + 1] = 0.6f + ((float)rand() / RAND_MAX) * 0.4f;
                    data[i * NUM_FEATURES + 2] = 6.0f + ((float)rand() / RAND_MAX - 0.5f) * 1.0f;
                    break;
                case 2:  // Current spike
                    data[i * NUM_FEATURES + 0] = 60.0f + ((float)rand() / RAND_MAX - 0.5f) * 10.0f;
                    data[i * NUM_FEATURES + 1] = 0.2f + ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
                    data[i * NUM_FEATURES + 2] = 12.0f + ((float)rand() / RAND_MAX) * 5.0f;
                    break;
            }
        } else {
            data[i * NUM_FEATURES + 0] = 60.0f + ((float)rand() / RAND_MAX - 0.5f) * 10.0f;
            data[i * NUM_FEATURES + 1] = 0.2f + ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            data[i * NUM_FEATURES + 2] = 6.0f + ((float)rand() / RAND_MAX - 0.5f) * 1.0f;
        }
    }
}

// ============================================================================
// JSON Output
// ============================================================================

static void output_json(int sample_idx, float32_t* features, float score, bool is_anomaly) {
    printf("{\"timestamp\": %d, \"type\": \"sensor\"", sample_idx);
    
    // Sensor values
    printf(", \"signals\": {");
    for (int i = 0; i < NUM_FEATURES; i++) {
        printf("\"%s\": %.4f%s", feature_names[i], features[i], i < NUM_FEATURES-1 ? ", " : "");
    }
    printf("}");
    
    // Anomaly info
    printf(", \"state\": {\"anomaly_score\": %.4f}", score);
    printf(", \"prediction\": \"%s\"", is_anomaly ? "ANOMALY" : "normal");
    
    printf("}\n");
    fflush(stdout);
}

// ============================================================================
// Display Functions
// ============================================================================

static void print_header(void) {
    printf("========================================\n");
    printf("Anomaly Detection Demo\n");
    printf("Predictive Maintenance Scenario\n");
    printf("========================================\n\n");
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON for real-time plotting\n");
    printf("  --continuous  Run all demos without pauses\n");
    printf("  --seed N      Random seed for reproducible results\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
    printf("  %s --continuous\n", prog);
}

static void print_sample_table_header(void) {
    printf("  %5s  %6s %6s %6s  %6s  %s\n", "Idx", "Temp", "Vib", "Curr", "Score", "Status");
    printf("  %5s  %6s %6s %6s  %6s  %s\n", "-----", "------", "------", "------", "------", "--------");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    int seed = 42;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--continuous") == 0) {
            continuous_mode = true;
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    srand(seed);
    
    if (!json_mode) {
        print_header();
    }
    
    // Initialize memory pool
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Allocate data arrays
    float32_t* train_data = (float32_t*)eif_memory_alloc(&pool, 
        NORMAL_SAMPLES * NUM_FEATURES * sizeof(float32_t), sizeof(float32_t));
    float32_t* test_data = (float32_t*)eif_memory_alloc(&pool,
        TEST_SAMPLES * NUM_FEATURES * sizeof(float32_t), sizeof(float32_t));
    int anomaly_indices[TEST_SAMPLES];
    int num_true_anomalies;
    
    if (!json_mode) {
        printf("Generating sensor data...\n");
        printf("  Features: Temperature (C), Vibration (g), Current (A)\n");
        printf("  Training samples: %d (normal operation)\n", NORMAL_SAMPLES);
        printf("  Test samples: %d (with injected anomalies)\n\n", TEST_SAMPLES);
    }
    
    generate_normal_data(train_data, NORMAL_SAMPLES);
    generate_test_data(test_data, TEST_SAMPLES, anomaly_indices, &num_true_anomalies);
    
    if (!json_mode) {
        printf("True anomalies injected: %d\n\n", num_true_anomalies);
    }
    
    // ========================================================================
    // Demo 1: Statistical Detector
    // ========================================================================
    if (!json_mode) {
        printf("=== Demo 1: Statistical Detector (Z-score) ===\n");
    }
    
    eif_stat_detector_t stat_det[NUM_FEATURES];
    for (int i = 0; i < NUM_FEATURES; i++) {
        eif_stat_detector_init(&stat_det[i], 50, 3.0f, &pool);
    }
    
    // Train on normal data
    for (int i = 0; i < NORMAL_SAMPLES; i++) {
        for (int j = 0; j < NUM_FEATURES; j++) {
            eif_stat_detector_update(&stat_det[j], train_data[i * NUM_FEATURES + j]);
        }
    }
    
    // Test
    int stat_detected = 0;
    for (int i = 0; i < TEST_SAMPLES; i++) {
        bool is_anomaly = false;
        for (int j = 0; j < NUM_FEATURES; j++) {
            if (eif_stat_detector_is_anomaly(&stat_det[j], test_data[i * NUM_FEATURES + j])) {
                is_anomaly = true;
            }
        }
        if (is_anomaly) stat_detected++;
    }
    
    if (!json_mode) {
        printf("  Detected: %d / %d true anomalies\n\n", stat_detected, num_true_anomalies);
    }
    
    // ========================================================================
    // Demo 2: Multivariate Detector (Isolation Forest)
    // ========================================================================
    if (!json_mode) {
        printf("=== Demo 2: Multivariate Detector (Isolation Forest) ===\n");
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    generate_normal_data(train_data, NORMAL_SAMPLES);
    generate_test_data(test_data, TEST_SAMPLES, anomaly_indices, &num_true_anomalies);
    
    eif_mv_detector_t mv_det;
    eif_mv_detector_init(&mv_det, NUM_FEATURES, 10, 0.6f, &pool);
    eif_mv_detector_fit(&mv_det, train_data, NORMAL_SAMPLES);
    
    int mv_detected = 0;
    for (int i = 0; i < TEST_SAMPLES; i++) {
        if (eif_mv_detector_is_anomaly(&mv_det, &test_data[i * NUM_FEATURES])) {
            mv_detected++;
        }
    }
    
    if (!json_mode) {
        printf("  Detected: %d / %d true anomalies\n\n", mv_detected, num_true_anomalies);
    }
    
    // ========================================================================
    // Demo 3: Time Series Detector (EWMA)
    // ========================================================================
    if (!json_mode) {
        printf("=== Demo 3: Time Series Detector (EWMA) ===\n");
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    generate_normal_data(train_data, NORMAL_SAMPLES);
    generate_test_data(test_data, TEST_SAMPLES, anomaly_indices, &num_true_anomalies);
    
    eif_ts_detector_t ts_det[NUM_FEATURES];
    for (int i = 0; i < NUM_FEATURES; i++) {
        eif_ts_detector_init(&ts_det[i], 0.1f, 3.0f, &pool);
    }
    
    // Train
    for (int i = 0; i < NORMAL_SAMPLES; i++) {
        for (int j = 0; j < NUM_FEATURES; j++) {
            eif_ts_detector_update(&ts_det[j], train_data[i * NUM_FEATURES + j]);
        }
    }
    
    // Test
    int ts_detected = 0;
    for (int i = 0; i < TEST_SAMPLES; i++) {
        float32_t max_score = 0.0f;
        for (int j = 0; j < NUM_FEATURES; j++) {
            float32_t score = eif_ts_detector_update(&ts_det[j], test_data[i * NUM_FEATURES + j]);
            if (score > max_score) max_score = score;
        }
        if (max_score > 0.8f) ts_detected++;
    }
    
    if (!json_mode) {
        printf("  Detected: %d / %d true anomalies\n\n", ts_detected, num_true_anomalies);
    }
    
    // ========================================================================
    // Demo 4: Ensemble Detector (with JSON output)
    // ========================================================================
    if (!json_mode) {
        printf("=== Demo 4: Ensemble Detector (Combined) ===\n");
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    generate_normal_data(train_data, NORMAL_SAMPLES);
    generate_test_data(test_data, TEST_SAMPLES, anomaly_indices, &num_true_anomalies);
    
    eif_ensemble_detector_t ensemble;
    eif_ensemble_init(&ensemble, NUM_FEATURES, 50, &pool);
    eif_ensemble_fit(&ensemble, train_data, NORMAL_SAMPLES);
    
    int ensemble_detected = 0;
    
    if (!json_mode) {
        printf("\n  Sample-by-sample scores:\n");
        print_sample_table_header();
    }
    
    for (int i = 0; i < TEST_SAMPLES; i++) {
        float32_t* sample = &test_data[i * NUM_FEATURES];
        float32_t score = eif_ensemble_score(&ensemble, sample);
        bool detected = score > 0.5f;
        
        if (detected) ensemble_detected++;
        
        if (json_mode) {
            output_json(sample_count++, sample, score, detected);
        } else {
            // Print first 10 and any anomalies
            if (i < 10 || detected) {
                printf("  %5d  %6.1f %6.2f %6.1f  %6.2f  %s\n",
                       i, sample[0], sample[1], sample[2], score,
                       detected ? "ANOMALY!" : "normal");
            } else if (i == 10) {
                printf("  ... (showing only first 10 and detected anomalies)\n");
            }
        }
    }
    
    // Summary
    if (!json_mode) {
        printf("\n  Ensemble detected: %d / %d true anomalies\n", ensemble_detected, num_true_anomalies);
        
        printf("\n========================================\n");
        printf("Summary\n");
        printf("========================================\n");
        printf("  Method           Detected\n");
        printf("  ---------------  --------\n");
        printf("  Statistical      %d\n", stat_detected);
        printf("  Isolation Forest %d\n", mv_detected);
        printf("  Time Series      %d\n", ts_detected);
        printf("  Ensemble         %d\n", ensemble_detected);
        printf("  True Anomalies   %d\n", num_true_anomalies);
        printf("\n");
    } else {
        // Final summary in JSON
        printf("{\"type\": \"summary\", \"stat_detected\": %d, \"mv_detected\": %d, "
               "\"ts_detected\": %d, \"ensemble_detected\": %d, \"true_anomalies\": %d}\n",
               stat_detected, mv_detected, ts_detected, ensemble_detected, num_true_anomalies);
    }
    
    return 0;
}
