/**
 * @file main.c
 * @brief Time Series Forecasting Demo - Energy Prediction
 * 
 * Demonstrates time series forecasting for predicting energy consumption
 * using ARIMA and Holt-Winters exponential smoothing.
 * 
 * Features:
 * - ARIMA modeling (Auto-Regressive Integrated Moving Average)
 * - Holt-Winters Exponential Smoothing (seasonal data)
 * - JSON output for visualization with eif_plotter.py
 * 
 * Usage:
 *   ./time_series_demo                       # Interactive tutorial
 *   ./time_series_demo --json                # JSON output for plotter
 *   ./time_series_demo --json | python3 tools/eif_plotter.py --stdin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_ts.h"
#include "../common/ascii_plot.h"

// ============================================================================
// Configuration
// ============================================================================

#define NUM_SAMPLES     72      // 3 days of hourly data
#define TRAIN_SIZE      48      // 2 days for training
#define TEST_SIZE       24      // 1 day for testing
#define SEASON_LENGTH   24      // Daily seasonality

static bool json_mode = false;
static bool continuous_mode = false;
static int sample_count = 0;

// ============================================================================
// Energy Data Generator
// ============================================================================

static void generate_energy_data(float32_t* data, int len, int seed) {
    srand(seed);
    
    for (int i = 0; i < len; i++) {
        int hour = i % 24;
        int day = i / 24;
        
        float32_t energy = 100.0f;
        
        if (hour >= 6 && hour <= 9) {
            energy += 30.0f * (hour - 6) / 3.0f;
        } else if (hour >= 10 && hour <= 17) {
            energy += 30.0f;
        } else if (hour >= 18 && hour <= 21) {
            energy += 40.0f;
        } else if (hour >= 22 || hour <= 5) {
            energy -= 20.0f;
        }
        
        energy += day * 2.0f;
        energy += 10.0f * ((float)rand()/RAND_MAX - 0.5f);
        
        data[i] = energy;
    }
}

// ============================================================================
// JSON Output
// ============================================================================

static void output_json_forecast(int hour, float actual, float arima_pred, float hw_pred) {
    printf("{\"timestamp\": %d, \"type\": \"forecast\"", sample_count++);
    printf(", \"signals\": {\"actual\": %.2f, \"arima\": %.2f, \"holt_winters\": %.2f}", 
           actual, arima_pred, hw_pred);
    printf(", \"errors\": {\"arima_error\": %.2f, \"hw_error\": %.2f}", 
           fabsf(arima_pred - actual), fabsf(hw_pred - actual));
    printf("}\n");
    fflush(stdout);
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON for real-time plotting\n");
    printf("  --continuous  Run without interactive pauses\n");
    printf("  --seed N      Random seed for reproducibility (default: 42)\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
    printf("  %s --continuous\n", prog);
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
    
    // Initialize memory pool
    static uint8_t pool_buffer[64 * 1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Generate data
    static float32_t data[NUM_SAMPLES];
    static float32_t predictions_arima[TEST_SIZE];
    static float32_t predictions_hw[TEST_SIZE];
    
    generate_energy_data(data, NUM_SAMPLES, seed);
    
    if (!json_mode) {
        printf("\n");
        ascii_section("Time Series Forecasting: Energy Prediction");
        
        printf("  Scenario: Smart building energy optimization\n");
        printf("  Data: 72 hours hourly energy (48 train, 24 test)\n\n");
        
        if (!continuous_mode) {
            printf("  Press Enter to continue...");
            getchar();
        }
        
        // Visualize training data
        ascii_section("Step 1: Historical Data");
        printf("  Training data (48 hours):\n");
        ascii_plot_waveform("Energy (kWh)", data, TRAIN_SIZE, 60, 8);
    }
    
    // ========================================================================
    // ARIMA Forecasting
    // ========================================================================
    eif_ts_arima_t arima;
    eif_ts_arima_init(&arima, 2, 0, 1, &pool);
    eif_ts_arima_fit(&arima, data, TRAIN_SIZE);
    
    for (int i = 0; i < TEST_SIZE; i++) {
        float32_t pred;
        eif_ts_arima_predict(&arima, data[TRAIN_SIZE + i - 1], &pred);
        predictions_arima[i] = pred;
    }
    
    float32_t arima_mae = 0;
    for (int i = 0; i < TEST_SIZE; i++) {
        arima_mae += fabsf(predictions_arima[i] - data[TRAIN_SIZE + i]);
    }
    arima_mae /= TEST_SIZE;
    
    if (!json_mode) {
        if (!continuous_mode) {
            printf("\n  Press Enter for ARIMA results...");
            getchar();
        }
        
        ascii_section("Step 2: ARIMA(2,0,1) Forecasting");
        printf("  AR coefficients: phi1=%.3f, phi2=%.3f\n",
               arima.ar_coeffs ? arima.ar_coeffs[0] : 0,
               arima.ar_coeffs ? arima.ar_coeffs[1] : 0);
        printf("  MA coefficient: theta1=%.3f\n\n",
               arima.ma_coeffs ? arima.ma_coeffs[0] : 0);
        
        ascii_plot_waveform("ARIMA Predictions", predictions_arima, TEST_SIZE, 50, 6);
        printf("  MAE: %.2f kWh\n", arima_mae);
    }
    
    // ========================================================================
    // Holt-Winters Forecasting
    // ========================================================================
    eif_ts_hw_t hw;
    eif_ts_hw_init(&hw, SEASON_LENGTH, EIF_TS_HW_ADDITIVE, &pool);
    
    hw.alpha = 0.3f;
    hw.beta = 0.1f;
    hw.gamma = 0.2f;
    
    for (int i = 0; i < TRAIN_SIZE; i++) {
        eif_ts_hw_update(&hw, data[i]);
    }
    
    eif_ts_hw_forecast(&hw, TEST_SIZE, predictions_hw);
    
    float32_t hw_mae = 0;
    for (int i = 0; i < TEST_SIZE; i++) {
        hw_mae += fabsf(predictions_hw[i] - data[TRAIN_SIZE + i]);
    }
    hw_mae /= TEST_SIZE;
    
    if (!json_mode) {
        if (!continuous_mode) {
            printf("\n  Press Enter for Holt-Winters results...");
            getchar();
        }
        
        ascii_section("Step 3: Holt-Winters Forecasting");
        printf("  Parameters: alpha=%.1f, beta=%.1f, gamma=%.1f\n\n",
               hw.alpha, hw.beta, hw.gamma);
        
        ascii_plot_waveform("Holt-Winters Predictions", predictions_hw, TEST_SIZE, 50, 6);
        printf("  MAE: %.2f kWh\n", hw_mae);
    }
    
    // ========================================================================
    // Output
    // ========================================================================
    if (json_mode) {
        // Output each forecast point
        for (int i = 0; i < TEST_SIZE; i++) {
            output_json_forecast(i, data[TRAIN_SIZE + i], predictions_arima[i], predictions_hw[i]);
        }
        
        // Summary
        printf("{\"type\": \"summary\", \"arima_mae\": %.2f, \"hw_mae\": %.2f, \"winner\": \"%s\"}\n",
               arima_mae, hw_mae, hw_mae < arima_mae ? "holt_winters" : "arima");
    } else {
        ascii_section("Summary");
        
        printf("  +-----------------+----------+\n");
        printf("  | Model           | MAE(kWh) |\n");
        printf("  +-----------------+----------+\n");
        printf("  | ARIMA(2,0,1)    |  %6.2f  |\n", arima_mae);
        printf("  | Holt-Winters    |  %6.2f  |\n", hw_mae);
        printf("  +-----------------+----------+\n\n");
        
        if (hw_mae < arima_mae) {
            printf("  Winner: Holt-Winters (captures seasonality better)\n\n");
        } else {
            printf("  Winner: ARIMA\n\n");
        }
    }
    
    return 0;
}
