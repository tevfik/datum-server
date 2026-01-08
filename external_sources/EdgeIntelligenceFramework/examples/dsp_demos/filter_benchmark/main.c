/**
 * @file main.c
 * @brief Filter Performance Benchmark
 *
 * Compares performance of different filter implementations:
 * - FIR vs Biquad
 * - Float vs Fixed-point
 * - Different filter orders
 *
 * Usage:
 *   ./filter_benchmark --batch
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../common/ascii_plot.h"
#include "../../common/demo_cli.h"
#include "eif_dsp_biquad.h"
#include "eif_dsp_biquad_fixed.h"
#include "eif_dsp_fir.h"
#include "eif_dsp_fir_fixed.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_SAMPLES 10000
#define NUM_ITERATIONS 100

static bool json_mode = false;

// Get time in microseconds
static double get_time_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

// Generate test signal
static void generate_test_signal(float *buffer, int16_t *buffer_q15, int len) {
  for (int i = 0; i < len; i++) {
    float x = sinf(2.0f * M_PI * 100.0f * i / 8000.0f) +
              0.5f * sinf(2.0f * M_PI * 500.0f * i / 8000.0f) +
              0.3f * ((float)rand() / RAND_MAX - 0.5f);
    buffer[i] = x;
    buffer_q15[i] = (int16_t)(x * 32767.0f);
  }
}

// Benchmark: FIR float
static double benchmark_fir_float(int order) {
  float coeffs[64];
  for (int i = 0; i < order; i++) {
    coeffs[i] = 1.0f / order; // Simple MA
  }

  eif_fir_t fir;
  eif_fir_init(&fir, coeffs, order);

  float input[NUM_SAMPLES], output[NUM_SAMPLES];
  generate_test_signal(input, NULL, NUM_SAMPLES);

  double start = get_time_us();
  for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
    for (int i = 0; i < NUM_SAMPLES; i++) {
      output[i] = eif_fir_process(&fir, input[i]);
    }
  }
  double end = get_time_us();

  return (end - start) / NUM_ITERATIONS / NUM_SAMPLES * 1000.0; // ns per sample
}

// Benchmark: FIR Q15
static double benchmark_fir_q15(int order) {
  int16_t coeffs[64];
  for (int i = 0; i < order; i++) {
    coeffs[i] = (int16_t)(32768 / order);
  }

  eif_fir_q15_t fir;
  eif_fir_q15_init(&fir, coeffs, order);

  float dummy[NUM_SAMPLES];
  int16_t input[NUM_SAMPLES], output[NUM_SAMPLES];
  generate_test_signal(dummy, input, NUM_SAMPLES);

  double start = get_time_us();
  for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
    for (int i = 0; i < NUM_SAMPLES; i++) {
      output[i] = eif_fir_q15_process(&fir, input[i]);
    }
  }
  double end = get_time_us();

  return (end - start) / NUM_ITERATIONS / NUM_SAMPLES * 1000.0;
}

// Benchmark: Biquad float
static double benchmark_biquad_float(void) {
  eif_biquad_t bq;
  eif_biquad_lowpass(&bq, 1000.0f, 8000.0f, 0.707f);

  float input[NUM_SAMPLES], output[NUM_SAMPLES];
  generate_test_signal(input, NULL, NUM_SAMPLES);

  double start = get_time_us();
  for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
    for (int i = 0; i < NUM_SAMPLES; i++) {
      output[i] = eif_biquad_process(&bq, input[i]);
    }
  }
  double end = get_time_us();

  return (end - start) / NUM_ITERATIONS / NUM_SAMPLES * 1000.0;
}

// Benchmark: Biquad Q15
static double benchmark_biquad_q15(void) {
  eif_biquad_q15_t bq;
  eif_biquad_q15_butter_lp_01(&bq); // Pre-computed

  float dummy[NUM_SAMPLES];
  int16_t input[NUM_SAMPLES], output[NUM_SAMPLES];
  generate_test_signal(dummy, input, NUM_SAMPLES);

  double start = get_time_us();
  for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
    for (int i = 0; i < NUM_SAMPLES; i++) {
      output[i] = eif_biquad_q15_process(&bq, input[i]);
    }
  }
  double end = get_time_us();

  return (end - start) / NUM_ITERATIONS / NUM_SAMPLES * 1000.0;
}

int main(int argc, char **argv) {
  demo_cli_result_t result = demo_parse_args(argc, argv, "filter_benchmark",
                                             "Filter performance comparison");

  if (result == DEMO_EXIT)
    return 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0)
      json_mode = true;
  }

  srand(42);

  if (!json_mode) {
    printf("\n");
    ascii_section("EIF Filter Benchmark");
    printf("  Comparing filter implementations\n\n");
    printf("  Samples: %d | Iterations: %d\n\n", NUM_SAMPLES, NUM_ITERATIONS);
  }

  // Run benchmarks
  if (!json_mode) {
    ascii_section("1. FIR Filter Performance");
    printf("\n  Order   Float (ns/sample)   Q15 (ns/sample)   Speedup\n");
    printf("  -----   -----------------   ---------------   -------\n");
  }

  int orders[] = {8, 16, 32, 64};
  for (int o = 0; o < 4; o++) {
    int order = orders[o];
    double time_float = benchmark_fir_float(order);
    double time_q15 = benchmark_fir_q15(order);

    if (json_mode) {
      printf("{\"filter\": \"fir\", \"order\": %d, \"float_ns\": %.2f, "
             "\"q15_ns\": %.2f}\n",
             order, time_float, time_q15);
    } else {
      printf("  %5d   %17.2f   %15.2f   %6.2fx\n", order, time_float, time_q15,
             time_float / time_q15);
    }
  }

  if (!json_mode) {
    ascii_section("2. Biquad Filter Performance");
    printf("\n");
  }

  double bq_float = benchmark_biquad_float();
  double bq_q15 = benchmark_biquad_q15();

  if (json_mode) {
    printf("{\"filter\": \"biquad\", \"float_ns\": %.2f, \"q15_ns\": %.2f}\n",
           bq_float, bq_q15);
  } else {
    printf("  Float Biquad:  %.2f ns/sample\n", bq_float);
    printf("  Q15 Biquad:    %.2f ns/sample\n", bq_q15);
    printf("  Speedup:       %.2fx\n", bq_float / bq_q15);
  }

  if (!json_mode) {
    ascii_section("3. Memory Usage");
    printf("\n  Filter Type           Memory (bytes)\n");
    printf("  -----------           --------------\n");
    printf("  FIR-16 Float:         %d\n", 16 * 4 * 2 + 8);
    printf("  FIR-16 Q15:           %d\n", eif_fir_q15_memory(16));
    printf("  FIR-32 Float:         %d\n", 32 * 4 * 2 + 8);
    printf("  FIR-32 Q15:           %d\n", eif_fir_q15_memory(32));
    printf("  Biquad Float:         %d\n", (int)sizeof(eif_biquad_t));
    printf("  Biquad Q15:           %d\n", eif_biquad_q15_memory());
    printf("  Biquad 4-stage Q15:   %d\n", eif_biquad_q15_cascade_memory(4));
  }

  if (!json_mode) {
    ascii_section("Summary");
    printf("  • Q15 fixed-point is faster on most MCUs\n");
    printf("  • Biquad is more efficient than high-order FIR\n");
    printf("  • Fixed-point uses half the memory\n");
    printf("  • Choose based on precision vs speed tradeoff\n\n");
  }

  return 0;
}
