/**
 * @file main.c
 * @brief SIMD Performance Benchmark
 *
 * Compares SIMD-optimized vs generic C implementations
 * for common neural network operations.
 */

#include "eif_hal_simd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Benchmark configuration
#define WARMUP_ITERATIONS 10
#define BENCHMARK_ITERATIONS 1000
#define VECTOR_SIZE 512

// Generic (non-SIMD) implementations for comparison
static float generic_dot_f32(const float *a, const float *b, int n) {
  float sum = 0.0f;
  for (int i = 0; i < n; i++) {
    sum += a[i] * b[i];
  }
  return sum;
}

static void generic_relu_f32(float *data, int n) {
  for (int i = 0; i < n; i++) {
    if (data[i] < 0.0f)
      data[i] = 0.0f;
  }
}

static void generic_add_f32(const float *a, const float *b, float *c, int n) {
  for (int i = 0; i < n; i++) {
    c[i] = a[i] + b[i];
  }
}

// Timer helpers
static clock_t start_time;

static void timer_start(void) { start_time = clock(); }

static double timer_stop_us(void) {
  return (double)(clock() - start_time) * 1000000.0 / CLOCKS_PER_SEC;
}

// Print benchmark result
static void print_result(const char *name, double generic_us, double simd_us) {
  double speedup = generic_us / simd_us;
  printf("%-25s %10.2f us  %10.2f us  %6.2fx\n", name, generic_us, simd_us,
         speedup);
}

