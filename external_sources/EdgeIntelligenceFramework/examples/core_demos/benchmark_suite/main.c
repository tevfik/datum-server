/**
 * @file main.c
 * @brief Comprehensive EIF Benchmark Suite
 *
 * Benchmarks all major EIF components:
 * - DSP filters (FIR, IIR, smoothing)
 * - ML classifiers (SVM, kNN, Decision Tree, etc.)
 * - Memory usage
 *
 * Usage:
 *   ./benchmark_suite --batch
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Core and benchmark
#include "eif_benchmark.h"

// DSP
#include "eif_dsp_biquad.h"
#include "eif_dsp_fir.h"
#include "eif_dsp_fir_fixed.h"
#include "eif_dsp_smooth.h"

// ML
#include "eif_activity.h"
#include "eif_adaptive_threshold.h"
#include "eif_sensor_fusion.h"

#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"

#define BENCH_ITERATIONS 1000
#define WARMUP_ITERATIONS 10

// =============================================================================
// DSP Benchmarks
// =============================================================================

static void benchmark_dsp(void) {
  ascii_section("DSP Benchmarks");

  eif_benchmark_print_header();

  // EMA
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "EMA filter");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_ema_t ema;
    eif_ema_init(&ema, 0.2f);

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      float input = (float)rand() / RAND_MAX;
      eif_benchmark_start(&bench);
      eif_ema_update(&ema, input);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_ema_t);
    eif_benchmark_print_row(&result);
  }

  // Median filter
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "Median filter (7)");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_median_t med;
    eif_median_init(&med, 7);

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      float input = (float)rand() / RAND_MAX;
      eif_benchmark_start(&bench);
      eif_median_update(&med, input);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_median_t);
    eif_benchmark_print_row(&result);
  }

  // FIR float
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "FIR filter (16 taps, float)");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_fir_t fir;
    float coeffs[16];
    // Simple window design
    for (int i = 0; i < 16; i++) {
      coeffs[i] = 1.0f / 16.0f; // Moving average
    }
    eif_fir_init(&fir, coeffs, 16);

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      float input = (float)rand() / RAND_MAX;
      eif_benchmark_start(&bench);
      eif_fir_process(&fir, input);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_fir_t);
    eif_benchmark_print_row(&result);
  }

  // FIR Q15
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "FIR filter (16 taps, Q15)");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_fir_q15_t fir;
    int16_t coeffs[16];
    eif_fir_q15_design_ma(coeffs, 16);
    eif_fir_q15_init(&fir, coeffs, 16);

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      int16_t input = rand() - 16384;
      eif_benchmark_start(&bench);
      eif_fir_q15_process(&fir, input);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_fir_q15_t);
    eif_benchmark_print_row(&result);
  }

  // Biquad float
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "Biquad filter (float)");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_biquad_t bq;
    eif_biquad_lowpass(&bq, 8000.0f, 1000.0f, 0.707f);

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      float input = (float)rand() / RAND_MAX;
      eif_benchmark_start(&bench);
      eif_biquad_process(&bq, input);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_biquad_t);
    eif_benchmark_print_row(&result);
  }
}

// =============================================================================
// ML Benchmarks
// =============================================================================

static void benchmark_ml(void) {
  ascii_section("ML Benchmarks");

  eif_benchmark_print_header();

  // Adaptive threshold
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "Adaptive Z-threshold");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_z_threshold_t thresh;
    eif_z_threshold_init(&thresh, 0.1f, 3.0f);

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      float input = (float)rand() / RAND_MAX;
      eif_benchmark_start(&bench);
      eif_z_threshold_check(&thresh, input);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_z_threshold_t);
    eif_benchmark_print_row(&result);
  }

  // Complementary filter
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "Complementary filter");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_complementary_t cf;
    eif_complementary_init(&cf, 0.98f, 0.01f);

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      float gyro = (float)rand() / RAND_MAX;
      float accel = (float)rand() / RAND_MAX;
      eif_benchmark_start(&bench);
      eif_complementary_update(&cf, gyro, accel);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_complementary_t);
    eif_benchmark_print_row(&result);
  }

  // 1D Kalman
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "1D Kalman filter");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_kalman_1d_t kf;
    eif_kalman_1d_init(&kf, 0.0f, 1.0f, 0.1f, 1.0f);

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      float input = (float)rand() / RAND_MAX;
      eif_benchmark_start(&bench);
      eif_kalman_1d_update(&kf, input);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_kalman_1d_t);
    eif_benchmark_print_row(&result);
  }

  // Activity feature extraction
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "Activity features (128 samples)");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS / 2);

    eif_accel_sample_t samples[128];
    for (int i = 0; i < 128; i++) {
      samples[i].x = (float)rand() / RAND_MAX;
      samples[i].y = (float)rand() / RAND_MAX;
      samples[i].z = 9.8f + (float)rand() / RAND_MAX;
    }

    eif_activity_features_t features;

    for (int i = 0; i < BENCH_ITERATIONS / 10; i++) {
      eif_benchmark_start(&bench);
      eif_activity_extract_features(samples, 128, &features);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes =
        sizeof(eif_activity_features_t) + 128 * sizeof(eif_accel_sample_t);
    eif_benchmark_print_row(&result);
  }

  // Activity classification
  {
    eif_benchmark_t bench;
    eif_benchmark_result_t result;
    eif_benchmark_result_init(&result, "Activity classification");
    eif_benchmark_init(&bench, WARMUP_ITERATIONS);

    eif_activity_features_t features = {.mean_x = 0.1f,
                                        .mean_y = 0.2f,
                                        .mean_z = 9.8f,
                                        .std_x = 0.5f,
                                        .std_y = 0.4f,
                                        .std_z = 0.3f,
                                        .magnitude_mean = 9.85f,
                                        .magnitude_std = 0.6f,
                                        .sma = 10.5f,
                                        .max_magnitude = 11.0f,
                                        .min_magnitude = 9.0f,
                                        .energy = 98.0f,
                                        .zero_crossings = 0.1f,
                                        .peak_frequency = 2.0f};

    for (int i = 0; i < BENCH_ITERATIONS; i++) {
      eif_benchmark_start(&bench);
      eif_activity_classify_rules(&features);
      eif_benchmark_stop(&bench);
    }

    eif_benchmark_stats(&bench, &result);
    result.memory_bytes = sizeof(eif_activity_features_t);
    eif_benchmark_print_row(&result);
  }
}

// =============================================================================
// Memory Usage
// =============================================================================

static void benchmark_memory(void) {
  ascii_section("Memory Usage");

  printf("| %-30s | %10s |\n", "Component", "Size");
  printf("|%s|%s|\n", "--------------------------------", "------------");

  // DSP
  printf("| %-30s | %10zu |\n", "eif_ema_t", sizeof(eif_ema_t));
  printf("| %-30s | %10zu |\n", "eif_median_t", sizeof(eif_median_t));
  printf("| %-30s | %10zu |\n", "eif_fir_t (16 taps)", sizeof(eif_fir_t));
  printf("| %-30s | %10zu |\n", "eif_fir_q15_t (16 taps)",
         sizeof(eif_fir_q15_t));
  printf("| %-30s | %10zu |\n", "eif_biquad_t", sizeof(eif_biquad_t));

  printf("|%s|%s|\n", "--------------------------------", "------------");

  // ML
  printf("| %-30s | %10zu |\n", "eif_z_threshold_t", sizeof(eif_z_threshold_t));
  printf("| %-30s | %10zu |\n", "eif_complementary_t",
         sizeof(eif_complementary_t));
  printf("| %-30s | %10zu |\n", "eif_kalman_1d_t", sizeof(eif_kalman_1d_t));
  printf("| %-30s | %10zu |\n", "eif_activity_window_t",
         sizeof(eif_activity_window_t));
  printf("| %-30s | %10zu |\n", "eif_activity_features_t",
         sizeof(eif_activity_features_t));

  printf("\n");
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char **argv) {
  demo_cli_result_t result = demo_parse_args(argc, argv, "benchmark_suite",
                                             "EIF Performance Benchmarks");

  if (result == DEMO_EXIT)
    return 0;

  srand(42);

  printf("\n");
  ascii_section("EIF Benchmark Suite");
  printf("  Performance benchmarks for all major components\n\n");

  benchmark_dsp();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  benchmark_ml();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  benchmark_memory();

  ascii_section("Summary");
  printf("  All benchmarks completed.\n");
  printf("  Results are for single operations (not block processing).\n");
  printf("  Times measured on host CPU - embedded times will vary.\n\n");

  return 0;
}
