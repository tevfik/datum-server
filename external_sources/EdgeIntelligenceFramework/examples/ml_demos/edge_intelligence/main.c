/**
 * @file main.c
 * @brief Edge Intelligence Demo
 *
 * Showcases core edge intelligence features:
 * - Model quantization
 * - Adaptive thresholds
 * - Sensor fusion
 * - Online learning
 *
 * Usage:
 *   ./edge_intelligence_demo --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"
#include "eif_adaptive_threshold.h"
#include "eif_edge_inference.h"
#include "eif_online_learning.h"
#include "eif_quantize.h"
#include "eif_sensor_fusion.h"

static bool json_mode = false;

// Demo: Quantization
static void demo_quantization(void) {
  if (!json_mode) {
    ascii_section("1. Model Quantization");
    printf("  Reducing model size for edge deployment\n\n");
  }

  // Sample weights
  float weights[16] = {0.12f, -0.45f, 0.78f, -0.23f, 0.56f, -0.89f,
                       0.34f, -0.67f, 0.90f, -0.12f, 0.45f, -0.78f,
                       0.23f, -0.56f, 0.89f, -0.34f};

  // Calibrate and quantize
  eif_quant_stats_t stats;
  eif_quant_stats_init(&stats);
  eif_quant_stats_update(&stats, weights, 16);

  eif_quant_params_t params;
  eif_quant_calc_int8_symmetric(&params, stats.min_val, stats.max_val);

  int8_t quantized[16];
  float dequantized[16];

  eif_quant_to_int8_sym(weights, quantized, 16, &params);
  eif_dequant_int8_sym(quantized, dequantized, 16, &params);

  float mse = eif_quant_mse(weights, dequantized, 16);
  float sqnr = eif_quant_sqnr(weights, dequantized, 16);

  if (!json_mode) {
    printf("  Original weights (first 4): [%.2f, %.2f, %.2f, %.2f]\n",
           weights[0], weights[1], weights[2], weights[3]);
    printf("  Quantized INT8:             [%d, %d, %d, %d]\n", quantized[0],
           quantized[1], quantized[2], quantized[3]);
    printf("  Dequantized:                [%.2f, %.2f, %.2f, %.2f]\n",
           dequantized[0], dequantized[1], dequantized[2], dequantized[3]);
    printf("\n  Quality Metrics:\n");
    printf("    Scale: %.6f\n", params.scale);
    printf("    MSE: %.6f\n", mse);
    printf("    SQNR: %.1f dB\n", sqnr);
    printf("    Memory savings: %.1fx\n", eif_quant_memory_ratio(params.type));
  }
}

// Demo: Adaptive thresholds
static void demo_adaptive_thresholds(void) {
  if (!json_mode) {
    ascii_section("2. Adaptive Thresholds");
    printf("  Self-tuning anomaly detection\n\n");
  }

  eif_z_threshold_t zt;
  eif_z_threshold_init(&zt, 0.1f, 2.5f); // alpha=0.1, 2.5 sigma

  // Simulate sensor readings
  float readings[] = {20.1f, 20.3f, 19.8f, 20.5f, 20.2f, // Normal
                      20.0f, 19.9f, 20.4f, 20.1f, 20.3f,
                      35.0f, // Anomaly!
                      20.2f, 20.1f, 19.9f, 20.3f, 20.0f};
  int num_readings = 16;

  if (!json_mode) {
    printf("  Sensor readings with adaptive threshold:\n\n");
    printf("  Time  Value   Status       Threshold Range\n");
    printf("  ----  ------  -----------  ---------------\n");
  }

  for (int i = 0; i < num_readings; i++) {
    bool anomaly = eif_z_threshold_check(&zt, readings[i]);

    float lower, upper;
    eif_z_threshold_get_bounds(&zt, &lower, &upper);

    if (!json_mode) {
      printf("  %3d   %5.1f   %-11s  [%.1f, %.1f]\n", i, readings[i],
             anomaly ? "⚠ ANOMALY" : "Normal", lower, upper);
    }
  }
}

// Demo: Sensor fusion
static void demo_sensor_fusion(void) {
  if (!json_mode) {
    ascii_section("3. Sensor Fusion");
    printf("  Combining multiple sensors for reliability\n\n");
  }

  // Complementary filter example
  eif_complementary_t cf;
  eif_complementary_init(&cf, 0.98f, 0.01f); // alpha=0.98, dt=10ms

  if (!json_mode) {
    printf("  Complementary Filter (IMU fusion):\n");
    printf("    α = 0.98 (trust gyro rate)\n\n");
  }

  // Simulate IMU readings
  float gyro_rate = 10.0f;  // degrees/sec
  float accel_angle = 5.0f; // degrees (from gravity)

  printf("  Time  Gyro Rate  Accel Angle  Fused Output\n");
  printf("  ----  ---------  -----------  ------------\n");

  for (int i = 0; i < 10; i++) {
    float fused =
        eif_complementary_update(&cf, gyro_rate, accel_angle + i * 0.1f);
    if (!json_mode) {
      printf("  %3d   %8.1f   %10.1f   %11.2f\n", i, gyro_rate,
             accel_angle + i * 0.1f, fused);
    }
  }

  // Sensor voting
  if (!json_mode) {
    printf("\n  Fault-Tolerant Voting (3 sensors):\n");
  }

  eif_sensor_voting_t sv;
  eif_voting_init(&sv, 3, 0.5f);

  float sensors[][3] = {
      {25.0f, 25.2f, 25.1f}, // All agree
      {25.1f, 99.9f, 25.0f}, // One faulty
      {25.2f, 25.3f, 25.1f}  // All agree
  };

  for (int i = 0; i < 3; i++) {
    float voted = eif_voting_compute(&sv, sensors[i], 3);
    bool consensus = eif_voting_has_consensus(&sv);

    if (!json_mode) {
      printf("    Sensors: [%.1f, %.1f, %.1f] → Voted: %.1f %s\n",
             sensors[i][0], sensors[i][1], sensors[i][2], voted,
             consensus ? "(consensus)" : "(no consensus)");
    }
  }
}

// Demo: Online learning
static void demo_online_learning(void) {
  if (!json_mode) {
    ascii_section("4. Online Learning");
    printf("  Adapting models on-device\n\n");
  }

  // Online centroid classifier
  eif_online_centroid_t oc;
  eif_online_centroid_init(&oc, 2, 2, 0.0f); // 2 classes, 2 features, pure mean

  // Train with streaming data
  float class0[][2] = {{1.0f, 1.0f}, {1.2f, 0.8f}, {0.9f, 1.1f}};
  float class1[][2] = {{5.0f, 5.0f}, {5.1f, 4.9f}, {4.8f, 5.2f}};

  for (int i = 0; i < 3; i++) {
    eif_online_centroid_update(&oc, class0[i], 0);
    eif_online_centroid_update(&oc, class1[i], 1);
  }

  // Test
  float test_points[][2] = {{1.1f, 0.9f}, {4.9f, 5.1f}, {3.0f, 3.0f}};

  if (!json_mode) {
    printf("  Online Centroid Classifier (trained with 6 samples):\n\n");
    printf("  Test Point      Predicted Class\n");
    printf("  ----------      ---------------\n");
  }

  for (int i = 0; i < 3; i++) {
    int pred = eif_online_centroid_predict(&oc, test_points[i]);
    if (!json_mode) {
      printf("  (%.1f, %.1f)       Class %d\n", test_points[i][0],
             test_points[i][1], pred);
    }
  }

  // Centroids
  if (!json_mode) {
    printf("\n  Learned Centroids:\n");
    printf("    Class 0: (%.2f, %.2f)\n", oc.centroids[0][0],
           oc.centroids[0][1]);
    printf("    Class 1: (%.2f, %.2f)\n", oc.centroids[1][0],
           oc.centroids[1][1]);
  }
}

// Demo: Memory estimation
static void demo_memory_estimation(void) {
  if (!json_mode) {
    ascii_section("5. Model Profiling");
    printf("  Estimating resources for edge deployment\n\n");
  }

  // Create simple model
  eif_model_t model;
  eif_model_init(&model);

  eif_tensor_shape_t input = {{1, 28, 28, 1}, 4};
  eif_tensor_shape_t conv_out = {{1, 26, 26, 16}, 4};
  eif_tensor_shape_t pool_out = {{1, 13, 13, 16}, 4};
  eif_tensor_shape_t flat_out = {{1, 2704, 0, 0}, 2};
  eif_tensor_shape_t dense_out = {{1, 10, 0, 0}, 2};

  int l1 = eif_model_add_layer(&model, EIF_LAYER_CONV2D, input, conv_out);
  model.layers[l1].params.conv2d.kernel_h = 3;
  model.layers[l1].params.conv2d.kernel_w = 3;

  eif_model_add_layer(&model, EIF_LAYER_MAXPOOL2D, conv_out, pool_out);
  eif_model_add_layer(&model, EIF_LAYER_FLATTEN, pool_out, flat_out);
  eif_model_add_layer(&model, EIF_LAYER_DENSE, flat_out, dense_out);

  model.layers[0].weights_size = 3 * 3 * 1 * 16 + 16;
  model.layers[3].weights_size = 2704 * 10 + 10;

  eif_model_profile(&model);

  eif_memory_estimate_t mem;
  eif_estimate_memory(&model, &mem);

  if (!json_mode) {
    printf("  Simple CNN for MNIST:\n");
    printf("    Layers: %d\n", model.num_layers);
    printf("    Total Parameters: %d\n", model.total_params);
    printf("    Total FLOPs: %d\n", model.total_flops);
    printf("\n  Memory Breakdown:\n");
    printf("    Weights: %d bytes (%.1f KB)\n", mem.weights_bytes,
           mem.weights_bytes / 1024.0f);
    printf("    Activations: %d bytes (%.1f KB)\n", mem.activations_bytes,
           mem.activations_bytes / 1024.0f);
    printf("    Scratch: %d bytes (%.1f KB)\n", mem.scratch_bytes,
           mem.scratch_bytes / 1024.0f);
    printf("    TOTAL: %d bytes (%.1f KB)\n", mem.total_bytes,
           mem.total_bytes / 1024.0f);

    printf("\n  Target Compatibility:\n");
    printf("    ESP32 (520KB): %s\n", eif_model_fits_memory(&model, 520 * 1024)
                                          ? "✓ Fits"
                                          : "✗ Too large");
    printf("    ESP32-C3 (400KB): %s\n",
           eif_model_fits_memory(&model, 400 * 1024) ? "✓ Fits"
                                                     : "✗ Too large");
    printf("    Arduino Nano (2KB): %s\n",
           eif_model_fits_memory(&model, 2 * 1024) ? "✓ Fits" : "✗ Too large");
  }
}

int main(int argc, char **argv) {
  demo_cli_result_t result =
      demo_parse_args(argc, argv, "edge_intelligence_demo",
                      "Core edge intelligence features showcase");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Edge Intelligence Demo");
    printf("  Adaptive, intelligent computing at the edge\n\n");
  }

  demo_quantization();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_adaptive_thresholds();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_sensor_fusion();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_online_learning();
  if (!demo_is_batch_mode())
    demo_wait("\n  Press Enter to continue...");

  demo_memory_estimation();

  if (!json_mode) {
    ascii_section("Summary");
    printf("  Edge Intelligence capabilities:\n");
    printf("    • INT8/Q15 quantization for 4x memory savings\n");
    printf("    • Self-tuning adaptive thresholds\n");
    printf("    • Fault-tolerant sensor fusion\n");
    printf("    • On-device online learning\n");
    printf("    • Model profiling for deployment\n\n");
  }

  return 0;
}