int main(void) {
  printf("\n=== EIF SIMD Performance Benchmark ===\n\n");
  printf("Platform: %s\n", eif_simd_get_platform());
  printf("Accelerated: %s\n", eif_simd_is_accelerated() ? "YES" : "NO");
  printf("Vector Size: %d elements\n", VECTOR_SIZE);
  printf("Iterations: %d\n\n", BENCHMARK_ITERATIONS);

  // Allocate aligned buffers
  float *a = (float *)malloc(VECTOR_SIZE * sizeof(float));
  float *b = (float *)malloc(VECTOR_SIZE * sizeof(float));
  float *c = (float *)malloc(VECTOR_SIZE * sizeof(float));

  if (!a || !b || !c) {
    printf("Memory allocation failed!\n");
    return 1;
  }

  // Initialize with random data
  srand(42);
  for (int i = 0; i < VECTOR_SIZE; i++) {
    a[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    b[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
  }

  printf("%-25s %12s  %12s  %8s\n", "Operation", "Generic", "SIMD", "Speedup");
  printf("%-25s %12s  %12s  %8s\n", "---------", "-------", "----", "-------");

  volatile float result; // Prevent optimization

  // ==========================================================================
  // Benchmark: Dot Product
  // ==========================================================================

  // Warmup
  for (int i = 0; i < WARMUP_ITERATIONS; i++) {
    result = generic_dot_f32(a, b, VECTOR_SIZE);
    result = eif_simd_dot_f32(a, b, VECTOR_SIZE);
  }

  // Generic
  timer_start();
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    result = generic_dot_f32(a, b, VECTOR_SIZE);
  }
  double dot_generic = timer_stop_us() / BENCHMARK_ITERATIONS;

  // SIMD
  timer_start();
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    result = eif_simd_dot_f32(a, b, VECTOR_SIZE);
  }
  double dot_simd = timer_stop_us() / BENCHMARK_ITERATIONS;

  print_result("Dot Product (512)", dot_generic, dot_simd);

  // ==========================================================================
  // Benchmark: ReLU
  // ==========================================================================

  // Warmup
  memcpy(c, a, VECTOR_SIZE * sizeof(float));
  for (int i = 0; i < WARMUP_ITERATIONS; i++) {
    memcpy(c, a, VECTOR_SIZE * sizeof(float));
    generic_relu_f32(c, VECTOR_SIZE);
  }

  // Generic
  timer_start();
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    memcpy(c, a, VECTOR_SIZE * sizeof(float));
    generic_relu_f32(c, VECTOR_SIZE);
  }
  double relu_generic = timer_stop_us() / BENCHMARK_ITERATIONS;

  // SIMD
  timer_start();
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    memcpy(c, a, VECTOR_SIZE * sizeof(float));
    eif_simd_relu_f32(c, VECTOR_SIZE);
  }
  double relu_simd = timer_stop_us() / BENCHMARK_ITERATIONS;

  print_result("ReLU (512)", relu_generic, relu_simd);

  // ==========================================================================
  // Benchmark: Vector Add
  // ==========================================================================

  // Warmup
  for (int i = 0; i < WARMUP_ITERATIONS; i++) {
    generic_add_f32(a, b, c, VECTOR_SIZE);
    eif_simd_add_f32(a, b, c, VECTOR_SIZE);
  }

  // Generic
  timer_start();
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    generic_add_f32(a, b, c, VECTOR_SIZE);
  }
  double add_generic = timer_stop_us() / BENCHMARK_ITERATIONS;

  // SIMD
  timer_start();
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    eif_simd_add_f32(a, b, c, VECTOR_SIZE);
  }
  double add_simd = timer_stop_us() / BENCHMARK_ITERATIONS;

  print_result("Vector Add (512)", add_generic, add_simd);

  // ==========================================================================
  // Benchmark: Vector Scale
  // ==========================================================================

  // Generic
  timer_start();
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    for (int j = 0; j < VECTOR_SIZE; j++) {
      c[j] = a[j] * 2.5f;
    }
  }
  double scale_generic = timer_stop_us() / BENCHMARK_ITERATIONS;

  // SIMD
  timer_start();
  for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
    eif_simd_scale_f32(a, 2.5f, c, VECTOR_SIZE);
  }
  double scale_simd = timer_stop_us() / BENCHMARK_ITERATIONS;

  print_result("Vector Scale (512)", scale_generic, scale_simd);

  // ==========================================================================
  // Benchmark: Matrix-Vector (64x512)
  // ==========================================================================

  int M = 64;
  int N = VECTOR_SIZE;
  float *matrix = (float *)malloc(M * N * sizeof(float));
  float *y = (float *)malloc(M * sizeof(float));

  if (matrix && y) {
    for (int i = 0; i < M * N; i++) {
      matrix[i] = (float)rand() / RAND_MAX;
    }

    // Generic
    timer_start();
    for (int iter = 0; iter < BENCHMARK_ITERATIONS / 10; iter++) {
      for (int i = 0; i < M; i++) {
        y[i] = generic_dot_f32(matrix + i * N, a, N);
      }
    }
    double matvec_generic = timer_stop_us() / (BENCHMARK_ITERATIONS / 10);

    // SIMD
    timer_start();
    for (int iter = 0; iter < BENCHMARK_ITERATIONS / 10; iter++) {
      eif_simd_matvec_f32(matrix, a, y, M, N);
    }
    double matvec_simd = timer_stop_us() / (BENCHMARK_ITERATIONS / 10);

    print_result("MatVec (64x512)", matvec_generic, matvec_simd);

    free(matrix);
    free(y);
  }

  // ==========================================================================
  // Summary
  // ==========================================================================

  printf("\n=== Summary ===\n");
  double avg_speedup = (dot_generic / dot_simd + relu_generic / relu_simd +
                        add_generic / add_simd + scale_generic / scale_simd) /
                       4.0;
  printf("Average Speedup: %.2fx\n", avg_speedup);

  if (!eif_simd_is_accelerated()) {
    printf("\nNote: Running on generic platform. Compile for ESP32-S3/ARM\n");
    printf("      to see real SIMD acceleration.\n");
  }

  free(a);
  free(b);
  free(c);

  printf("\nBenchmark complete.\n");
  return 0;
}
